#include "brain_test_support.hpp"
#include "fake_entity_world.hpp"
#include "test_camera.hpp"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/managers/sequencer_camera.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;
using Catch::Approx;

namespace
{

struct SequencerRig
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

    managers::SequencerCamera* addManager()
    {
        const CameraId id =
            system.createCamera<managers::SequencerCamera>("sequencer");
        auto* manager =
            static_cast<managers::SequencerCamera*>(system.camera(id));
        manager->standbyUpdate = StandbyUpdateMode::Always;
        return manager;
    }
};

void start(SequencerRig& rig, managers::SequencerCamera& manager, float time)
{
    rig.world.setTime(time);
    manager.onTransitionFromCamera(nullptr, math::Vec3::up, 0.0f);
}

void update(
    SequencerRig& rig,
    managers::SequencerCamera& manager,
    float time,
    float deltaTime)
{
    rig.world.setTime(time);
    manager.internalUpdateCameraState(math::Vec3::up, deltaTime);
}

} // namespace

TEST_CASE(
    "Sequencer defaults, instruction validation, and reset are explicit",
    "[scenario][manager][sequencer]")
{
    managers::SequencerCamera manager;
    REQUIRE_FALSE(manager.loop);
    REQUIRE(manager.instructions.empty());
    REQUIRE(manager.currentInstruction() == 0);

    managers::SequencerCamera::Instruction defaults;
    REQUIRE(defaults.camera == nullptr);
    REQUIRE(defaults.blend.style == BlendStyle::Cut);
    REQUIRE(defaults.blend.time == 0.0f);
    REQUIRE(defaults.hold == 0.0f);

    defaults.hold = -1.0f;
    defaults.validate();
    REQUIRE(defaults.hold == 0.0f);
    defaults.hold = std::numeric_limits<float>::infinity();
    defaults.validate();
    REQUIRE(defaults.hold == 0.0f);
    defaults.hold = 2.5f;
    defaults.validate();
    REQUIRE(defaults.hold == 2.5f);

    manager.loop = true;
    manager.instructions.push_back(defaults);
    manager.defaultBlend = {BlendStyle::Linear, 2.0f};
    manager.customBlends.addRule({"A", "B", {BlendStyle::Cut, 0.0f}});
    manager.reset();
    REQUIRE_FALSE(manager.loop);
    REQUIRE(manager.instructions.empty());
    REQUIRE(manager.currentInstruction() == 0);
    REQUIRE(manager.defaultBlend.style == BlendStyle::EaseInOut);
    REQUIRE(manager.defaultBlend.time == Approx(0.5f));
    REQUIRE(manager.customBlends.rules().empty());
    REQUIRE_FALSE(manager.previousStateIsValid);
    REQUIRE(manager.liveChild() == nullptr);
}

TEST_CASE(
    "Sequencer starts on transition and honors strict hold timing",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();
    TestCamera* a = rig.addCamera("A", 1, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 2, {10.0f, 0.0f, 0.0f});
    TestCamera* c = rig.addCamera("C", 3, {20.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->addChild(*c);
    manager->instructions = {
        {a, {BlendStyle::Cut, 0.0f}, 1.0f},
        {b, {BlendStyle::Cut, 0.0f}, 1.0f},
        {c, {BlendStyle::Cut, 0.0f}, 1.0f},
    };

    update(rig, *manager, 0.5f, 0.0f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->currentInstruction() == -1);

    start(rig, *manager, 1.0f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 2.0f, 1.0f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 2.1f, 0.1f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);

    update(rig, *manager, 3.0f, 0.9f);
    REQUIRE(manager->liveChild() == b);
    update(rig, *manager, 3.1f, 0.1f);
    REQUIRE(manager->liveChild() == b);
    update(rig, *manager, 3.2f, 0.1f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->currentInstruction() == 2);
}

TEST_CASE(
    "Sequencer schedules hold plus blend-in and blends into the next shot",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();
    TestCamera* a = rig.addCamera("A", 10, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 11, {10.0f, 0.0f, 0.0f});
    TestCamera* c = rig.addCamera("C", 12, {20.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->addChild(*c);
    manager->instructions = {
        {a, {BlendStyle::Linear, 0.5f}, 0.25f},
        {b, {BlendStyle::Linear, 1.0f}, 0.25f},
        {c, {BlendStyle::Cut, 0.0f}, 0.25f},
    };

    start(rig, *manager, 0.0f);
    REQUIRE(manager->liveChild() == a);

    update(rig, *manager, 0.75f, 0.75f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 0.8f, 0.05f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);
    REQUIRE(manager->isBlending());
    REQUIRE(manager->activeBlend() != nullptr);
    REQUIRE(manager->activeBlend()->duration == Approx(1.0f));
    REQUIRE(manager->isLiveChild(a));
    REQUIRE(manager->isLiveChild(b));

    update(rig, *manager, 1.9f, 1.1f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);
    REQUIRE_FALSE(manager->isBlending());

    update(rig, *manager, 2.1f, 0.4f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->currentInstruction() == 2);
}

TEST_CASE(
    "Sequencer holds the last shot unless looping",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();
    TestCamera* a = rig.addCamera("A", 20, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 21, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {a, {BlendStyle::Cut, 0.0f}, 0.2f},
        {b, {BlendStyle::Cut, 0.0f}, 0.2f},
    };

    start(rig, *manager, 0.0f);
    update(rig, *manager, 0.3f, 0.3f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);

    update(rig, *manager, 10.0f, 9.7f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);

    manager->loop = true;
    update(rig, *manager, 10.5f, 0.5f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 10.8f, 0.3f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);
}

TEST_CASE(
    "Sequencer loops one instruction per update",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();
    TestCamera* a = rig.addCamera("A", 30, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 31, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->loop = true;
    manager->instructions = {
        {a, {BlendStyle::Cut, 0.0f}, 0.0f},
        {b, {BlendStyle::Cut, 0.0f}, 0.0f},
    };

    start(rig, *manager, 0.0f);
    REQUIRE(manager->liveChild() == a);

    update(rig, *manager, 10.0f, 10.0f);
    REQUIRE(manager->liveChild() == b);
    REQUIRE(manager->currentInstruction() == 1);

    update(rig, *manager, 10.0f, 0.0f);
    REQUIRE(manager->liveChild() == b);

    update(rig, *manager, 10.1f, 0.1f);
    REQUIRE(manager->liveChild() == a);
}

TEST_CASE(
    "Sequencer restarts on transition and negative delta time",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();
    TestCamera* a = rig.addCamera("A", 40, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 41, {10.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->instructions = {
        {a, {BlendStyle::Cut, 0.0f}, 0.5f},
        {b, {BlendStyle::Cut, 0.0f}, 0.5f},
    };

    start(rig, *manager, 0.0f);
    update(rig, *manager, 0.6f, 0.6f);
    REQUIRE(manager->liveChild() == b);

    start(rig, *manager, 1.0f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 1.6f, 0.6f);
    REQUIRE(manager->liveChild() == b);

    update(rig, *manager, 1.7f, -0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 2.3f, 0.6f);
    REQUIRE(manager->liveChild() == b);
}

TEST_CASE(
    "Sequencer safely handles missing, null, and disabled instructions",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();

    start(rig, *manager, 0.0f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->currentInstruction() == -1);

    TestCamera* a = rig.addCamera("A", 50, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 51, {10.0f, 0.0f, 0.0f});
    TestCamera* orphan = rig.addCamera("Orphan", 52, {20.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    a->setEnabled(false);
    manager->instructions = {
        {nullptr, {BlendStyle::Cut, 0.0f}, 0.2f},
        {a, {BlendStyle::Cut, 0.0f}, 0.2f},
        {orphan, {BlendStyle::Cut, 0.0f}, 0.2f},
    };

    start(rig, *manager, 1.0f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->currentInstruction() == 0);

    update(rig, *manager, 1.3f, 0.3f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(a->isEnabled());
    REQUIRE(manager->currentInstruction() == 1);

    update(rig, *manager, 1.6f, 0.3f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->currentInstruction() == 2);

    manager->instructions.clear();
    update(rig, *manager, 1.9f, 0.3f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->currentInstruction() == -1);
}

TEST_CASE(
    "Sequencer clamps and resets when its instruction list changes",
    "[scenario][manager][sequencer]")
{
    SequencerRig rig;
    managers::SequencerCamera* manager = rig.addManager();
    TestCamera* a = rig.addCamera("A", 60, {0.0f, 0.0f, 0.0f});
    TestCamera* b = rig.addCamera("B", 61, {10.0f, 0.0f, 0.0f});
    TestCamera* c = rig.addCamera("C", 62, {20.0f, 0.0f, 0.0f});
    manager->addChild(*a);
    manager->addChild(*b);
    manager->addChild(*c);
    manager->instructions = {
        {a, {BlendStyle::Cut, 0.0f}, 0.0f},
        {b, {BlendStyle::Cut, 0.0f}, 0.0f},
        {c, {BlendStyle::Cut, 0.0f}, 0.0f},
    };

    start(rig, *manager, 0.0f);
    update(rig, *manager, 0.1f, 0.1f);
    update(rig, *manager, 0.2f, 0.1f);
    REQUIRE(manager->liveChild() == c);
    REQUIRE(manager->currentInstruction() == 2);

    manager->instructions.resize(1);
    update(rig, *manager, 0.3f, 0.1f);
    REQUIRE(manager->liveChild() == a);
    REQUIRE(manager->currentInstruction() == 0);

    manager->instructions.clear();
    update(rig, *manager, 0.4f, 0.1f);
    REQUIRE(manager->liveChild() == nullptr);
    REQUIRE(manager->currentInstruction() == -1);
}

TEST_CASE(
    "Sequencer starts and advances when selected through the brain",
    "[scenario][manager][sequencer]")
{
    BrainConfig config;
    config.defaultBlend = {BlendStyle::Cut, 0.0f};
    BrainTestRig rig(config);
    const CameraId aId = rig.addCamera("A", 0, 0.0f);
    const CameraId bId = rig.addCamera("B", 0, 10.0f);
    VirtualCamera& a = rig.camera(aId);
    VirtualCamera& b = rig.camera(bId);
    a.standbyUpdate = StandbyUpdateMode::Always;
    b.standbyUpdate = StandbyUpdateMode::Always;

    const CameraId managerId =
        rig.system.createCamera<managers::SequencerCamera>("sequencer");
    auto* manager =
        static_cast<managers::SequencerCamera*>(rig.system.camera(managerId));
    manager->priority = PrioritySettings(100);
    manager->standbyUpdate = StandbyUpdateMode::Always;
    manager->addChild(a);
    manager->addChild(b);
    manager->instructions = {
        {&a, {BlendStyle::Cut, 0.0f}, 0.2f},
        {&b, {BlendStyle::Cut, 0.0f}, 0.2f},
    };

    rig.world.setTime(0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == manager);
    REQUIRE(manager->liveChild() == &a);
    REQUIRE(rig.output.applied.hasCamera);

    rig.world.setTime(0.3f);
    rig.step(0.3f);
    REQUIRE(manager->liveChild() == &b);
    REQUIRE(manager->currentInstruction() == 1);
    REQUIRE(math::approximately(
        rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 0.0f}));
}
