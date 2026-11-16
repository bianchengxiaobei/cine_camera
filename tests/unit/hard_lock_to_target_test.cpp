#include "fake_entity_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/hard_lock_to_target.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct HardLockFixture
{
    HardLockFixture()
    {
        world.add(target, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("hard lock");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        lock = &camera->setBody<HardLockToTarget>();
    }

    void setTarget(math::Vec3 position)
    {
        world.set(target, Transform{position, math::Quat::identity(), math::Vec3::one});
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
    HardLockToTarget* lock = nullptr;
    EntityId target = 1;
    float time = 0.0f;
};

} // namespace

TEST_CASE("HardLockToTarget locks to the tracking target", "[unit][hard-lock]")
{
    HardLockFixture fixture;
    REQUIRE(fixture.lock->stage() == Stage::Body);
    REQUIRE(fixture.lock->isValid());
    REQUIRE(fixture.lock->damping == 0.0f);
    REQUIRE(fixture.lock->maxDampTime() == 0.0f);

    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {1.0f, 2.0f, 3.0f},
        1e-4f));

    fixture.setTarget({-4.0f, 5.0f, 6.0f});
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {-4.0f, 5.0f, 6.0f},
        1e-4f));
}

TEST_CASE("HardLockToTarget damps target motion and snaps on negative time", "[unit][hard-lock]")
{
    HardLockFixture fixture;
    fixture.lock->damping = 0.5f;
    fixture.step();
    fixture.setTarget({10.0f, 0.0f, 0.0f});
    fixture.step();

    REQUIRE(fixture.camera->state().rawPosition.x > 0.0f);
    REQUIRE(fixture.camera->state().rawPosition.x < 10.0f);

    for (int frame = 0; frame < 120; ++frame)
        fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {10.0f, 0.0f, 0.0f},
        1e-3f));

    fixture.setTarget({20.0f, 0.0f, 0.0f});
    fixture.camera->updateCameraState(math::Vec3::up, -1.0f);
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {20.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("HardLockToTarget handles target warps and attachment damping", "[unit][hard-lock]")
{
    HardLockFixture fixture;
    fixture.lock->damping = 0.5f;
    fixture.step();

    fixture.setTarget({11.0f, 2.0f, 3.0f});
    fixture.camera->onTargetObjectWarped(fixture.target, {10.0f, 0.0f, 0.0f});
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {11.0f, 2.0f, 3.0f},
        1e-3f));

    fixture.camera->followTargetAttachment = 0.0f;
    CameraState state = fixture.camera->state();
    fixture.lock->mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::approximately(
        state.rawPosition,
        {11.0f, 2.0f, 3.0f},
        1e-3f));
}

TEST_CASE("HardLockToTarget ignores missing targets and validates damping", "[unit][hard-lock]")
{
    HardLockFixture fixture;
    fixture.lock->damping = -2.0f;
    fixture.lock->validate();
    REQUIRE(fixture.lock->damping == 0.0f);

    fixture.world.remove(fixture.target);
    REQUIRE_FALSE(fixture.lock->isValid());
    const math::Vec3 before = fixture.camera->state().rawPosition;
    fixture.step();
    REQUIRE(math::approximately(fixture.camera->state().rawPosition, before, 1e-4f));
}

