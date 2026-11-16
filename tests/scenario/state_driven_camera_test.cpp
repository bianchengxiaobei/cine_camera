#include "brain_test_support.hpp"
#include "fake_entity_world.hpp"
#include "test_camera.hpp"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/managers/state_driven_camera.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

class FakeAnimationStateProvider final : public IAnimationStateProvider
{
public:
    int currentStateHash(EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex ? currentHash : 0;
    }

    int nextStateHash(EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex && transitioning
            ? nextHash
            : 0;
    }

    float normalizedTime(EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex ? 0.5f : 0.0f;
    }

    bool isInTransition(EntityId entity, int layer) const override
    {
        return entity == target && layer == layerIndex && transitioning;
    }

    EntityId target = 0;
    int layerIndex = 0;
    int currentHash = 0;
    int nextHash = 0;
    bool transitioning = false;
};

struct StateRig
{
    FakeEntityWorld world;
    CameraSystem system{world};

    TestCamera* addCamera(
        const char* name,
        EntityId entity,
        math::Vec3 position)
    {
        world.add(entity, Transform{position, math::Quat::identity()});
        const CameraId id = system.createCamera<TestCamera>(name);
        auto* camera = static_cast<TestCamera*>(system.camera(id));
        camera->ownerEntity = entity;
        camera->standbyUpdate = StandbyUpdateMode::Always;
        return camera;
    }

    managers::StateDrivenCamera* addManager()
    {
        const CameraId id =
            system.createCamera<managers::StateDrivenCamera>("state-driven");
        auto* manager =
            static_cast<managers::StateDrivenCamera*>(system.camera(id));
        manager->standbyUpdate = StandbyUpdateMode::Always;
        return manager;
    }

    void update(
        managers::StateDrivenCamera& manager,
        float time,
        float deltaTime)
    {
        world.setTime(time);
        manager.internalUpdateCameraState(math::Vec3::up, deltaTime);
    }
};

} // namespace

TEST_CASE(
    "StateDriven defaults, instruction validation, and reset are explicit",
    "[scenario][manager][statedriven]")
{
    managers::StateDrivenCamera manager;
    REQUIRE(manager.animatedTarget == 0);
    REQUIRE(manager.layerIndex == 0);
    REQUIRE(manager.animationState == nullptr);
    REQUIRE(manager.instructions.empty());
    REQUIRE(manager.parentHashes.empty());
    REQUIRE(manager.activeInstruction() == -1);
    REQUIRE(manager.pendingInstruction() == -1);
    REQUIRE_FALSE(manager.isWaiting());

    managers::StateDrivenCamera::Instruction defaults;
    REQUIRE(defaults.fullHash == 0);
    REQUIRE(defaults.camera == nullptr);
    REQUIRE(defaults.activateAfter == 0.0f);
    REQUIRE(defaults.minDuration == 0.0f);

    defaults.activateAfter = -1.0f;
    defaults.minDuration = std::numeric_limits<float>::infinity();
    defaults.validate();
    REQUIRE(defaults.activateAfter == 0.0f);
    REQUIRE(defaults.minDuration == 0.0f);
    defaults.activateAfter = 1.5f;
    defaults.minDuration = 2.5f;
    defaults.validate();
    REQUIRE(defaults.activateAfter == 1.5f);
    REQUIRE(defaults.minDuration == 2.5f);

    manager.animatedTarget = 42;
    manager.layerIndex = 2;
    manager.instructions.push_back(defaults);
    manager.parentHashes.push_back({42, 7});
    manager.defaultBlend = {BlendStyle::Linear, 2.0f};
    manager.customBlends.addRule({"A", "B", {BlendStyle::Cut, 0.0f}});
    manager.reset();
    REQUIRE(manager.animatedTarget == 0);
    REQUIRE(manager.layerIndex == 0);
    REQUIRE(manager.animationState == nullptr);
    REQUIRE(manager.instructions.empty());
    REQUIRE(manager.parentHashes.empty());
    REQUIRE(manager.activeInstruction() == -1);
    REQUIRE(manager.pendingInstruction() == -1);
    REQUIRE_FALSE(manager.isWaiting());
    REQUIRE(manager.defaultBlend.style == BlendStyle::EaseInOut);
    REQUIRE(manager.defaultBlend.time == Catch::Approx(0.5f));
    REQUIRE(manager.customBlends.rules().empty());
    REQUIRE_FALSE(manager.previousStateIsValid);
    REQUIRE(manager.liveChild() == nullptr);
}

TEST_CASE(
    "StateDriven falls back safely without children, target, or provider",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    managers::StateDrivenCamera* manager = rig.addManager();

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->activeInstruction() == -1);

    TestCamera* a = rig.addCamera("A", 1, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 2, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    rig.update(*manager, 1.1f, 0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->activeInstruction() == -1);

    rig.world.add(10, Transform{});
    manager->animatedTarget = 10;
    rig.update(*manager, 1.2f, 0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->activeInstruction() == -1);

    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 99;
    manager->animationState = &animation;
    manager->layerIndex = -1;
    rig.update(*manager, 1.25f, 0.05f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->activeInstruction() == -1);

    manager->layerIndex = 0;
    rig.update(*manager, 1.3f, 0.05f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->activeInstruction() == -1);

    manager->instructions = {
        {99, b, 0.0f, 0.0f},
    };
    rig.update(*manager, 1.4f, 0.1f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeInstruction() == 0);
}

TEST_CASE(
    "StateDriven chooses priority then the earliest instruction",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    TestCamera* c = rig.addCamera("C", 22, {20.0f, 0.0f, 0.0f});
    TestCamera* d = rig.addCamera("D", 23, {30.0f, 0.0f, 0.0f});
    TestCamera* e = rig.addCamera("E", 24, {40.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->addChild(*c);
    manager->addChild(*d);
    manager->addChild(*e);
    a->priority = PrioritySettings(1);
    b->priority = PrioritySettings(5);
    c->priority = PrioritySettings(10);
    c->setEnabled(false);
    manager->instructions = {
        {10, a, 0.0f, 0.0f},
        {10, b, 0.0f, 0.0f},
        {10, c, 0.0f, 0.0f},
        {20, d, 0.0f, 0.0f},
        {20, e, 0.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeInstruction() == 1);

    c->setEnabled(true);
    rig.update(*manager, 2.0f, 1.0f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->pendingInstruction() == 2);
    rig.update(*manager, 2.1f, 0.1f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->activeInstruction() == 2);

    animation.currentHash = 20;
    rig.update(*manager, 2.2f, 0.1f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->pendingInstruction() == 3);
    rig.update(*manager, 2.3f, 0.1f);
    REQUIRE(manager->liveChild() == d);
    REQUIRE(manager->activeInstruction() == 3);
}

TEST_CASE(
    "StateDriven uses incoming states and ignores invalid instruction cameras",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    animation.nextHash = 20;
    animation.transitioning = true;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    TestCamera* orphan = rig.addCamera("Orphan", 22, {20.0f, 0.0f, 0.0f});
    orphan->priority = PrioritySettings(100);
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {10, a, 0.0f, 0.0f},
        {20, nullptr, 0.0f, 0.0f},
        {20, orphan, 0.0f, 0.0f},
        {20, b, 0.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeInstruction() == 3);

    animation.transitioning = false;
    animation.currentHash = 10;
    rig.update(*manager, 1.2f, 0.2f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->pendingInstruction() == 0);
    rig.update(*manager, 1.3f, 0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->activeInstruction() == 0);
}

TEST_CASE(
    "StateDriven resolves parent hashes and terminates cycles",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 30;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->parentHashes = {{30, 20}, {20, 10}};
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {10, a, 0.0f, 0.0f},
        {0, b, 0.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == a);

    animation.currentHash = 40;
    rig.update(*manager, 1.2f, 0.2f);
    REQUIRE(manager->pendingInstruction() == 1);
    rig.update(*manager, 1.3f, 0.1f);
    REQUIRE(manager->liveChild() == b);

    StateRig cycleRig;
    cycleRig.world.add(20, Transform{});
    FakeAnimationStateProvider cycleAnimation;
    cycleAnimation.target = 20;
    cycleAnimation.currentHash = 60;
    managers::StateDrivenCamera* cycleManager = cycleRig.addManager();
    cycleManager->animatedTarget = 20;
    cycleManager->animationState = &cycleAnimation;
    cycleManager->parentHashes = {{60, 70}, {70, 60}};
    TestCamera* cycleFallback =
        cycleRig.addCamera("Fallback", 30, {0.0f, 0.0f, 0.0f});
    TestCamera* defaultCamera =
        cycleRig.addCamera("Default", 31, {10.0f, 0.0f, 0.0f});
    cycleManager->addChild(*cycleFallback);
    cycleManager->addChild(*defaultCamera);
    cycleManager->instructions = {
        {0, defaultCamera, 0.0f, 0.0f},
    };

    cycleRig.world.setTime(1.0f);
    cycleManager->internalUpdateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(cycleManager->liveChild() == defaultCamera);
}

TEST_CASE(
    "StateDriven honors activateAfter with strict timing",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {10, a, 0.0f, 0.0f},
        {20, b, 0.5f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == a);
    animation.currentHash = 20;

    rig.update(*manager, 1.2f, 0.2f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->pendingInstruction() == 1);

    rig.update(*manager, 1.7f, 0.5f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->isWaiting());

    rig.update(*manager, 1.8f, 0.1f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeInstruction() == 1);
    REQUIRE_FALSE(manager->isWaiting());
}

TEST_CASE(
    "StateDriven honors the active instruction minimum duration",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {10, a, 0.0f, 1.0f},
        {20, b, 0.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    animation.currentHash = 20;
    rig.update(*manager, 1.1f, 0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->pendingInstruction() == 1);

    rig.update(*manager, 2.0f, 0.9f);
    REQUIRE(manager->liveChild() == a);
    rig.update(*manager, 2.01f, 0.01f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeInstruction() == 1);
}

TEST_CASE(
    "StateDriven cancelWait promotes a pending instruction",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {10, a, 0.0f, 0.0f},
        {20, b, 5.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    animation.currentHash = 20;
    rig.update(*manager, 1.2f, 0.2f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->pendingInstruction() == 1);
    REQUIRE(manager->isWaiting());

    rig.world.setTime(1.3f);
    manager->cancelWait();
    REQUIRE(manager->activeInstruction() == 1);
    REQUIRE(manager->pendingInstruction() == -1);
    REQUIRE_FALSE(manager->isWaiting());

    rig.update(*manager, 1.4f, 0.1f);
    REQUIRE(manager->liveChild() == b);
}

TEST_CASE(
    "StateDriven keeps unmapped states and clamps a shrinking list",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    TestCamera* c = rig.addCamera("C", 22, {20.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->addChild(*c);
    manager->instructions = {
        {20, a, 0.0f, 0.0f},
        {30, b, 0.0f, 0.0f},
        {10, c, 0.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->activeInstruction() == 2);

    animation.currentHash = 99;
    rig.update(*manager, 1.2f, 0.2f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->activeInstruction() == 2);

    manager->instructions.resize(1);
    animation.currentHash = 20;
    rig.update(*manager, 1.3f, 0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->activeInstruction() == 0);
}

TEST_CASE(
    "StateDriven ignores instruction cameras after destruction",
    "[scenario][manager][statedriven]")
{
    StateRig rig;
    rig.world.add(10, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = 10;
    animation.currentHash = 10;
    managers::StateDrivenCamera* manager = rig.addManager();
    manager->animatedTarget = 10;
    manager->animationState = &animation;

    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {10, a, 0.0f, 0.0f},
        {20, b, 0.0f, 0.0f},
    };

    rig.update(*manager, 1.0f, 0.0f);
    REQUIRE(manager->liveChild() == a);

    const CameraId aId = a->cameraId();
    rig.system.destroyCamera(aId);
    rig.update(*manager, 1.1f, 0.1f);
    REQUIRE(manager->childCameras().size() == 1);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->activeInstruction() == -1);
}

TEST_CASE(
    "StateDriven participates in brain selection and pending switches",
    "[scenario][manager][statedriven]")
{
    BrainConfig config;
    config.defaultBlend = {BlendStyle::Cut, 0.0f};
    BrainTestRig rig(config);
    const EntityId targetEntity = 200;
    rig.world.add(targetEntity, Transform{});
    FakeAnimationStateProvider animation;
    animation.target = targetEntity;
    animation.currentHash = 10;

    const CameraId aId = rig.addCamera("A", 0, 0.0f);
    const CameraId bId = rig.addCamera("B", 0, 10.0f);
    VirtualCamera& a = rig.camera(aId);
    VirtualCamera& b = rig.camera(bId);
    a.standbyUpdate = StandbyUpdateMode::Always;
    b.standbyUpdate = StandbyUpdateMode::Always;

    const CameraId managerId =
        rig.system.createCamera<managers::StateDrivenCamera>("state-driven");
    auto* manager =
        static_cast<managers::StateDrivenCamera*>(rig.system.camera(managerId));
    manager->priority = PrioritySettings(100);
    manager->standbyUpdate = StandbyUpdateMode::Always;
    manager->defaultBlend = {BlendStyle::Cut, 0.0f};
    manager->animatedTarget = targetEntity;
    manager->animationState = &animation;
    manager->addChild(a);
    manager->addChild(b);
    manager->instructions = {
        {10, &a, 0.0f, 0.0f},
        {20, &b, 0.0f, 0.0f},
    };

    rig.world.setTime(1.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == manager);
    REQUIRE(manager->liveChild() == &a);
    REQUIRE(rig.output.applied.hasCamera);

    animation.currentHash = 20;
    rig.world.setTime(2.0f);
    rig.step(1.0f);
    REQUIRE(manager->liveChild() == &a);
    REQUIRE(manager->pendingInstruction() == 1);

    rig.world.setTime(2.1f);
    rig.step(0.1f);
    REQUIRE(manager->liveChild() == &b);
    REQUIRE(manager->activeInstruction() == 1);
    REQUIRE(math::approximately(
        rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 0.0f}));
}
