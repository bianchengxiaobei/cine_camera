#include "brain_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("Brain selects the highest-priority channel-compatible camera", "[scenario][brain][priority]")
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Cut, 0.0f};
    BrainTestRig rig(config);
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 15, 10.0f);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(rig.output.applied.hasCamera);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));
    REQUIRE_FALSE(rig.brain->isBlending());

    rig.camera(b).outputChannel = OutputChannels::Channel02;
    rig.camera(a).outputChannel = OutputChannels::Channel01;
    rig.brain->config().channelMask = OutputChannels::Channel01;
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Brain reports no active camera and preserves output hints", "[scenario][brain][priority]")
{
    BrainTestRig rig;
    rig.output.setOutputLens([] {
        LensSettings lens = LensSettings::Default;
        lens.fieldOfView = 42.0f;
        return lens;
    }());
    rig.output.setAspect(1.5f);

    rig.step(0.0f);

    REQUIRE(rig.brain->activeVirtualCamera() == nullptr);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE_FALSE(rig.output.applied.hasCamera);
    REQUIRE(rig.output.applied.aspect == 1.5f);
    REQUIRE(rig.output.applied.state.lens.fieldOfView == 42.0f);
    REQUIRE(hasBlendHint(rig.output.applied.state.blendHint, BlendHint::NoTransform));
    REQUIRE(hasBlendHint(rig.output.applied.state.blendHint, BlendHint::NoLens));
}

TEST_CASE("CameraSystem drives its registered brains", "[scenario][brain][priority]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    rig.system.setUpdateMethod(UpdateMethod::Late);
    rig.world.setFrame(UpdateClock::Late, 1);

    rig.system.lateUpdate(0.0f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));
    REQUIRE(rig.output.applied.hasCamera);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("The active camera updates every frame while in standby", "[scenario][brain][priority]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    (void)a;
    rig.step(0.0f);

    rig.world.set(rig.camera(b).ownerEntity, Transform{{20.0f, 0.0f, 0.0f}, math::Quat::identity()});
    rig.step(0.0f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {20.0f, 0.0f, 0.0f}));
}

TEST_CASE("Destroying the active camera resets the brain safely", "[scenario][brain][priority]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.destroyCamera(a);
    rig.step(0.0f);

    REQUIRE(rig.brain->activeVirtualCamera() == nullptr);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE_FALSE(rig.output.applied.hasCamera);
}

TEST_CASE("Brain follows a changed priority immediately", "[scenario][brain][priority]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 5, 10.0f);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.camera(b).priority = PrioritySettings(20);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
}
