#include "fake_entity_world.hpp"

#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/position_composer.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct PositionComposerFixture
{
    PositionComposerFixture()
    {
        world.add(target, Transform{});
        const CameraId id = system.createCamera<VirtualCamera>("position composer");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        camera->ownerEntity = 2;
        world.add(camera->ownerEntity, {
            {0.0f, 0.0f, 5.0f},
            math::Quat::identity(),
            math::Vec3::one,
        });
        composer = &camera->setBody<PositionComposer>();
        composer->damping = math::Vec3::zero;
        camera->lens.fieldOfView = 60.0f;
        camera->lens.aspect = 1.0f;
    }

    CameraState makeState() const
    {
        CameraState state = CameraState::makeDefault();
        state.rawPosition = {0.0f, 0.0f, 5.0f};
        state.rawOrientation = math::Quat::identity();
        state.lens = camera->lens;
        if (const auto lookAt = camera->lookAtTargetPosition())
            state.referenceLookAt = *lookAt;
        return state;
    }

    void step(CameraState& state, float deltaTime)
    {
        state.lens = camera->lens;
        if (const auto lookAt = camera->lookAtTargetPosition())
            state.referenceLookAt = *lookAt;
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
    PositionComposer* composer = nullptr;
    EntityId target = 1;
};

} // namespace

TEST_CASE("PositionComposer exposes full defaults", "[unit][position-composer]")
{
    PositionComposer composer;
    REQUIRE(composer.cameraDistance == Catch::Approx(10.0f));
    REQUIRE(composer.deadZoneDepth == Catch::Approx(0.0f));
    REQUIRE(math::approximately(composer.composition.screenPosition, math::Vec2::zero));
    REQUIRE(composer.centerOnActivate);
    REQUIRE(math::approximately(composer.targetOffset, math::Vec3::zero));
    REQUIRE(math::approximately(composer.damping, math::Vec3::one));
    REQUIRE_FALSE(composer.lookahead.enabled);
    REQUIRE(composer.stage() == Stage::Body);
    REQUIRE(composer.bodyAppliesAfterAim());
    REQUIRE(composer.maxDampTime() == Catch::Approx(1.0f));
}

TEST_CASE("PositionComposer centers the target at camera distance", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.setTarget({10.0f, 0.0f, 0.0f});
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.rawPosition, {10.0f, 0.0f, 10.0f}, 1e-4f));
    REQUIRE(math::approximately(fixture.project(state, {10.0f, 0.0f, 0.0f}), {0.5f, 0.5f}, 1e-4f));
}

TEST_CASE("PositionComposer maps screen position to perspective framing", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.composer->centerOnActivate = false;
    fixture.composer->composition.screenPosition = {0.25f, -0.2f};
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, math::Vec3::zero);
    REQUIRE(projected.x == Catch::Approx(0.75f).margin(1e-2f));
    REQUIRE(projected.y == Catch::Approx(0.30f).margin(1e-2f));
}

TEST_CASE("PositionComposer keeps depth inside the dead-zone band", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.composer->centerOnActivate = false;
    fixture.composer->cameraDistance = 10.0f;
    fixture.composer->deadZoneDepth = 4.0f;
    CameraState state = fixture.makeState();
    state.rawPosition = {0.0f, 0.0f, 8.0f};

    fixture.step(state, 1.0f / 60.0f);
    REQUIRE(state.rawPosition.z == Catch::Approx(8.0f).margin(1e-4f));

    fixture.camera->previousStateIsValid = false;
    state.rawPosition = {0.0f, 0.0f, 2.0f};
    fixture.step(state, 1.0f / 60.0f);
    REQUIRE(state.rawPosition.z == Catch::Approx(8.0f).margin(1e-4f));
}

TEST_CASE("PositionComposer applies target offset in target-local space", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.setTarget(
        {0.0f, 0.0f, 0.0f},
        math::quatFromAxisAngle(math::Vec3::up, math::HalfPi));
    fixture.composer->targetOffset = {1.0f, 0.0f, 0.0f};
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(state.rawPosition.z == Catch::Approx(9.0f).margin(1e-4f));
    REQUIRE(math::approximately(fixture.project(state, {0.0f, 0.0f, -1.0f}), {0.5f, 0.5f}, 1e-4f));
}

TEST_CASE("PositionComposer uses the soft guide edge without centerOnActivate", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.composer->centerOnActivate = false;
    fixture.composer->composition.deadZone.enabled = true;
    fixture.composer->composition.deadZone.size = {0.4f, 0.4f};
    fixture.setTarget({5.0f, 0.0f, 0.0f});
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, {5.0f, 0.0f, 0.0f});
    REQUIRE(projected.x > 0.5f);
    REQUIRE(projected.x < 0.71f);
}

TEST_CASE("PositionComposer holds position inside the dead zone", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);
    const math::Vec3 centered = state.rawPosition;

    fixture.composer->composition.deadZone.enabled = true;
    fixture.composer->composition.deadZone.size = {0.5f, 0.5f};
    fixture.setTarget({0.4f, 0.2f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.rawPosition, centered, 1e-4f));
}

TEST_CASE("PositionComposer clamps the real target to hard limits", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->damping = {10.0f, 10.0f, 10.0f};
    fixture.composer->composition.hardLimits.enabled = true;
    fixture.composer->composition.hardLimits.size = {0.4f, 0.4f};
    fixture.setTarget({20.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, {20.0f, 0.0f, 0.0f});
    REQUIRE(projected.x <= 0.7f + 1e-3f);
    REQUIRE(projected.x >= 0.5f);
}

TEST_CASE("PositionComposer applies screen-position changes without damping", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.composer->damping = math::Vec3::one;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->composition.screenPosition = {0.25f, 0.2f};
    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, math::Vec3::zero);
    REQUIRE(projected.x == Catch::Approx(0.75f).margin(1e-2f));
    REQUIRE(projected.y == Catch::Approx(0.70f).margin(1e-2f));
}

TEST_CASE("PositionComposer supports orthographic framing", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 5.0f;
    fixture.composer->centerOnActivate = false;
    fixture.composer->composition.screenPosition = {0.25f, 0.0f};
    CameraState state = fixture.makeState();

    fixture.step(state, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(state, math::Vec3::zero);
    REQUIRE(projected.x == Catch::Approx(0.75f).margin(1e-2f));
}

TEST_CASE("PositionComposer predicts target movement for lookahead", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->lookahead.enabled = true;
    fixture.composer->lookahead.time = 0.2f;
    fixture.composer->lookahead.smoothing = 0.0f;

    fixture.setTarget({1.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({2.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({3.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(state.rawPosition.x > 3.0f);
}

TEST_CASE("PositionComposer lookahead can ignore Y motion", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->lookahead.enabled = true;
    fixture.composer->lookahead.time = 0.2f;
    fixture.composer->lookahead.ignoreY = true;

    fixture.setTarget({0.0f, 1.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({0.0f, 2.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    fixture.setTarget({0.0f, 3.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);

    REQUIRE(state.rawPosition.y == Catch::Approx(3.0f).margin(1e-3f));
}

TEST_CASE("PositionComposer snaps on negative time", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.composer->damping = math::Vec3::one;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.setTarget({5.0f, 0.0f, 0.0f});
    fixture.step(state, -1.0f);

    const math::Vec2 projected = fixture.project(state, {5.0f, 0.0f, 0.0f});
    REQUIRE(projected.x == Catch::Approx(0.5f).margin(1e-3f));
}

TEST_CASE("PositionComposer leaves position unchanged for an invalid target", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.camera->targets.tracking = TargetRef::entity(999);

    CameraState state = CameraState::makeDefault();
    state.rawPosition = {4.0f, 5.0f, 6.0f};
    state.rawOrientation = math::Quat::identity();
    state.lens = fixture.camera->lens;

    REQUIRE_FALSE(fixture.composer->isValid());
    fixture.composer->mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.rawPosition, {4.0f, 5.0f, 6.0f}));
}

TEST_CASE("PositionComposer accepts target warps and forced positions", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    CameraState state = fixture.makeState();
    fixture.step(state, 1.0f / 60.0f);

    fixture.composer->onTargetObjectWarped(fixture.target, {100.0f, 0.0f, 0.0f});
    fixture.setTarget({100.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    REQUIRE(std::isfinite(state.rawPosition.x));
    REQUIRE(state.rawPosition.x == Catch::Approx(100.0f).margin(1e-2f));

    fixture.camera->forceCameraPosition(
        {0.0f, 5.0f, 5.0f},
        math::quatFromAxisAngle(math::Vec3::up, 0.4f));
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.step(state, 1.0f / 60.0f);
    REQUIRE(std::isfinite(state.rawPosition.x));
    REQUIRE(std::isfinite(state.rawPosition.z));
}

TEST_CASE("PositionComposer respects InheritPosition on transition", "[unit][position-composer]")
{
    PositionComposerFixture fixture;
    fixture.composer->composition.deadZone.enabled = true;
    fixture.composer->composition.deadZone.size = {0.4f, 0.4f};
    fixture.camera->blendHint = BlendHint::InheritPosition;

    const CameraId fromId = fixture.system.createCamera<VirtualCamera>("from");
    VirtualCamera* from = fixture.system.camera(fromId);
    from->ownerEntity = 3;
    fixture.world.add(from->ownerEntity, {
        {5.0f, 0.0f, 10.0f},
        math::Quat::identity(),
        math::Vec3::one,
    });
    from->updateCameraState(math::Vec3::up, 1.0f / 60.0f);

    fixture.camera->onTransitionFromCamera(from, math::Vec3::up, 1.0f / 60.0f);

    const math::Vec2 projected = fixture.project(
        fixture.camera->state(),
        math::Vec3::zero);
    REQUIRE(projected.x == Catch::Approx(0.3f).margin(1e-2f));
}
