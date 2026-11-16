#include "fake_entity_world.hpp"

#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/rotation_composer.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct ComposerFixture
{
    ComposerFixture()
    {
        world.add(target, Transform{});
        const CameraId id = system.createCamera<VirtualCamera>("composer");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        composer = &camera->setAim<RotationComposer>();
        composer->damping = math::Vec2::zero;
        camera->lens.fieldOfView = 60.0f;
        camera->lens.aspect = 1.0f;
    }

    CameraState makeState() const
    {
        CameraState state = CameraState::makeDefault();
        state.rawPosition = {0.0f, 0.0f, 5.0f};
        state.lens = camera->lens;
        state.referenceLookAt = *camera->lookAtTargetPosition();
        return state;
    }

    void step(CameraState& state, float deltaTime)
    {
        state.rawPosition = {0.0f, 0.0f, 5.0f};
        state.lens = camera->lens;
        state.referenceLookAt = *camera->lookAtTargetPosition();
        composer->prePipelineMutate(state, deltaTime);
        composer->mutateCameraState(state, deltaTime);
        camera->previousStateIsValid = true;
    }

    void setTarget(math::Vec3 position, math::Quat rotation = math::Quat::identity())
    {
        world.set(target, Transform{position, rotation, math::Vec3::one});
    }

    math::Vec2 project(const CameraState& state, math::Vec3 worldPoint) const
    {
        const math::Vec3 local = math::rotate(
            math::inverse(state.rawOrientation),
            worldPoint - state.rawPosition);
        if (state.lens.orthographic)
        {
            const float halfHeight = std::max(state.lens.orthographicSize, math::Epsilon);
            const float halfWidth = halfHeight * state.lens.aspect;
            return {
                0.5f + local.x / (2.0f * halfWidth),
                0.5f + local.y / (2.0f * halfHeight),
            };
        }

        const float distance = -local.z;
        if (distance <= math::Epsilon)
            return {0.5f, 0.5f};
        const float halfHeight = std::tan(state.lens.fieldOfView * math::DegToRad * 0.5f);
        const float halfWidth = halfHeight * state.lens.aspect;
        return {
            0.5f + (local.x / distance) / (2.0f * halfWidth),
            0.5f + (local.y / distance) / (2.0f * halfHeight),
        };
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    RotationComposer* composer = nullptr;
    EntityId target = 1;
};

} // namespace

TEST_CASE("RotationComposer exposes full defaults", "[unit][rotation-composer]")
{
    RotationComposer composer;
    REQUIRE(math::approximately(composer.composition.screenPosition, math::Vec2::zero));
    REQUIRE(composer.composition.deadZone.enabled == false);
    REQUIRE(composer.composition.hardLimits.enabled == false);
    REQUIRE(composer.centerOnActivate);
    REQUIRE(math::approximately(composer.targetOffset, math::Vec3::zero));
    REQUIRE(math::approximately(composer.damping, math::Vec2{0.5f, 0.5f}));
    REQUIRE_FALSE(composer.lookahead.enabled);
    REQUIRE(composer.maxDampTime() == Catch::Approx(0.5f));
}

TEST_CASE("RotationComposer centers the target when composition is centered", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.rawOrientation, math::Quat::identity(), 1e-4f));
    REQUIRE(math::approximately(fixture.project(state, math::Vec3::zero), {0.5f, 0.5f}, 1e-4f));
}

TEST_CASE("RotationComposer maps screen position to perspective framing", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.composer->centerOnActivate = false;
    fixture.composer->composition.screenPosition = {0.25f, -0.2f};
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, math::Vec3::zero);
    REQUIRE(projected.x == Catch::Approx(0.75f).margin(1e-2f));
    REQUIRE(projected.y == Catch::Approx(0.30f).margin(1e-2f));
}

TEST_CASE("RotationComposer centerOnActivate centers the target", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.composer->composition.deadZone.enabled = true;
    fixture.composer->composition.deadZone.size = {0.4f, 0.4f};
    fixture.setTarget({5.0f, 0.0f, 5.0f});
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, {5.0f, 0.0f, 5.0f});
    REQUIRE(projected.x == Catch::Approx(0.5f).margin(1e-3f));
    REQUIRE(projected.y == Catch::Approx(0.5f).margin(1e-3f));
}

TEST_CASE("RotationComposer uses the soft guide edge without centerOnActivate", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.composer->centerOnActivate = false;
    fixture.composer->composition.deadZone.enabled = true;
    fixture.composer->composition.deadZone.size = {0.4f, 0.4f};
    fixture.setTarget({5.0f, 0.0f, 5.0f});
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, {5.0f, 0.0f, 5.0f});
    REQUIRE(projected.x > 0.5f);
    REQUIRE(projected.x < 0.71f);
}

TEST_CASE("RotationComposer applies target offset in target-local space", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.setTarget(
        {0.0f, 0.0f, 0.0f},
        math::quatFromAxisAngle(math::Vec3::up, math::HalfPi));
    fixture.composer->targetOffset = {1.0f, 0.0f, 0.0f};
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.referenceLookAt, {0.0f, 0.0f, -1.0f}, 1e-4f));
    REQUIRE(math::approximately(fixture.project(state, {0.0f, 0.0f, -1.0f}), {0.5f, 0.5f}, 1e-4f));
}

TEST_CASE("RotationComposer holds orientation inside the dead zone", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);
    const math::Quat centered = state.rawOrientation;

    fixture.composer->composition.deadZone.enabled = true;
    fixture.composer->composition.deadZone.size = {0.5f, 0.5f};
    fixture.setTarget({0.4f, 0.2f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(math::angle(state.rawOrientation, centered) < 1e-4f);
}

TEST_CASE("RotationComposer clamps the target to hard limits", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->damping = {10.0f, 10.0f};
    fixture.composer->composition.hardLimits.enabled = true;
    fixture.composer->composition.hardLimits.size = {0.4f, 0.4f};
    fixture.setTarget({20.0f, 0.0f, 5.0f});
    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, {20.0f, 0.0f, 5.0f});
    REQUIRE(projected.x <= 0.7f + 1e-3f);
    REQUIRE(projected.x >= 0.5f);
}

TEST_CASE("RotationComposer predicts target movement for lookahead", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->lookahead.enabled = true;
    fixture.composer->lookahead.time = 0.2f;
    fixture.composer->lookahead.smoothing = 0.0f;

    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({1.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({2.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(state.referenceLookAt.x > 2.0f);
}

TEST_CASE("RotationComposer lookahead can ignore Y motion", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->lookahead.enabled = true;
    fixture.composer->lookahead.time = 0.2f;
    fixture.composer->lookahead.ignoreY = true;

    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({0.0f, 1.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({0.0f, 2.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(state.referenceLookAt.y == Catch::Approx(2.0f).margin(1e-3f));
}

TEST_CASE("RotationComposer derives an orthographic effective FOV", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 5.0f;
    fixture.composer->centerOnActivate = false;
    fixture.composer->composition.screenPosition = {0.5f, 0.0f};
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const float yaw = math::angle(state.rawOrientation, math::Quat::identity());
    REQUIRE(yaw == Catch::Approx(45.0f * math::DegToRad).margin(1e-3f));
}

TEST_CASE("RotationComposer snaps on negative time", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.composer->damping = {0.5f, 0.5f};
    CameraState state = fixture.makeState();
    fixture.setTarget({5.0f, 0.0f, 5.0f});
    state.referenceLookAt = *fixture.camera->lookAtTargetPosition();
    fixture.composer->prePipelineMutate(state, 1.0f / 60.0f);
    fixture.composer->mutateCameraState(state, 1.0f / 60.0f);
    fixture.camera->previousStateIsValid = true;

    fixture.composer->mutateCameraState(state, -1.0f);

    const math::Vec2 projected = fixture.project(state, {5.0f, 0.0f, 5.0f});
    REQUIRE(projected.x == Catch::Approx(0.5f).margin(1e-3f));
}

TEST_CASE("RotationComposer leaves orientation unchanged for an invalid target", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    fixture.camera->targets.tracking = TargetRef::entity(999);

    CameraState state = CameraState::makeDefault();
    state.rawPosition = {0.0f, 0.0f, 5.0f};
    state.lens = fixture.camera->lens;
    state.referenceLookAt = math::Vec3::zero;
    state.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, 0.3f);
    const math::Quat original = state.rawOrientation;

    REQUIRE_FALSE(fixture.composer->isValid());
    fixture.composer->mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.rawOrientation, original));
}

TEST_CASE("RotationComposer accepts target warps and forced positions", "[unit][rotation-composer]")
{
    ComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->onTargetObjectWarped(fixture.target, {100.0f, 0.0f, 0.0f});
    fixture.setTarget({100.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    REQUIRE(std::isfinite(state.rawOrientation.x));
    REQUIRE(std::isfinite(state.rawOrientation.w));

    fixture.camera->forceCameraPosition(
        {0.0f, 0.0f, 5.0f},
        math::quatFromAxisAngle(math::Vec3::up, 0.4f));
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    REQUIRE(std::isfinite(state.rawOrientation.x));
    REQUIRE(std::isfinite(state.rawOrientation.w));
}
