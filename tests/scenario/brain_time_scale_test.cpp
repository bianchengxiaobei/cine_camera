#include "brain_test_support.hpp"
#include "test_camera.hpp"

#include <memory>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/impulse/impulse_manager.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

class ConstantSignal final : public ISignalSource6D
{
public:
    float signalDuration() const override { return 0.0f; }

    void getSignal(float, math::Vec3& pos, math::Quat& rot) const override
    {
        pos = {1.0f, 0.0f, 0.0f};
        rot = math::Quat::identity();
    }
};

BrainConfig blendConfig(bool ignoreTimeScale)
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = {BlendStyle::EaseInOut, 2.0f};
    config.ignoreTimeScale = ignoreTimeScale;
    return config;
}

struct ClockRig
{
    FakeEntityWorld world;
    CameraSystem system;
    FakeCameraOutput output;
    CameraBrain* brain = nullptr;
    TestCamera* first = nullptr;
    TestCamera* second = nullptr;
    std::uint64_t frame = 0;

    explicit ClockRig(const BrainConfig& config)
        : system(world)
    {
        system.setUnitTestMode(true);
        first = addCamera("A", 20, 0.0f);
        second = addCamera("B", 10, 10.0f);
        brain = system.brain(system.createBrain(output, config));
    }

    TestCamera* addCamera(const char* name, int priority, float x)
    {
        const EntityId entity = nextEntity_++;
        world.add(entity, Transform{{x, 0.0f, 0.0f}, math::Quat::identity()});
        const CameraId id = system.createCamera<TestCamera>(name);
        TestCamera* camera = static_cast<TestCamera*>(system.camera(id));
        camera->priority = PrioritySettings(priority);
        camera->ownerEntity = entity;
        return camera;
    }

    void step(UpdateClock clock, float deltaTime)
    {
        ++frame;
        world.setFrame(clock, frame);
        if (clock == UpdateClock::Fixed)
            brain->fixedUpdate(deltaTime);
        else
            brain->update(deltaTime);
    }

private:
    EntityId nextEntity_ = 100;
};

} // namespace

TEST_CASE("Brain blends use scaled delta by default", "[scenario][brain][time-scale]")
{
    BrainTestRig rig(blendConfig(false));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.world.setUnscaledDeltaTime(UpdateClock::Late, 1.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->timeInBlend == Catch::Approx(0.5f));
}

TEST_CASE("Brain blends use unscaled delta and apply it once", "[scenario][brain][time-scale]")
{
    const BrainConfig config = blendConfig(true);
    BrainTestRig rig(config);
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.world.setUnscaledDeltaTime(UpdateClock::Late, 0.125f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    for (int step = 0; step < 4; ++step)
        rig.step(10.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->timeInBlend == Catch::Approx(0.5f));
    REQUIRE(blend->timeInBlend != Catch::Approx(40.0f));
}

TEST_CASE("Fixed-clock Brain updates use the world unscaled fixed delta", "[scenario][brain][time-scale]")
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Fixed;
    config.blendUpdateMethod = BrainConfig::BlendUpdateMethod::Fixed;
    config.ignoreTimeScale = true;
    ClockRig rig(config);
    rig.world.setDeltaTime(UpdateClock::Fixed, 0.04f);
    rig.world.setUnscaledDeltaTime(UpdateClock::Late, 0.5f);

    rig.step(UpdateClock::Fixed, 0.9f);
    REQUIRE(rig.first->lastDeltaTime == Catch::Approx(0.04f));

    rig.world.setUnscaledDeltaTime(UpdateClock::Fixed, 0.01f);
    rig.step(UpdateClock::Fixed, 0.9f);
    REQUIRE(rig.first->lastDeltaTime == Catch::Approx(0.01f));
}

TEST_CASE("Blend-manager delta override wins over ignore time scale", "[scenario][brain][time-scale]")
{
    ClockRig rig(blendConfig(true));
    rig.world.setUnscaledDeltaTime(UpdateClock::Late, 0.1f);
    rig.step(UpdateClock::Late, 0.0f);

    rig.brain->setCameraOverride(-1, 0, rig.first, rig.second, 0.0f, 0.5f);
    rig.step(UpdateClock::Late, 0.01f);

    REQUIRE(rig.first->lastDeltaTime == Catch::Approx(0.5f));
}

TEST_CASE("Scaled brain blends and unscaled impulse clocks stay independent", "[scenario][brain][time-scale]")
{
    BrainTestRig rig(blendConfig(false));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.world.setUnscaledDeltaTime(UpdateClock::Late, 1.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->timeInBlend == Catch::Approx(0.5f));

    impulse::ImpulseManager& manager = rig.system.impulseManager();
    manager.ignoreTimeScale = true;
    rig.world.setTime(0.0f);
    rig.world.setUnscaledTime(50.0f);

    auto event = manager.newImpulseEvent();
    event->signalSource = std::make_unique<ConstantSignal>();
    event->envelope = {};
    event->envelope.sustainTime = 0.5f;
    event->channel = 1u;
    manager.addImpulseEvent(std::move(event));
    REQUIRE(manager.currentTime() == 50.0f);
    REQUIRE(manager.activeEventCount() == 1);

    rig.world.setUnscaledTime(51.0f);
    math::Vec3 pos{};
    math::Quat rot{};
    REQUIRE_FALSE(manager.getImpulseAt({}, false, 1u, pos, rot));
    REQUIRE(rig.brain->activeBlend() != nullptr);
    REQUIRE(rig.brain->activeBlend()->timeInBlend == Catch::Approx(0.5f));
}
