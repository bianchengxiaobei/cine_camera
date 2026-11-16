#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"
#include "test_camera.hpp"

#include <memory>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

class FakeTargetGroup final : public TargetGroup
{
public:
    bool isValid() const override { return true; }
    bool isEmpty() const override { return targets.empty(); }
    std::size_t targetCount() const override { return targets.size(); }
    math::Vec3 targetPosition(std::size_t index) const override
    {
        return index < targets.size() ? targets[index] : math::Vec3{};
    }
    math::Quat targetRotation(std::size_t) const override { return math::Quat::identity(); }
    float targetWeight(std::size_t) const override { return 1.0f; }
    float targetRadius(std::size_t) const override { return 0.0f; }

    std::vector<math::Vec3> targets;
};

}

TEST_CASE("Camera handles and owned services have safe lifetimes", "[scenario][camera-system][lifecycle]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    FakeCameraOutput output;

    const CameraId cameraId = system.createCamera<TestCamera>("lifecycle");
    REQUIRE(cameraId != 0);
    REQUIRE(system.camera(cameraId) != nullptr);
    REQUIRE(system.cameraCount() == 1);

    const CameraId secondId = system.createCamera<TestCamera>("second");
    REQUIRE(secondId != cameraId);

    const BrainId brainId = system.createBrain(output, BrainConfig{});
    REQUIRE(brainId != 0);
    REQUIRE(system.brainCount() == 1);

    auto group = std::make_unique<FakeTargetGroup>();
    group->targets.push_back({1.0f, 2.0f, 3.0f});
    const GroupId groupId = system.createTargetGroup(std::move(group));
    REQUIRE(groupId != 0);
    REQUIRE(system.targetGroupCount() == 1);
    REQUIRE(system.targetGroup(groupId) != nullptr);

    system.destroyBrain(brainId);
    system.destroyTargetGroup(groupId);
    REQUIRE(system.brainCount() == 0);
    REQUIRE(system.targetGroupCount() == 0);

    system.destroyCamera(cameraId);
    system.destroyCamera(cameraId);
    REQUIRE(system.camera(cameraId) == nullptr);
    REQUIRE(system.cameraCount() == 1);
    REQUIRE(system.camera(secondId) != nullptr);
}

TEST_CASE("Disable, re-enable and parent changes keep the registry consistent", "[scenario][camera-system][lifecycle]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    const CameraId parent = system.createCamera<TestCamera>("parent");
    const CameraId child = system.createCamera<TestCamera>("child");
    const CameraId orphan = system.createCamera<TestCamera>("orphan");

    REQUIRE(system.activeCameraCount() == 3);
    REQUIRE(system.setCameraParent(child, parent));
    REQUIRE(system.setCameraParent(orphan, parent));

    const int activationBeforeDisable = system.camera(parent)->activationId();
    system.camera(parent)->setEnabled(false);
    REQUIRE(system.activeCameraCount() == 2);

    system.camera(parent)->setEnabled(true);
    REQUIRE(system.activeCameraCount() == 3);
    REQUIRE(system.camera(parent)->activationId() > activationBeforeDisable);

    system.destroyCamera(parent);
    REQUIRE(system.camera(child)->parentVirtualCamera() == nullptr);
    REQUIRE(system.camera(orphan)->parentVirtualCamera() == nullptr);
    REQUIRE(system.activeCameraCount() == 2);

    REQUIRE_FALSE(system.setCameraParent(child, parent));
    REQUIRE_FALSE(system.setCameraParent(child, 9999));
    REQUIRE_FALSE(system.setCameraParent(child, child));
}

TEST_CASE("Reset invalidates previous camera state and update status", "[scenario][camera-system][lifecycle]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    system.setUpdateMethod(UpdateMethod::Late);
    const CameraId id = system.createCamera<TestCamera>("reset");
    TestCamera* camera = static_cast<TestCamera*>(system.camera(id));

    world.setFrame(UpdateClock::Late, 1);
    system.lateUpdate(0.016f);
    REQUIRE(camera->previousStateIsValid);
    REQUIRE(system.updateManager().status(camera) != nullptr);

    system.resetCameraState();
    REQUIRE_FALSE(camera->previousStateIsValid);
    REQUIRE(system.updateManager().status(camera) == nullptr);
    REQUIRE(system.updateTracker().trackedTargetCount() == 0);
}

TEST_CASE("Camera system exposes the engine adapters and config", "[scenario][camera-system][lifecycle]")
{
    FakeEntityWorld world;
    CameraSystem system(world);

    REQUIRE(&system.entityWorld() == &world);
    REQUIRE(system.physicsWorld().raycast(Ray{}, 1.0f, {}).has_value() == false);
    REQUIRE(system.splineProvider() == nullptr);
    REQUIRE(system.registry().activeCameraCount() == 0);

    system.setWorldUp({0.0f, 1.0f, 0.0f});
    REQUIRE(system.worldUp().y == 1.0f);
    system.setChannelMask(OutputChannels::Channel02);
    REQUIRE(system.channelMask() == OutputChannels::Channel02);
    system.setUpdateMethod(UpdateMethod::Manual);
    REQUIRE(system.updateMethod() == UpdateMethod::Manual);
}
