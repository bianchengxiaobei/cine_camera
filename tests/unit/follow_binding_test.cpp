#include "fake_entity_world.hpp"

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
        follow->followOffset = {0.0f, 0.0f, -6.0f};
        follow->trackerSettings.positionDamping = math::Vec3::zero;
        follow->trackerSettings.rotationDamping = math::Vec3::zero;
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    Follow* follow = nullptr;
    EntityId target = 1;
};

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

} // namespace

TEST_CASE("Follow LockToTarget rotates its offset with the target", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.follow->trackerSettings.bindingMode = BindingMode::LockToTarget;
    const math::Quat rotation = yaw(90.0f);
    fixture.world.set(
        fixture.target,
        Transform{{1.0f, 2.0f, 3.0f}, rotation, math::Vec3::one});

    CameraState state = CameraState::makeDefault();
    fixture.follow->mutateCameraState(state, 1.0f / 60.0f);

    const math::Vec3 expected = math::Vec3{1.0f, 2.0f, 3.0f}
        + math::rotate(rotation, fixture.follow->followOffset);
    REQUIRE(math::approximately(state.rawPosition, expected));
}

TEST_CASE("Follow LazyFollow keeps the offset behind the camera", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.follow->trackerSettings.bindingMode = BindingMode::LazyFollow;
    fixture.follow->followOffset = {2.0f, 0.0f, -6.0f};

    const math::Vec3 offset = fixture.follow->effectiveOffset();
    REQUIRE(offset.x == 0.0f);
    REQUIRE(offset.z == -6.0f);
    REQUIRE(fixture.follow->trackerSettings.effectivePositionDamping().x == 0.0f);
}

TEST_CASE("Follow force position is retained on a zero-delta update", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.follow->trackerSettings.positionDamping = {1.0f, 1.0f, 1.0f};

    fixture.camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);
    REQUIRE(math::approximately(fixture.camera->state().rawPosition, {0.0f, 0.0f, -6.0f}));

    const math::Vec3 forcedPosition{0.0f, 5.0f, -5.0f};
    fixture.camera->forceCameraPosition(forcedPosition, math::Quat::identity());
    fixture.camera->updateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        forcedPosition,
        1e-3f));
}

TEST_CASE("Follow target warping moves the tracked state with the target", "[unit][follow]")
{
    FollowFixture fixture;
    fixture.camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);
    REQUIRE(math::approximately(fixture.camera->state().rawPosition, {0.0f, 0.0f, -6.0f}));

    fixture.world.set(
        fixture.target,
        Transform{{3.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    fixture.camera->onTargetObjectWarped(fixture.target, {3.0f, 0.0f, 0.0f});
    fixture.camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);

    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {3.0f, 0.0f, -6.0f},
        1e-3f));
}
