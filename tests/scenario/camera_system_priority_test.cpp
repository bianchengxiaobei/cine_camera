#include "fake_entity_world.hpp"
#include "test_camera.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("Active cameras sort by priority and activation order", "[scenario][camera-system][priority]")
{
    FakeEntityWorld world;
    CameraSystem system(world);

    const CameraId a = system.createCamera<TestCamera>("A");
    const CameraId b = system.createCamera<TestCamera>("B");
    const CameraId c = system.createCamera<TestCamera>("C");

    system.camera(a)->priority = PrioritySettings(10);
    system.camera(b)->priority = PrioritySettings(10);
    system.camera(c)->priority = PrioritySettings(20);

    REQUIRE(system.activeCameraCount() == 3);
    REQUIRE(system.activeCamera(0)->name() == "C");
    REQUIRE(system.activeCamera(1)->name() == "B");
    REQUIRE(system.activeCamera(2)->name() == "A");

    system.camera(b)->priority = PrioritySettings(30);
    REQUIRE(system.activeCamera(0)->name() == "B");
    system.camera(b)->priority = PrioritySettings(10);

    REQUIRE(system.setCameraEnabled(a, false));
    REQUIRE(system.activeCameraCount() == 2);
    REQUIRE(system.setCameraEnabled(a, true));
    REQUIRE(system.activeCamera(0)->name() == "C");
    REQUIRE(system.activeCamera(1)->name() == "A");
    REQUIRE(system.activeCamera(2)->name() == "B");
    REQUIRE(system.camera(a)->activationId() > system.camera(b)->activationId());
}

TEST_CASE("Nested cameras update leaf-first", "[scenario][camera-system][priority]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    system.setUpdateMethod(UpdateMethod::Late);

    std::vector<std::string> order;
    const CameraId parent = system.createCamera<TestCamera>("parent", &order);
    const CameraId child = system.createCamera<TestCamera>("child", &order);
    REQUIRE(system.setCameraParent(child, parent));

    system.camera(parent)->setLive(true);
    system.camera(child)->setLive(true);

    const auto& levels = system.registry().camerasByLevel();
    REQUIRE(levels.size() == 2);
    REQUIRE(levels[1].size() == 1);
    REQUIRE(levels[1][0] == system.camera(child));

    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(order.size() == 2);
    REQUIRE(order[0] == "child");
    REQUIRE(order[1] == "parent");

    REQUIRE_FALSE(system.setCameraParent(parent, child));
}
