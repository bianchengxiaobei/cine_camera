#include "fake_entity_world.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/spline_dolly_look_at_targets.hpp>
#include <cinerig/core/bezier_spline.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BezierSpline makeTrack()
{
    return BezierSpline::linear(
        {
            {7.0f, 1.0f, -6.0f},
            {13.0f, 1.0f, -6.0f},
            {13.0f, 1.0f, 1.0f},
            {7.0f, 1.0f, 1.0f},
        },
        true);
}

float forwardDot(const CameraState& state, math::Vec3 direction)
{
    return math::dot(
        math::rotate(state.rawOrientation, math::Vec3::forward),
        math::normalize(direction));
}

struct LookAtFixture
{
    LookAtFixture()
    {
        const CameraId id = system.createCamera<VirtualCamera>("look at");
        camera = system.camera(id);
        dolly = &camera->setBody<SplineDolly>();
        dolly->spline.spline = &track;
        dolly->spline.units = PathIndexUnit::Normalized;
        dolly->spline.position = 0.0f;
        dolly->damping.enabled = false;
        lookAt = &camera->setAim<SplineDollyLookAtTargets>();
    }

    CameraState update(float deltaTime = 0.0f)
    {
        camera->updateCameraState(math::Vec3::up, deltaTime);
        return camera->state();
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    BezierSpline track = makeTrack();
    VirtualCamera* camera = nullptr;
    SplineDolly* dolly = nullptr;
    SplineDollyLookAtTargets* lookAt = nullptr;
};

} // namespace

TEST_CASE("SplineDollyLookAtTargets defaults, reset, and validation", "[unit][spline-look-at]")
{
    SplineDollyLookAtTargets lookAt;
    REQUIRE(lookAt.stage() == Stage::Aim);
    REQUIRE_FALSE(lookAt.isValid());
    REQUIRE(lookAt.targets.empty());
    REQUIRE(lookAt.defaultItem.easing == Catch::Approx(1.0f));

    lookAt.targets = {
        {1.5f, {0, {2.0f, 0.0f, 0.0f}, 2.0f}},
        {-0.5f, {0, {1.0f, 0.0f, 0.0f}, -1.0f}},
        {0.5f, {0, {3.0f, 0.0f, 0.0f}, 0.5f}},
    };
    lookAt.defaultItem.easing = 2.0f;
    lookAt.validate();
    REQUIRE(lookAt.defaultItem.easing == Catch::Approx(1.0f));
    REQUIRE(lookAt.targets.size() == 3);
    REQUIRE(lookAt.targets[0].position == Catch::Approx(0.0f));
    REQUIRE(lookAt.targets[0].item.easing == Catch::Approx(0.0f));
    REQUIRE(lookAt.targets[1].position == Catch::Approx(0.5f));
    REQUIRE(lookAt.targets[2].position == Catch::Approx(1.0f));
    REQUIRE(lookAt.targets[2].item.easing == Catch::Approx(1.0f));

    lookAt.reset();
    REQUIRE(lookAt.targets.empty());
    REQUIRE(lookAt.defaultItem.easing == Catch::Approx(1.0f));

    LookAtFixture fixture;
    REQUIRE(fixture.lookAt->isValid());
    fixture.dolly->spline.spline = nullptr;
    REQUIRE_FALSE(fixture.lookAt->isValid());
    fixture.dolly->spline.spline = &fixture.track;

    const CameraId noBodyId =
        fixture.system.createCamera<VirtualCamera>("no body");
    VirtualCamera* noBody = fixture.system.camera(noBodyId);
    SplineDollyLookAtTargets& noBodyLookAt =
        noBody->setAim<SplineDollyLookAtTargets>();
    REQUIRE_FALSE(noBodyLookAt.isValid());
}

TEST_CASE("SplineDollyLookAtTargets interpolates the original world targets", "[unit][spline-look-at]")
{
    LookAtFixture fixture;
    const math::Vec3 first{20.0f, 10.0f, 0.0f};
    const math::Vec3 second{-10.0f, 0.0f, -10.0f};
    fixture.lookAt->targets = {
        {0.5f, {0, first, 1.0f}},
        {0.0f, {0, second, 1.0f}},
    };
    fixture.lookAt->validate();

    fixture.dolly->spline.position = 0.0f;
    CameraState state = fixture.update();
    REQUIRE(forwardDot(state, second - state.rawPosition)
        == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceLookAt, second));

    fixture.dolly->spline.position = 0.25f;
    state = fixture.update();
    const math::Vec3 midpoint = math::lerp(second, first, 0.5f);
    REQUIRE(forwardDot(state, midpoint - state.rawPosition)
        == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceLookAt, midpoint, 1e-4f));

    fixture.dolly->spline.position = 0.5f;
    state = fixture.update();
    REQUIRE(forwardDot(state, first - state.rawPosition)
        == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceLookAt, first));
}

TEST_CASE("SplineDollyLookAtTargets applies per-item easing", "[unit][spline-look-at]")
{
    LookAtFixture fixture;
    fixture.lookAt->targets = {
        {0.0f, {0, {0.0f, 0.0f, 0.0f}, 1.0f}},
        {0.5f, {0, {10.0f, 0.0f, 0.0f}, 1.0f}},
    };
    fixture.lookAt->validate();

    fixture.dolly->spline.position = 0.125f;
    CameraState state = fixture.update();
    REQUIRE(state.referenceLookAt.x == Catch::Approx(1.5625f).margin(1e-4f));

    fixture.lookAt->targets[0].item.easing = 0.0f;
    fixture.lookAt->targets[1].item.easing = 0.0f;
    state = fixture.update();
    REQUIRE(state.referenceLookAt.x == Catch::Approx(2.5f).margin(1e-3f));
}

TEST_CASE("SplineDollyLookAtTargets resolves entity offsets and missing entities", "[unit][spline-look-at]")
{
    LookAtFixture fixture;
    const EntityId targetEntity = 10;
    const math::Quat targetRotation =
        math::quatFromAxisAngle(math::Vec3::up, math::Pi * 0.5f);
    fixture.world.add(
        targetEntity,
        Transform{
            {3.0f, 2.0f, 4.0f},
            targetRotation,
            math::Vec3::one});
    fixture.lookAt->targets = {
        {0.0f, {targetEntity, {2.0f, 0.0f, 0.0f}, 1.0f}},
    };
    fixture.lookAt->validate();

    CameraState state = fixture.update();
    const math::Vec3 expected = fixture.world.transform(targetEntity).position
        + math::rotate(targetRotation, {2.0f, 0.0f, 0.0f});
    REQUIRE(math::approximately(state.referenceLookAt, expected, 1e-4f));

    fixture.lookAt->targets[0].item.lookAt = 999;
    fixture.lookAt->targets[0].item.offset = {6.0f, 5.0f, 4.0f};
    state = fixture.update();
    REQUIRE(math::approximately(state.referenceLookAt, {6.0f, 5.0f, 4.0f}));
}

TEST_CASE("SplineDollyLookAtTargets clamps endpoint keys", "[unit][spline-look-at]")
{
    LookAtFixture fixture;
    fixture.lookAt->targets = {
        {0.25f, {0, {1.0f, 0.0f, 0.0f}, 1.0f}},
        {0.75f, {0, {9.0f, 0.0f, 0.0f}, 1.0f}},
    };
    fixture.lookAt->validate();

    fixture.dolly->spline.position = 0.0f;
    REQUIRE(math::approximately(fixture.update().referenceLookAt, {1.0f, 0.0f, 0.0f}));
    fixture.dolly->spline.position = 0.99f;
    REQUIRE(math::approximately(fixture.update().referenceLookAt, {9.0f, 0.0f, 0.0f}));
}

TEST_CASE("SplineDollyLookAtTargets handles parallel up and zero directions", "[unit][spline-look-at]")
{
    LookAtFixture fixture;
    fixture.lookAt->targets = {
        {0.0f, {0, {7.0f, 11.0f, -6.0f}, 1.0f}},
    };
    fixture.lookAt->validate();

    CameraState state = fixture.update();
    REQUIRE(forwardDot(state, math::Vec3::up)
        == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE_FALSE(state.rawOrientation.isNaN());
    REQUIRE(math::approximately(state.referenceLookAt, {7.0f, 11.0f, -6.0f}));

    fixture.lookAt->targets[0].item.offset = state.rawPosition;
    state = fixture.update();
    REQUIRE_FALSE(state.rawOrientation.isNaN());
    REQUIRE(math::approximately(state.referenceLookAt, state.rawPosition));
}
