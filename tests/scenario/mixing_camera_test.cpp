#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"
#include "pipeline_test_components.hpp"
#include "test_camera.hpp"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/managers/mixing_camera.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;
using Catch::Approx;

namespace
{

struct MixRig
{
    FakeEntityWorld world;
    CameraSystem system{world};

    TestCamera* addChild(
        const char* name,
        EntityId entity,
        math::Vec3 position,
        float fieldOfView = 40.0f)
    {
        world.add(entity, Transform{position, math::Quat::identity()});
        const CameraId id = system.createCamera<TestCamera>(name);
        auto* camera = static_cast<TestCamera*>(system.camera(id));
        camera->ownerEntity = entity;
        camera->lens.fieldOfView = fieldOfView;
        camera->standbyUpdate = StandbyUpdateMode::Always;
        return camera;
    }

    managers::MixingCamera* addManager(EntityId owner = 0)
    {
        const CameraId id =
            system.createCamera<managers::MixingCamera>("mix");
        auto* manager =
            static_cast<managers::MixingCamera*>(system.camera(id));
        manager->ownerEntity = owner;
        manager->standbyUpdate = StandbyUpdateMode::Always;
        return manager;
    }
};

} // namespace

TEST_CASE("Mixing camera weights have fixed slots and explicit reset", "[scenario][manager][mixing]")
{
    managers::MixingCamera manager;
    REQUIRE(managers::MixingCamera::MaxCameras == 8);
    for (float weight : manager.weights)
        REQUIRE(weight == Approx(0.5f));

    REQUIRE(manager.setWeight(0, 2.0f));
    REQUIRE(manager.setWeight(7, 3.0f));
    REQUIRE_FALSE(manager.setWeight(-1, 1.0f));
    REQUIRE_FALSE(manager.setWeight(8, 1.0f));
    REQUIRE(manager.getWeight(0) == 2.0f);
    REQUIRE(manager.getWeight(7) == 3.0f);
    REQUIRE(manager.getWeight(8) == 0.0f);

    VirtualCamera a("a");
    VirtualCamera b("b");
    VirtualCamera outsider("outsider");
    REQUIRE(manager.addChild(a));
    REQUIRE(manager.addChild(b));
    REQUIRE(manager.setWeight(a, 4.0f));
    REQUIRE(manager.getWeight(a) == 4.0f);
    REQUIRE_FALSE(manager.setWeight(outsider, 1.0f));
    REQUIRE(manager.getWeight(outsider) == 0.0f);

    manager.setWeight(1, -3.0f);
    manager.setWeight(2, std::numeric_limits<float>::infinity());
    manager.validate();
    REQUIRE(manager.weights[1] == 0.0f);
    REQUIRE(manager.weights[2] == 0.0f);

    manager.reset();
    REQUIRE(manager.weights[0] == 1.0f);
    for (std::size_t index = 1; index < manager.weights.size(); ++index)
        REQUIRE(manager.weights[index] == 0.0f);
    REQUIRE(manager.liveChild() == nullptr);
    REQUIRE(manager.liveChildPercent() == 0.0f);
    REQUIRE_FALSE(manager.previousStateIsValid);
}

TEST_CASE("Mixing camera performs weighted state averaging", "[scenario][manager][mixing]")
{
    MixRig rig;
    managers::MixingCamera* manager = rig.addManager();
    TestCamera* a = rig.addChild("A", 10, {0.0f, 0.0f, 0.0f}, 30.0f);
    TestCamera* b = rig.addChild("B", 11, {10.0f, 4.0f, 0.0f}, 70.0f);
    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));
    manager->reset();
    manager->setWeight(0, 1.0f);
    manager->setWeight(1, 3.0f);

    a->updateCameraState(math::Vec3::up, 0.0f);
    b->updateCameraState(math::Vec3::up, 0.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(math::approximately(manager->state().rawPosition, {7.5f, 3.0f, 0.0f}));
    REQUIRE(manager->state().lens.fieldOfView == Approx(60.0f));
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->liveChildPercent() == Approx(75.0f));
    REQUIRE(manager->isLiveChild(a));
    REQUIRE(manager->isLiveChild(b));
    REQUIRE(manager->isLiveChild(a, true) == false);
    REQUIRE(manager->isLiveChild(b, true));
    REQUIRE(a->isLive());
    REQUIRE(b->isLive());
    REQUIRE_FALSE(manager->isBlending());
    REQUIRE(manager->activeBlend() == nullptr);
    REQUIRE(manager->previousStateIsValid);
}

TEST_CASE("Mixing camera ignores zero disabled and negative weights", "[scenario][manager][mixing]")
{
    MixRig rig;
    managers::MixingCamera* manager = rig.addManager();
    TestCamera* a = rig.addChild("A", 20, {2.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addChild("B", 21, {8.0f, 0.0f, 0.0f});
    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));
    manager->reset();
    manager->setWeight(0, 0.0f);
    manager->setWeight(1, 0.0f);

    a->updateCameraState(math::Vec3::up, 0.0f);
    b->updateCameraState(math::Vec3::up, 0.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->liveChildPercent() == 0.0f);
    REQUIRE(math::approximately(manager->state().rawPosition, math::Vec3::zero));

    manager->setWeight(0, 1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(math::approximately(manager->state().rawPosition, {2.0f, 0.0f, 0.0f}));
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->liveChildPercent() == Approx(100.0f));

    manager->setWeight(0, 0.0f);
    manager->setWeight(1, -2.0f);
    b->setEnabled(false);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(math::approximately(manager->state().rawPosition, {2.0f, 0.0f, 0.0f}));
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE_FALSE(manager->isLiveChild(b));
    REQUIRE_FALSE(a->isLive());
    REQUIRE_FALSE(b->isLive());
}

TEST_CASE("Mixing camera contributes only the first eight children", "[scenario][manager][mixing]")
{
    MixRig rig;
    managers::MixingCamera* manager = rig.addManager();
    std::vector<TestCamera*> children;
    for (int index = 0; index < 9; ++index)
    {
        children.push_back(rig.addChild(
            "child",
            static_cast<EntityId>(30 + index),
            {static_cast<float>(index), 0.0f, 0.0f}));
        REQUIRE(manager->addChild(*children.back()));
    }

    for (std::size_t index = 0; index < 8; ++index)
    {
        manager->setWeight(static_cast<int>(index), 1.0f);
        children[index]->updateCameraState(math::Vec3::up, 0.0f);
    }
    children.back()->updateCameraState(math::Vec3::up, 0.0f);

    REQUIRE_FALSE(manager->setWeight(*children.back(), 1.0f));
    REQUIRE(manager->getWeight(*children.back()) == 0.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(math::approximately(manager->state().rawPosition, {3.5f, 0.0f, 0.0f}));
    REQUIRE(manager->liveChild() == children.front());
    REQUIRE(manager->liveChildPercent() == Approx(12.5f));
    REQUIRE_FALSE(children.back()->isLive());
}

TEST_CASE("Mixing camera live flags track weighted children", "[scenario][manager][mixing]")
{
    MixRig rig;
    managers::MixingCamera* manager = rig.addManager();
    TestCamera* a = rig.addChild("A", 40, {1.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addChild("B", 41, {9.0f, 0.0f, 0.0f});
    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));
    manager->reset();
    manager->setWeight(0, 1.0f);
    manager->setWeight(1, 0.0f);
    a->updateCameraState(math::Vec3::up, 0.0f);
    b->updateCameraState(math::Vec3::up, 0.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(a->isLive());
    REQUIRE_FALSE(b->isLive());

    manager->setWeight(0, 0.0f);
    manager->setWeight(1, 1.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE_FALSE(a->isLive());
    REQUIRE(b->isLive());
    REQUIRE(manager->liveChild() == b);
    REQUIRE(math::approximately(manager->state().rawPosition, {9.0f, 0.0f, 0.0f}));
}

TEST_CASE("Mixing camera finalizes and pushes the mixed owner state", "[scenario][manager][mixing]")
{
    MixRig rig;
    const EntityId owner = 50;
    rig.world.add(owner, Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity()});
    managers::MixingCamera* manager = rig.addManager(owner);
    TestCamera* a = rig.addChild("A", 51, {2.0f, 1.0f, 0.0f});
    TestCamera* b = rig.addChild("B", 52, {8.0f, 3.0f, 0.0f});
    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));
    manager->reset();
    manager->setWeight(0, 1.0f);
    manager->setWeight(1, 1.0f);

    auto extension = std::make_unique<RecordingExtension>("mix-extension");
    RecordingExtension* extensionPtr = extension.get();
    manager->addExtension(std::move(extension));

    a->updateCameraState(math::Vec3::up, 0.0f);
    b->updateCameraState(math::Vec3::up, 0.0f);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(extensionPtr->postCount > 0);
    REQUIRE(extensionPtr->lastStage == Stage::Finalize);
    REQUIRE(extensionPtr->callbackCamera == manager);
    REQUIRE(math::approximately(manager->state().rawPosition, {5.0f, 2.0f, 0.0f}));
    REQUIRE(math::approximately(
        rig.world.transform(owner).position,
        {5.0f, 2.0f, 0.0f}));
}

TEST_CASE("Mixing camera forwards transitions to every child", "[scenario][manager][mixing]")
{
    MixRig rig;
    managers::MixingCamera* manager = rig.addManager();
    TestCamera* a = rig.addChild("A", 60, {1.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addChild("B", 61, {2.0f, 0.0f, 0.0f});
    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));
    auto& bodyA = a->setBody<RecordingComponent>(Stage::Body, "body-a");
    auto& bodyB = b->setBody<RecordingComponent>(Stage::Body, "body-b");
    manager->setWeight(0, 1.0f);
    manager->setWeight(1, 1.0f);

    manager->onTransitionFromCamera(nullptr, math::Vec3::up, 0.016f);

    REQUIRE(bodyA.transitionCount == 1);
    REQUIRE(bodyB.transitionCount == 1);
}

TEST_CASE("Mixing camera participates as one camera in the brain", "[scenario][manager][mixing]")
{
    MixRig rig;
    FakeCameraOutput output;
    rig.system.createBrain(output, BrainConfig{});
    managers::MixingCamera* manager = rig.addManager();
    TestCamera* a = rig.addChild("A", 70, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addChild("B", 71, {10.0f, 0.0f, 0.0f});
    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));
    manager->reset();
    manager->setWeight(0, 1.0f);
    manager->setWeight(1, 1.0f);
    manager->priority = PrioritySettings(5);

    rig.system.setUpdateMethod(UpdateMethod::Late);
    rig.world.setFrame(UpdateClock::Late, 1);
    rig.system.lateUpdate(0.016f);

    REQUIRE(rig.system.brain(1) != nullptr);
    REQUIRE(rig.system.brain(1)->activeVirtualCamera() == manager);
    REQUIRE(rig.system.activeCameraCount() == 3);
    REQUIRE(output.applyCount == 1);
    REQUIRE(math::approximately(
        output.applied.state.rawPosition,
        {5.0f, 0.0f, 0.0f}));
    REQUIRE(a->updateCount > 0);
    REQUIRE(b->updateCount > 0);
}
