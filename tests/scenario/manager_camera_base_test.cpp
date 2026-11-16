#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"
#include "pipeline_test_components.hpp"
#include "test_camera.hpp"

#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/camera/virtual_camera_manager_base.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

class FixedManager final : public VirtualCameraManagerBase
{
public:
    FixedManager()
        : VirtualCameraManagerBase("manager")
    {
    }

    void choose(VirtualCamera* camera) noexcept { chosen = camera; }

    int chooseCount = 0;
    math::Vec3 lastWorldUp{};
    float lastDeltaTime = 0.0f;
    VirtualCamera* chosen = nullptr;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override
    {
        ++chooseCount;
        lastWorldUp = worldUp;
        lastDeltaTime = deltaTime;
        return chosen;
    }
};

class StateProbeCamera final : public VirtualCamera
{
public:
    explicit StateProbeCamera(std::string cameraName)
        : VirtualCamera(std::move(cameraName))
    {
    }

    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override
    {
        sawInvalidState = sawInvalidState || !previousStateIsValid;
        VirtualCamera::internalUpdateCameraState(worldUp, deltaTime);
    }

    bool sawInvalidState = false;
};

class TransitionProbeCamera final : public VirtualCamera
{
public:
    explicit TransitionProbeCamera(std::string cameraName)
        : VirtualCamera(std::move(cameraName))
    {
    }

    void onCameraActivated(const ActivationEventParams& event) override
    {
        ++activationCount;
        lastOutgoing = event.outgoingCamera;
        lastIncoming = event.incomingCamera;
        VirtualCamera::onCameraActivated(event);
    }

    int activationCount = 0;
    const ICinemachineCamera* lastOutgoing = nullptr;
    const ICinemachineCamera* lastIncoming = nullptr;
};

struct CameraRig
{
    FakeEntityWorld world;
    CameraSystem system{world};

    CameraId addCamera(
        const char* name,
        EntityId entity,
        math::Vec3 position = {})
    {
        world.add(entity, Transform{position, math::Quat::identity()});
        const CameraId id = system.createCamera<TestCamera>(name);
        system.camera(id)->ownerEntity = entity;
        return id;
    }
};

VirtualCamera* camera(CameraSystem& system, CameraId id)
{
    return system.camera(id);
}

} // namespace

TEST_CASE("Manager children are explicit, safe, and cycle checked", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId otherId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.addCamera("child", 1, {1.0f, 0.0f, 0.0f});
    const CameraId secondChildId = rig.addCamera("second", 2, {2.0f, 0.0f, 0.0f});

    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    auto* other = static_cast<FixedManager*>(camera(rig.system, otherId));
    VirtualCamera* child = camera(rig.system, childId);
    VirtualCamera* secondChild = camera(rig.system, secondChildId);

    REQUIRE(manager->childCameras().empty());
    REQUIRE(manager->addChild(*child));
    REQUIRE(manager->childCameras().size() == 1);
    REQUIRE(child->parentVirtualCamera() == manager);
    REQUIRE_FALSE(manager->addChild(*child));
    REQUIRE_FALSE(manager->addChild(*manager));

    REQUIRE(other->addChild(*manager));
    REQUIRE_FALSE(manager->addChild(*other));
    REQUIRE_FALSE(manager->addChild(*child));
    REQUIRE(manager->childCameras().size() == 1);

    REQUIRE(manager->removeChild(*child));
    REQUIRE_FALSE(manager->removeChild(*child));
    REQUIRE(manager->childCameras().empty());
    REQUIRE(child->parentVirtualCamera() == nullptr);

    REQUIRE(manager->addChild(*child));
    rig.system.destroyCamera(childId);
    REQUIRE(manager->childCameras().empty());

    REQUIRE(other->addChild(*secondChild));
    REQUIRE_FALSE(manager->addChild(*secondChild));

    REQUIRE(rig.system.setCameraParent(secondChildId, managerId));
    REQUIRE(manager->childCameras().size() == 1);
    REQUIRE(secondChild->parentVirtualCamera() == manager);
    REQUIRE(rig.system.setCameraParent(secondChildId, 0));
    REQUIRE(manager->childCameras().empty());
    REQUIRE(secondChild->parentVirtualCamera() == nullptr);
}

TEST_CASE("Manager selects a child, blends with defaults, and completes", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId aId = rig.addCamera("A", 10, {0.0f, 0.0f, 0.0f});
    const CameraId bId = rig.addCamera("B", 11, {10.0f, 0.0f, 0.0f});

    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    VirtualCamera* a = camera(rig.system, aId);
    VirtualCamera* b = camera(rig.system, bId);

    REQUIRE(manager->addChild(*a));
    REQUIRE(manager->addChild(*b));

    manager->choose(a);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(manager->chooseCount == 1);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->previousStateIsValid);
    REQUIRE_FALSE(manager->isBlending());
    REQUIRE(manager->activeBlend() == nullptr);
    REQUIRE(math::approximately(manager->state().rawPosition, {0.0f, 0.0f, 0.0f}));

    manager->choose(b);
    manager->internalUpdateCameraState(math::Vec3::up, 0.25f);

    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->isBlending());
    REQUIRE(manager->activeBlend() != nullptr);
    REQUIRE(manager->activeBlend()->duration == 0.5f);
    REQUIRE(manager->isLiveChild(a));
    REQUIRE(manager->isLiveChild(b));
    REQUIRE_FALSE(manager->isLiveChild(a, true));
    REQUIRE(manager->isLiveChild(b, true));
    REQUIRE(manager->state().rawPosition.x > 0.0f);
    REQUIRE(manager->state().rawPosition.x < 10.0f);

    manager->internalUpdateCameraState(math::Vec3::up, 0.25f);

    REQUIRE_FALSE(manager->isBlending());
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeBlend() == nullptr);
    REQUIRE(math::approximately(manager->state().rawPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("Manager custom blends override the default blend", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId aId = rig.addCamera("A", 10, {0.0f, 0.0f, 0.0f});
    const CameraId bId = rig.addCamera("B", 11, {10.0f, 0.0f, 0.0f});

    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    VirtualCamera* a = camera(rig.system, aId);
    VirtualCamera* b = camera(rig.system, bId);
    manager->addChild(*a);
    manager->addChild(*b);
    manager->customBlends.addRule(
        BlendRule{std::string(a->name()), std::string(b->name()),
                  BlendDefinition{BlendStyle::Linear, 1.25f}});

    manager->choose(a);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    manager->choose(b);
    manager->internalUpdateCameraState(math::Vec3::up, 0.25f);

    REQUIRE(manager->isBlending());
    REQUIRE(manager->activeBlend() != nullptr);
    REQUIRE(manager->activeBlend()->duration == 1.25f);
    REQUIRE(manager->activeBlend()->timeInBlend == 0.25f);
}

TEST_CASE("Manager invalidates cache, propagates previous state, and resets live", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.system.createCamera<StateProbeCamera>("probe");
    const CameraId otherId = rig.addCamera("other", 3, {5.0f, 0.0f, 0.0f});

    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    auto* probe = static_cast<StateProbeCamera*>(camera(rig.system, childId));
    VirtualCamera* other = camera(rig.system, otherId);
    manager->addChild(*probe);
    manager->addChild(*other);

    manager->choose(probe);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->previousStateIsValid);
    REQUIRE(probe->previousStateIsValid);
    REQUIRE(probe->sawInvalidState);
    REQUIRE(manager->liveChild() == probe);

    manager->choose(other);
    other->previousStateIsValid = false;
    manager->internalUpdateCameraState(math::Vec3::up, 0.1f);
    REQUIRE(manager->liveChild() == other);

    manager->invalidateCameraCache();
    REQUIRE_FALSE(manager->previousStateIsValid);
    REQUIRE_FALSE(probe->previousStateIsValid);
    REQUIRE_FALSE(other->previousStateIsValid);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->activeBlend() == nullptr);
    REQUIRE_FALSE(manager->isBlending());
    REQUIRE(manager->childCameras().size() == 2);

    manager->choose(probe);
    probe->previousStateIsValid = false;
    probe->sawInvalidState = false;
    manager->internalUpdateCameraState(math::Vec3::up, 0.05f);
    REQUIRE(probe->sawInvalidState);
    REQUIRE(manager->previousStateIsValid);
}

TEST_CASE("Manager default targets are inherited by children", "[scenario][manager]")
{
    CameraRig rig;
    rig.world.add(1, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity()});
    rig.world.add(2, Transform{{4.0f, 5.0f, 6.0f}, math::Quat::identity()});
    rig.world.add(3, Transform{{7.0f, 8.0f, 9.0f}, math::Quat::identity()});

    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.system.createCamera<TestCamera>("target-child");
    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    auto* child = static_cast<TestCamera*>(camera(rig.system, childId));
    child->ownerEntity = 1;
    manager->addChild(*child);

    manager->defaultTarget.enabled = true;
    manager->defaultTarget.target.tracking = TargetRef::entity(2);
    REQUIRE(child->effectiveFollowTarget() == TargetRef::entity(2));
    REQUIRE(child->effectiveLookAtTarget() == TargetRef::entity(2));

    child->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(math::approximately(child->state().referenceLookAt, {4.0f, 5.0f, 6.0f}));

    manager->defaultTarget.target.customLookAt = true;
    manager->defaultTarget.target.lookAt = TargetRef::entity(3);
    REQUIRE(child->effectiveLookAtTarget() == TargetRef::entity(3));
    REQUIRE(child->effectiveFollowTarget() == TargetRef::entity(2));

    manager->defaultTarget.enabled = false;
    REQUIRE_FALSE(child->effectiveFollowTarget().isValid());
    REQUIRE_FALSE(child->effectiveLookAtTarget().isValid());
}

TEST_CASE("Manager transition notifies the live child", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.system.createCamera<TransitionProbeCamera>("transition-child");
    const CameraId fromId = rig.addCamera("from", 4, {1.0f, 0.0f, 0.0f});

    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    auto* child = static_cast<TransitionProbeCamera*>(camera(rig.system, childId));
    VirtualCamera* from = camera(rig.system, fromId);
    manager->addChild(*child);
    manager->choose(child);

    manager->onTransitionFromCamera(from, math::Vec3::up, 0.016f);

    REQUIRE(manager->previousStateIsValid);
    REQUIRE(manager->liveChild() == child);
    REQUIRE(child->activationCount >= 1);
    REQUIRE(child->lastOutgoing == from);
    REQUIRE(child->lastIncoming == child);
}

TEST_CASE("Manager propagates force position and target warps", "[scenario][manager]")
{
    CameraRig rig;
    rig.world.add(2, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity()});
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.system.createCamera<TestCamera>("force-child");
    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    auto* child = static_cast<TestCamera*>(camera(rig.system, childId));
    child->ownerEntity = 1;
    child->targets.tracking = TargetRef::entity(2);

    RecordingComponent& body = child->setBody<RecordingComponent>(Stage::Body, "body");
    auto extension = std::make_unique<RecordingExtension>("extension");
    RecordingExtension* extensionPtr = extension.get();
    child->addExtension(std::move(extension));
    manager->addChild(*child);
    manager->choose(child);

    manager->forceCameraPosition({10.0f, 20.0f, 30.0f}, math::Quat::identity());
    REQUIRE(math::approximately(manager->state().rawPosition, {10.0f, 20.0f, 30.0f}));
    REQUIRE(math::approximately(child->state().rawPosition, {10.0f, 20.0f, 30.0f}));
    REQUIRE(math::approximately(body.forcedPosition, {10.0f, 20.0f, 30.0f}));
    REQUIRE(math::approximately(extensionPtr->forcedPosition, {10.0f, 20.0f, 30.0f}));
    REQUIRE(child->previousStateIsValid);

    child->internalUpdateCameraState(math::Vec3::up, 0.0f);
    manager->onTargetObjectWarped(2, {1.0f, 1.0f, 1.0f});

    REQUIRE(body.warpedTarget == 2);
    REQUIRE(math::approximately(body.warpDelta, {1.0f, 1.0f, 1.0f}));
    REQUIRE(extensionPtr->warpedTarget == 2);
}

TEST_CASE("Nested managers update leaf first and resolve live children", "[scenario][manager]")
{
    CameraRig rig;
    rig.world.setDefaultWritable(true);
    rig.world.add(10, Transform{{2.0f, 0.0f, 0.0f}, math::Quat::identity()});

    const CameraId outerId = rig.system.createCamera<FixedManager>();
    const CameraId innerId = rig.system.createCamera<FixedManager>();
    const CameraId leafId = rig.system.createCamera<TestCamera>("leaf");
    auto* outer = static_cast<FixedManager*>(camera(rig.system, outerId));
    auto* inner = static_cast<FixedManager*>(camera(rig.system, innerId));
    auto* leaf = static_cast<TestCamera*>(camera(rig.system, leafId));
    leaf->ownerEntity = 10;

    outer->standbyUpdate = StandbyUpdateMode::Always;
    inner->standbyUpdate = StandbyUpdateMode::Always;
    leaf->standbyUpdate = StandbyUpdateMode::Always;

    REQUIRE(inner->addChild(*leaf));
    REQUIRE(outer->addChild(*inner));
    outer->choose(inner);
    inner->choose(leaf);

    rig.system.setUpdateMethod(UpdateMethod::Late);
    rig.world.setFrame(UpdateClock::Late, 1);
    rig.system.lateUpdate(0.016f);

    REQUIRE(leaf->updateCount > 0);
    REQUIRE(outer->liveChild() == inner);
    REQUIRE(inner->liveChild() == leaf);
    REQUIRE(outer->isLiveChild(inner));
    REQUIRE(outer->isLiveChild(leaf));
    REQUIRE(outer->isLiveChild(leaf, true));
    REQUIRE(math::approximately(outer->state().rawPosition, {2.0f, 0.0f, 0.0f}));

    outer->invalidateCameraCache();
    REQUIRE_FALSE(outer->previousStateIsValid);
    REQUIRE_FALSE(inner->previousStateIsValid);
    REQUIRE_FALSE(leaf->previousStateIsValid);
}

TEST_CASE("Manager finalize invokes its extensions with the manager state", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.addCamera("finalize-child", 1, {3.0f, 0.0f, 0.0f});
    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    VirtualCamera* child = camera(rig.system, childId);
    manager->addChild(*child);
    manager->choose(child);

    auto extension = std::make_unique<RecordingExtension>("manager-extension");
    RecordingExtension* extensionPtr = extension.get();
    manager->addExtension(std::move(extension));

    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);

    REQUIRE(extensionPtr->postCount > 0);
    REQUIRE(extensionPtr->lastStage == Stage::Finalize);
    REQUIRE(extensionPtr->callbackCamera == manager);
    REQUIRE(math::approximately(manager->state().rawPosition, {3.0f, 0.0f, 0.0f}));
}

TEST_CASE("Manager participates in brain output as one virtual camera", "[scenario][manager]")
{
    CameraRig rig;
    FakeCameraOutput output;
    rig.system.createBrain(output, BrainConfig{});

    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.addCamera("brain-child", 10, {2.0f, 0.0f, 0.0f});
    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    VirtualCamera* child = camera(rig.system, childId);
    manager->standbyUpdate = StandbyUpdateMode::Always;
    child->standbyUpdate = StandbyUpdateMode::Always;
    manager->priority = PrioritySettings(5);
    manager->addChild(*child);
    manager->choose(child);

    rig.world.setFrame(UpdateClock::Late, 1);
    rig.system.lateUpdate(0.016f);

    REQUIRE(rig.system.activeCameraCount() == 2);
    REQUIRE(rig.system.brain(1) != nullptr);
    REQUIRE(rig.system.brain(1)->activeVirtualCamera() == manager);
    REQUIRE(math::approximately(manager->state().rawPosition, {2.0f, 0.0f, 0.0f}));
    REQUIRE(output.applyCount == 1);
    REQUIRE(output.applied.hasCamera);
    REQUIRE(math::approximately(output.applied.state.rawPosition, {2.0f, 0.0f, 0.0f}));
}

TEST_CASE("Manager enables a disabled selected child during update", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.addCamera("disabled-child", 10, {4.0f, 0.0f, 0.0f});
    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    VirtualCamera* child = camera(rig.system, childId);

    child->setEnabled(false);
    manager->standbyUpdate = StandbyUpdateMode::Always;
    manager->addChild(*child);
    manager->choose(child);

    rig.system.setUpdateMethod(UpdateMethod::Late);
    rig.world.setFrame(UpdateClock::Late, 1);
    rig.system.lateUpdate(0.016f);

    REQUIRE(child->isEnabled());
    REQUIRE(manager->liveChild() == child);
    REQUIRE(manager->previousStateIsValid);
    REQUIRE(math::approximately(manager->state().rawPosition, {4.0f, 0.0f, 0.0f}));
}

TEST_CASE("Destroying a manager safely detaches its children", "[scenario][manager]")
{
    CameraRig rig;
    const CameraId managerId = rig.system.createCamera<FixedManager>();
    const CameraId childId = rig.addCamera("detached-child", 10, {1.0f, 0.0f, 0.0f});
    auto* manager = static_cast<FixedManager*>(camera(rig.system, managerId));
    VirtualCamera* child = camera(rig.system, childId);

    manager->addChild(*child);
    manager->choose(child);
    manager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(manager->liveChild() == child);
    REQUIRE(child->isLive());

    rig.system.destroyCamera(managerId);
    REQUIRE(rig.system.camera(managerId) == nullptr);
    REQUIRE(child->parentVirtualCamera() == nullptr);
    REQUIRE_FALSE(child->isLive());

    rig.system.setUpdateMethod(UpdateMethod::Late);
    rig.world.setFrame(UpdateClock::Late, 1);
    rig.system.lateUpdate(0.016f);
    REQUIRE(child->previousStateIsValid);
}
