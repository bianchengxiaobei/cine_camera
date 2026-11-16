#include "fake_entity_world.hpp"
#include "test_camera.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("Standby modes control non-live camera updates", "[scenario][camera-system][standby]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    system.setUpdateMethod(UpdateMethod::Late);

    const CameraId never = system.createCamera<TestCamera>("never");
    const CameraId always = system.createCamera<TestCamera>("always");
    const CameraId roundRobin = system.createCamera<TestCamera>("round-robin");

    system.camera(never)->standbyUpdate = StandbyUpdateMode::Never;
    system.camera(always)->standbyUpdate = StandbyUpdateMode::Always;
    system.camera(roundRobin)->standbyUpdate = StandbyUpdateMode::RoundRobin;

    world.setFrame(UpdateClock::Late, 1);
    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(static_cast<TestCamera*>(system.camera(never))->updateCount == 0);
    REQUIRE(static_cast<TestCamera*>(system.camera(always))->updateCount == 1);

    system.camera(never)->setLive(true);
    world.setFrame(UpdateClock::Late, 2);
    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(static_cast<TestCamera*>(system.camera(never))->updateCount == 1);

    system.camera(never)->setLive(false);
    system.camera(never)->standbyUpdate = StandbyUpdateMode::Never;
    world.setFrame(UpdateClock::Late, 3);
    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(static_cast<TestCamera*>(system.camera(never))->updateCount == 1);
}

TEST_CASE("Round robin cycles through one standby camera per rendered frame", "[scenario][camera-system][standby]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    system.setUpdateMethod(UpdateMethod::Late);

    const CameraId a = system.createCamera<TestCamera>("a");
    const CameraId b = system.createCamera<TestCamera>("b");
    const CameraId c = system.createCamera<TestCamera>("c");

    for (CameraId id : {a, b, c})
    {
        system.camera(id)->standbyUpdate = StandbyUpdateMode::RoundRobin;
        system.camera(id)->setLive(false);
    }

    for (std::uint64_t frame = 0; frame < 3; ++frame)
    {
        world.setFrame(UpdateClock::Late, frame);
        system.lateUpdate(1.0f / 60.0f);
    }

    REQUIRE(static_cast<TestCamera*>(system.camera(a))->updateCount == 1);
    REQUIRE(static_cast<TestCamera*>(system.camera(b))->updateCount == 1);
    REQUIRE(static_cast<TestCamera*>(system.camera(c))->updateCount == 1);
}

TEST_CASE("Channel mask excludes cameras from updates", "[scenario][camera-system][standby]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    system.setUpdateMethod(UpdateMethod::Late);

    const CameraId id = system.createCamera<TestCamera>("channel");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));
    camera->outputChannel = OutputChannels::Channel01;
    camera->setLive(true);

    system.setChannelMask(OutputChannels::Default);
    world.setFrame(UpdateClock::Late, 1);
    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(camera->updateCount == 0);

    system.setChannelMask(OutputChannels::Channel01);
    world.setFrame(UpdateClock::Late, 2);
    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(camera->updateCount == 1);
}
