#include "fake_entity_world.hpp"
#include "test_camera.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("Fixed and late clocks update at most once per frame", "[scenario][camera-system][clock]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    const CameraId id = system.createCamera<TestCamera>("clock");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));

    system.setUpdateMethod(UpdateMethod::Fixed);
    world.setFrame(UpdateClock::Fixed, 0);
    system.fixedUpdate(0.02f);
    REQUIRE(camera->updateCount == 1);
    REQUIRE(camera->lastDeltaTime == Catch::Approx(0.02f));

    system.fixedUpdate(0.02f);
    REQUIRE(camera->updateCount == 1);

    world.setFrame(UpdateClock::Fixed, 1);
    system.fixedUpdate(0.02f);
    REQUIRE(camera->updateCount == 2);

    system.setUpdateMethod(UpdateMethod::Late);
    world.setFrame(UpdateClock::Late, 0);
    system.lateUpdate(0.016f);
    const int lateCount = camera->updateCount;
    REQUIRE(lateCount == 3);
    system.lateUpdate(0.016f);
    REQUIRE(camera->updateCount == lateCount);

    world.setFrame(UpdateClock::Late, 1);
    system.lateUpdate(0.016f);
    REQUIRE(camera->updateCount == lateCount + 1);
}

TEST_CASE("Smart update follows target clock preference", "[scenario][camera-system][clock]")
{
    FakeEntityWorld world;
    world.add(42, Transform{});
    CameraSystem system(world);
    const CameraId id = system.createCamera<TestCamera>("smart");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));

    camera->targets.tracking = TargetRef::entity(42);
    camera->setLive(true);
    system.setUpdateMethod(UpdateMethod::Smart);
    world.setLastChangeClock(42, UpdateClock::Fixed);

    world.setFrame(UpdateClock::Fixed, 1);
    system.fixedUpdate(0.02f);
    REQUIRE(camera->updateCount == 1);

    world.setFrame(UpdateClock::Late, 1);
    system.lateUpdate(0.016f);
    REQUIRE(camera->updateCount == 1);

    world.setLastChangeClock(42, UpdateClock::Late);
    world.setFrame(UpdateClock::Late, 2);
    system.lateUpdate(0.016f);
    REQUIRE(camera->updateCount == 2);

    world.setFrame(UpdateClock::Fixed, 2);
    system.fixedUpdate(0.02f);
    REQUIRE(camera->updateCount == 2);
}

TEST_CASE("Smart update infers fixed movement when the world reports no clock", "[scenario][camera-system][clock]")
{
    FakeEntityWorld world;
    world.add(7, Transform{});
    CameraSystem system(world);
    const CameraId id = system.createCamera<TestCamera>("inferred");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));
    camera->targets.tracking = TargetRef::entity(7);
    camera->setLive(true);
    system.setUpdateMethod(UpdateMethod::Smart);

    world.setFrame(UpdateClock::Fixed, 0);
    system.fixedUpdate(0.02f);

    for (int frame = 1; frame <= 4; ++frame)
    {
        world.setFrame(UpdateClock::Fixed, static_cast<std::uint64_t>(frame));
        world.set(7, Transform{{static_cast<float>(frame), 0.0f, 0.0f}, math::Quat::identity(), {}});
        system.fixedUpdate(0.02f);
    }

    REQUIRE(camera->updateCount >= 1);
}

TEST_CASE("Multi-frame updates catch up while negative dt never catches up", "[scenario][camera-system][clock]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    const CameraId id = system.createCamera<TestCamera>("catch-up");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));
    system.setUpdateMethod(UpdateMethod::Fixed);

    world.setFrame(UpdateClock::Fixed, 0);
    system.fixedUpdate(0.01f);
    REQUIRE(camera->lastDeltaTime == Catch::Approx(0.01f));

    world.setFrame(UpdateClock::Fixed, 3);
    system.fixedUpdate(0.01f);
    REQUIRE(camera->lastDeltaTime == Catch::Approx(0.03f));

    system.manualUpdate(-1.0f);
    REQUIRE(camera->lastDeltaTime == Catch::Approx(-1.0f));
}

TEST_CASE("Manual update is deterministic for a fixed frame", "[scenario][camera-system][clock]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    const CameraId id = system.createCamera<TestCamera>("manual");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));
    system.setUpdateMethod(UpdateMethod::Manual);

    world.setFrame(UpdateClock::Late, 10);
    system.manualUpdate(0.02f);
    REQUIRE(camera->updateCount == 1);
    system.manualUpdate(0.02f);
    REQUIRE(camera->updateCount == 1);

    world.setFrame(UpdateClock::Late, 11);
    system.manualUpdate(0.02f);
    REQUIRE(camera->updateCount == 2);
}
