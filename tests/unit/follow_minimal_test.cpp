#include "fake_entity_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/follow.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct FollowFixture
{
    FollowFixture()
    {
        world.add(target, Transform{});
        const CameraId id = system.createCamera<VirtualCamera>("follow");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        follow = &camera->setBody<Follow>();
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    Follow* follow = nullptr;
    EntityId target = 1;
};

} // namespace

TEST_CASE("Minimal Follow snaps to the world-space offset on first update", "[unit][follow]")
{
    FollowFixture fixture;

    REQUIRE(fixture.follow->isValid());
    REQUIRE(math::approximately(fixture.follow->followOffset, {0.0f, 0.0f, -10.0f}));
    REQUIRE(math::approximately(fixture.follow->trackerSettings.positionDamping, math::Vec3::one));

    CameraState state = CameraState::makeDefault();
    fixture.follow->mutateCameraState(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.rawPosition, {0.0f, 0.0f, -10.0f}));
}

TEST_CASE("Minimal Follow applies per-axis damping and snaps on negative time", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.camera->previousStateIsValid = true;

    fixture.follow->trackerSettings.positionDamping = {1.0f, 1.0f, 1.0f};
    CameraState state = CameraState::makeDefault();
    fixture.follow->mutateCameraState(state, 1.0f / 60.0f);

    fixture.world.set(
        fixture.target,
        Transform{{10.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});

    fixture.follow->mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(state.rawPosition.x > 0.0f);
    REQUIRE(state.rawPosition.x < 10.0f);

    fixture.follow->mutateCameraState(state, -1.0f);
    REQUIRE(math::approximately(state.rawPosition, {10.0f, 0.0f, -10.0f}));
}

TEST_CASE("Minimal Follow is immediate with zero damping", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.camera->previousStateIsValid = true;
    fixture.follow->trackerSettings.positionDamping = math::Vec3::zero;
    fixture.world.set(
        fixture.target,
        Transform{{3.0f, 4.0f, 5.0f}, math::Quat::identity(), math::Vec3::one});

    CameraState state = CameraState::makeDefault();
    fixture.follow->mutateCameraState(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.rawPosition, {3.0f, 4.0f, -5.0f}));
    REQUIRE(fixture.follow->maxDampTime() == 0.0f);
}

TEST_CASE("Minimal Follow leaves state unchanged when the target is invalid", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.camera->previousStateIsValid = true;
    fixture.camera->targets.tracking = TargetRef::entity(999);

    CameraState state = CameraState::makeDefault();
    state.rawPosition = {4.0f, 5.0f, 6.0f};

    REQUIRE_FALSE(fixture.follow->isValid());
    fixture.follow->mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.rawPosition, {4.0f, 5.0f, 6.0f}));
}
