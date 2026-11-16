#include "brain_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig linearConfig(float duration)
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Linear, duration};
    return config;
}

} // namespace

TEST_CASE("Interrupting a blend creates a nested blend", "[scenario][brain][nested]")
{
    BrainTestRig rig(linearConfig(2.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    const CameraId c = rig.addCamera("C", 30, 20.0f);
    rig.system.setCameraEnabled(b, false);
    rig.system.setCameraEnabled(c, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {2.5f, 0.0f, 0.0f}));

    rig.system.setCameraEnabled(c, true);
    rig.step(0.5f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(c));
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.brain->isLiveChild(&rig.camera(a)));
    REQUIRE(rig.brain->isLiveChild(&rig.camera(b)));
    REQUIRE(rig.brain->isLiveChild(&rig.camera(c)));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {8.75f, 0.0f, 0.0f}));

    rig.step(1.5f);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(c));
    REQUIRE_FALSE(rig.brain->isLiveChild(&rig.camera(a)));
    REQUIRE_FALSE(rig.brain->isLiveChild(&rig.camera(b)));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {20.0f, 0.0f, 0.0f}));
}

TEST_CASE("FreezeWhenBlendingOut freezes the outgoing snapshot", "[scenario][brain][nested]")
{
    BrainTestRig rig(linearConfig(2.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.camera(a).blendHint = BlendHint::FreezeWhenBlendingOut;
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.0f);
    REQUIRE(rig.brain->isBlending());

    rig.world.set(rig.camera(a).ownerEntity, Transform{{100.0f, 0.0f, 0.0f}, math::Quat::identity()});
    rig.step(0.5f);

    REQUIRE(rig.brain->isBlending());
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {2.5f, 0.0f, 0.0f}));
}

TEST_CASE("InheritPosition makes the incoming camera start at the outgoing state", "[scenario][brain][nested]")
{
    BrainTestRig rig(linearConfig(2.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.camera(b).blendHint = BlendHint::InheritPosition;
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.0f);

    REQUIRE(math::approximately(rig.camera(b).state().rawPosition, {0.0f, 0.0f, 0.0f}));
    REQUIRE(math::approximately(
        rig.world.transform(rig.camera(b).ownerEntity).position,
        {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Backing out of a blend shortens and reverses it", "[scenario][brain][nested]")
{
    BrainTestRig rig(linearConfig(1.0f));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {5.0f, 0.0f, 0.0f}));

    rig.system.setCameraEnabled(b, false);
    rig.step(0.2f);

    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));
    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(math::approximately(blend->duration, 0.5f));
    REQUIRE(math::approximately(blend->timeInBlend, 0.2f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {3.0f, 0.0f, 0.0f}));
}
