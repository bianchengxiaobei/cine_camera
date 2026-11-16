#include "fake_entity_world.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/free_look_modifier.hpp>
#include <cinerig/components/orbital_follow.hpp>
#include <cinerig/components/pan_tilt.hpp>
#include <cinerig/components/rotation_composer.hpp>
#include <cinerig/components/third_person_follow.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct FreeLookFixture
{
    FreeLookFixture()
    {
        world.add(target, Transform{{0.0f, 1.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("free look");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        orbital = &camera->setBody<OrbitalFollow>();
        composer = &camera->setAim<RotationComposer>();
        auto extension = std::make_unique<FreeLookModifier>();
        freeLook = extension.get();
        camera->addExtension(std::move(extension));
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    OrbitalFollow* orbital = nullptr;
    RotationComposer* composer = nullptr;
    FreeLookModifier* freeLook = nullptr;
    EntityId target = 1;
};

struct FakeNoise : CameraComponent, IModifiableNoise
{
    std::pair<float, float> value{1.0f, 2.0f};

    Stage stage() const override { return Stage::Noise; }
    bool isValid() const override { return true; }
    void mutateCameraState(CameraState& state, float deltaTime) override
    {
        (void)state;
        (void)deltaTime;
    }

    std::pair<float, float> getNoiseAmplitudeFrequency() const override { return value; }
    void setNoiseAmplitudeFrequency(std::pair<float, float> newValue) override { value = newValue; }
};

} // namespace

TEST_CASE("FreeLookModifier value sources expose normalized axis and rig values", "[unit][free-look-modifier]")
{
    FreeLookFixture fixture;
    fixture.orbital->verticalAxis.range = {-1.0f, 1.0f};
    fixture.orbital->verticalAxis.value = 0.5f;
    REQUIRE(math::approximately(
        fixture.orbital->getNormalizedModifierValue(),
        0.5f,
        1e-4f));
    fixture.orbital->verticalAxis.value = -1.0f;
    REQUIRE(math::approximately(
        fixture.orbital->getNormalizedModifierValue(),
        -1.0f,
        1e-4f));

    fixture.orbital->setDistance(7.0f);
    fixture.orbital->setPositionDamping({1.0f, 2.0f, 3.0f});
    REQUIRE(fixture.orbital->getDistance() == 7.0f);
    REQUIRE(math::approximately(
        fixture.orbital->getPositionDamping(),
        {1.0f, 2.0f, 3.0f}));

    PanTilt panTilt;
    panTilt.tiltAxis.range = {-70.0f, 70.0f};
    panTilt.tiltAxis.value = 35.0f;
    REQUIRE(math::approximately(panTilt.getNormalizedModifierValue(), 0.5f, 1e-4f));

    auto& thirdPerson = fixture.camera->setBody<ThirdPersonFollow>();
    fixture.world.set(fixture.target, Transform{
        {0.0f, 1.0f, 0.0f},
        math::quatFromAxisAngle(math::Vec3::right, 30.0f * math::DegToRad),
        math::Vec3::one});
    const math::Quat targetRotation = fixture.world.transform(fixture.target).rotation;
    const math::Vec3 targetUp = math::rotate(targetRotation, math::Vec3::up);
    const math::Vec3 targetRight = math::rotate(targetRotation, math::Vec3::right);
    const float expected = math::clamp(
        math::signedAngle(targetUp, math::Vec3::up, targetRight),
        -math::Pi * 0.5f,
        math::Pi * 0.5f) / (-math::Pi * 0.5f);
    REQUIRE(math::approximately(
        thirdPerson.getNormalizedModifierValue(),
        expected,
        1e-4f));
}

TEST_CASE("FreeLookModifier applies and restores top/center/bottom modifiers", "[unit][free-look-modifier]")
{
    FreeLookFixture fixture;
    auto distance = std::make_unique<FreeLookModifier::DistanceModifier>();
    auto composition = std::make_unique<FreeLookModifier::CompositionModifier>();
    auto lens = std::make_unique<FreeLookModifier::LensModifier>();
    auto tilt = std::make_unique<FreeLookModifier::TiltModifier>();
    auto* distanceModifier = distance.get();
    auto* compositionModifier = composition.get();
    auto* lensModifier = lens.get();
    auto* tiltModifier = tilt.get();
    fixture.freeLook->modifiers.push_back(std::move(distance));
    fixture.freeLook->modifiers.push_back(std::move(composition));
    fixture.freeLook->modifiers.push_back(std::move(lens));
    fixture.freeLook->modifiers.push_back(std::move(tilt));
    fixture.freeLook->easing = 1.0f;
    fixture.freeLook->refreshCache(*fixture.camera);
    fixture.freeLook->reset(*fixture.camera);

    fixture.orbital->verticalAxis.range = {-1.0f, 1.0f};
    fixture.orbital->verticalAxis.value = 0.5f;
    distanceModifier->distance = {20.0f, 4.0f};
    compositionModifier->composition.top.screenPosition = {0.0f, 0.4f};
    compositionModifier->composition.bottom.screenPosition = {0.0f, -0.4f};
    lensModifier->lens.top.fieldOfView = 70.0f;
    lensModifier->lens.bottom.fieldOfView = 30.0f;
    tiltModifier->tilt = {10.0f, -10.0f};

    CameraState state = CameraState::makeDefault();
    state.lens.fieldOfView = 55.0f;
    fixture.freeLook->prePipelineMutateCameraStateCallback(
        *fixture.camera,
        state,
        1.0f / 60.0f);

    REQUIRE(math::approximately(fixture.freeLook->currentValue(), 0.125f, 1e-4f));
    REQUIRE(math::approximately(fixture.orbital->getDistance(), 11.25f, 1e-4f));
    REQUIRE(math::approximately(
        fixture.composer->composition.screenPosition.y,
        0.05f,
        1e-4f));
    REQUIRE(math::approximately(state.lens.fieldOfView, 56.875f, 1e-4f));

    fixture.freeLook->postPipelineStageCallback(
        *fixture.camera,
        Stage::Finalize,
        state,
        1.0f / 60.0f);

    REQUIRE(math::approximately(fixture.orbital->getDistance(), 10.0f, 1e-4f));
    REQUIRE(math::approximately(
        fixture.composer->composition.screenPosition.y,
        0.0f,
        1e-4f));
    REQUIRE(math::approximately(state.lens.fieldOfView, 56.875f, 1e-4f));
    REQUIRE(math::angle(
        state.orientationCorrection,
        math::quatFromAxisAngle(math::Vec3::right, 1.25f * math::DegToRad)) < 1e-4f);
}

TEST_CASE("FreeLookModifier handles bottom values and restores noise settings", "[unit][free-look-modifier]")
{
    FreeLookFixture fixture;
    auto& noiseComponent = fixture.camera->setNoise<FakeNoise>();
    auto distance = std::make_unique<FreeLookModifier::DistanceModifier>();
    auto noise = std::make_unique<FreeLookModifier::NoiseModifier>();
    auto* distanceModifier = distance.get();
    auto* noiseModifier = noise.get();
    fixture.freeLook->modifiers.push_back(std::move(distance));
    fixture.freeLook->modifiers.push_back(std::move(noise));
    fixture.freeLook->easing = 0.0f;
    fixture.freeLook->refreshCache(*fixture.camera);
    fixture.freeLook->reset(*fixture.camera);

    fixture.orbital->verticalAxis.range = {-1.0f, 1.0f};
    fixture.orbital->verticalAxis.value = -1.0f;
    distanceModifier->distance = {20.0f, 4.0f};
    noiseModifier->noise.top = {2.0f, 4.0f};
    noiseModifier->noise.bottom = {0.5f, 1.0f};

    CameraState state = CameraState::makeDefault();
    fixture.freeLook->prePipelineMutateCameraStateCallback(
        *fixture.camera,
        state,
        1.0f / 60.0f);
    REQUIRE(math::approximately(fixture.freeLook->currentValue(), -1.0f, 1e-4f));
    REQUIRE(math::approximately(fixture.orbital->getDistance(), 4.0f, 1e-4f));
    REQUIRE(math::approximately(noiseComponent.value.first, 0.5f, 1e-4f));
    REQUIRE(math::approximately(noiseComponent.value.second, 1.0f, 1e-4f));

    fixture.freeLook->postPipelineStageCallback(
        *fixture.camera,
        Stage::Finalize,
        state,
        1.0f / 60.0f);
    REQUIRE(math::approximately(fixture.orbital->getDistance(), 10.0f, 1e-4f));
    REQUIRE(math::approximately(noiseComponent.value.first, 1.0f, 1e-4f));
    REQUIRE(math::approximately(noiseComponent.value.second, 2.0f, 1e-4f));
}

TEST_CASE("FreeLookModifier validates all built-in modifier settings", "[unit][free-look-modifier]")
{
    FreeLookModifier freeLook;
    auto tilt = std::make_unique<FreeLookModifier::TiltModifier>();
    auto distance = std::make_unique<FreeLookModifier::DistanceModifier>();
    auto damping = std::make_unique<FreeLookModifier::PositionDampingModifier>();
    auto composition = std::make_unique<FreeLookModifier::CompositionModifier>();
    auto lens = std::make_unique<FreeLookModifier::LensModifier>();
    auto* tiltModifier = tilt.get();
    auto* distanceModifier = distance.get();
    auto* dampingModifier = damping.get();
    auto* compositionModifier = composition.get();
    auto* lensModifier = lens.get();
    freeLook.modifiers.push_back(std::move(tilt));
    freeLook.modifiers.push_back(std::move(distance));
    freeLook.modifiers.push_back(std::move(damping));
    freeLook.modifiers.push_back(std::move(composition));
    freeLook.modifiers.push_back(std::move(lens));

    freeLook.easing = 2.0f;
    tiltModifier->tilt = {40.0f, -40.0f};
    distanceModifier->distance = {-2.0f, 3.0f};
    dampingModifier->damping = {{-1.0f, 2.0f, -3.0f}, {1.0f, -2.0f, 3.0f}};
    compositionModifier->composition.top.screenPosition = {3.0f, -4.0f};
    lensModifier->lens.top.fieldOfView = 200.0f;
    freeLook.validate();

    REQUIRE(freeLook.easing == 1.0f);
    REQUIRE(tiltModifier->tilt.top == 30.0f);
    REQUIRE(tiltModifier->tilt.bottom == -30.0f);
    REQUIRE(distanceModifier->distance.top == 0.0f);
    REQUIRE(distanceModifier->distance.bottom == 3.0f);
    REQUIRE(math::approximately(
        dampingModifier->damping.top,
        {0.0f, 2.0f, 0.0f}));
    REQUIRE(math::approximately(
        dampingModifier->damping.bottom,
        {1.0f, 0.0f, 3.0f}));
    REQUIRE(compositionModifier->composition.top.screenPosition.x == 1.5f);
    REQUIRE(compositionModifier->composition.top.screenPosition.y == -1.5f);
    REQUIRE(lensModifier->lens.top.fieldOfView == 179.0f);
}
