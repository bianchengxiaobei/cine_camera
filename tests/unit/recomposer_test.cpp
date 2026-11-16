#include "fake_entity_world.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/recomposer.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct RecomposerFixture
{
    RecomposerFixture()
    {
        world.add(cameraEntity, Transform{{0.0f, 2.0f, 9.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("recomposer");
        camera = system.camera(id);
        camera->ownerEntity = cameraEntity;
        auto extension = std::make_unique<Recomposer>();
        recomposer = extension.get();
        camera->addExtension(std::move(extension));
    }

    void step(float deltaTime = 1.0f / 60.0f)
    {
        time += deltaTime;
        world.setTime(time);
        camera->updateCameraState(math::Vec3::up, deltaTime);
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    Recomposer* recomposer = nullptr;
    EntityId cameraEntity = 1;
    float time = 0.0f;
};

} // namespace

TEST_CASE("Recomposer exposes the original defaults and validates ranges", "[unit][recomposer]")
{
    Recomposer recomposer;
    REQUIRE(recomposer.applyAfter == Stage::Finalize);
    REQUIRE(recomposer.tilt == 0.0f);
    REQUIRE(recomposer.pan == 0.0f);
    REQUIRE(recomposer.dutch == 0.0f);
    REQUIRE(recomposer.zoomScale == 1.0f);
    REQUIRE(recomposer.followAttachment == 1.0f);
    REQUIRE(recomposer.lookAtAttachment == 1.0f);

    recomposer.zoomScale = 0.0f;
    recomposer.followAttachment = -0.5f;
    recomposer.lookAtAttachment = 2.0f;
    recomposer.validate();
    REQUIRE(recomposer.zoomScale == 0.01f);
    REQUIRE(recomposer.followAttachment == 0.0f);
    REQUIRE(recomposer.lookAtAttachment == 1.0f);
}

TEST_CASE("Recomposer writes target attachments before the pipeline", "[unit][recomposer]")
{
    RecomposerFixture fixture;
    fixture.recomposer->followAttachment = 0.35f;
    fixture.recomposer->lookAtAttachment = 0.65f;
    fixture.step();

    REQUIRE(fixture.camera->followTargetAttachment == 0.35f);
    REQUIRE(fixture.camera->lookAtTargetAttachment == 0.65f);
}

TEST_CASE("Recomposer applies tilt, pan, dutch, and zoom at the selected stage", "[unit][recomposer]")
{
    RecomposerFixture fixture;
    fixture.recomposer->tilt = 6.0f;
    fixture.recomposer->pan = 12.0f;
    fixture.recomposer->dutch = 5.0f;
    fixture.recomposer->zoomScale = 1.5f;

    CameraState state = CameraState::makeDefault();
    state.rawOrientation = math::quatFromAxisAngle(
        math::Vec3::up,
        30.0f * math::DegToRad);
    state.lens.fieldOfView = 50.0f;
    state.lens.orthographicSize = 8.0f;
    state.lens.dutch = 2.0f;

    fixture.recomposer->postPipelineStageCallback(*fixture.camera, Stage::Aim, state, 0.0f);
    REQUIRE(math::approximately(state.orientationCorrection, math::Quat::identity()));
    REQUIRE(state.lens.fieldOfView == 50.0f);

    fixture.recomposer->postPipelineStageCallback(*fixture.camera, Stage::Finalize, state, 0.0f);

    const math::Quat tilted = state.rawOrientation
        * math::quatFromAxisAngle(math::Vec3::right, 6.0f * math::DegToRad);
    const math::Quat desired = math::quatFromAxisAngle(
        state.referenceUp,
        12.0f * math::DegToRad)
        * tilted;
    REQUIRE(math::angle(state.correctedOrientation(), desired) < 1e-4f);
    REQUIRE(math::approximately(state.rawOrientation, math::quatFromAxisAngle(
        math::Vec3::up,
        30.0f * math::DegToRad), 1e-4f));
    REQUIRE(math::approximately(state.lens.dutch, 7.0f, 1e-4f));
    REQUIRE(math::approximately(state.lens.fieldOfView, 75.0f, 1e-4f));
    REQUIRE(math::approximately(state.lens.orthographicSize, 12.0f, 1e-4f));
}

TEST_CASE("Recomposer honors apply-after and leaves the lens alone at unit zoom", "[unit][recomposer]")
{
    RecomposerFixture fixture;
    fixture.recomposer->applyAfter = Stage::Aim;
    fixture.recomposer->tilt = 4.0f;
    fixture.recomposer->dutch = 3.0f;

    CameraState state = CameraState::makeDefault();
    state.lens.fieldOfView = 42.0f;
    state.lens.orthographicSize = 9.0f;
    fixture.recomposer->postPipelineStageCallback(*fixture.camera, Stage::Body, state, 0.0f);
    REQUIRE(math::approximately(state.orientationCorrection, math::Quat::identity()));
    REQUIRE(state.lens.fieldOfView == 42.0f);
    REQUIRE(state.lens.dutch == 0.0f);

    fixture.recomposer->postPipelineStageCallback(*fixture.camera, Stage::Aim, state, 0.0f);
    REQUIRE_FALSE(math::approximately(state.orientationCorrection, math::Quat::identity()));
    REQUIRE(state.lens.fieldOfView == 42.0f);
    REQUIRE(state.lens.orthographicSize == 9.0f);
    REQUIRE(math::approximately(state.lens.dutch, 3.0f, 1e-4f));
}
