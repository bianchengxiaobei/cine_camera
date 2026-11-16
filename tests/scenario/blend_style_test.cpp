#include "brain_test_support.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig configFor(BlendDefinition definition)
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = definition;
    return config;
}

BlendDefinition customSlowIn()
{
    BlendDefinition definition;
    definition.style = BlendStyle::Custom;
    definition.time = 2.0f;
    definition.customCurve = math::Curve({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 3.0f, 3.0f},
    });
    return definition;
}

} // namespace

TEST_CASE("Cut switches immediately without an active blend", "[scenario][brain][blend]")
{
    BrainTestRig rig(configFor(BlendDefinition{BlendStyle::Cut, 2.0f}));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.0f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeBlend() == nullptr);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("Linear blending follows normalized blend time", "[scenario][brain][blend]")
{
    BrainTestRig rig(configFor(BlendDefinition{BlendStyle::Linear, 2.0f}));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    rig.step(1.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->duration == 2.0f);
    REQUIRE(blend->timeInBlend == 1.0f);
    REQUIRE(blend->blendWeight() == Catch::Approx(0.5f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {5.0f, 0.0f, 0.0f}));

    rig.step(1.0f);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("EaseInOut starts at zero, passes through half, and completes", "[scenario][brain][blend]")
{
    BrainTestRig rig(configFor(BlendDefinition{BlendStyle::EaseInOut, 2.0f}));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->blendWeight() == 0.0f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));

    rig.step(1.0f);
    blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->blendWeight() == Catch::Approx(0.5f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {5.0f, 0.0f, 0.0f}));

    rig.step(1.0f);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("Custom curves drive the blend weight", "[scenario][brain][blend]")
{
    BrainTestRig rig(configFor(customSlowIn()));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(1.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->blendWeight() == Catch::Approx(0.125f).margin(1e-4f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {1.25f, 0.0f, 0.0f}));
}

TEST_CASE("Changing the default style does not change the active blend", "[scenario][brain][blend]")
{
    BrainTestRig rig(configFor(BlendDefinition{BlendStyle::Linear, 2.0f}));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(1.0f);
    REQUIRE(rig.brain->isBlending());

    rig.brain->config().defaultBlend = BlendDefinition{BlendStyle::Cut, 0.0f};
    rig.step(0.5f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->duration == 2.0f);
    REQUIRE(blend->timeInBlend == 1.5f);
    REQUIRE(blend->blendWeight() == Catch::Approx(0.75f));

    rig.camera(a).priority = PrioritySettings(30);
    rig.step(0.0f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));
    REQUIRE_FALSE(rig.brain->isBlending());
}

TEST_CASE("A third camera creates a nested blend while another blend is active", "[scenario][brain][blend]")
{
    BrainTestRig rig(configFor(BlendDefinition{BlendStyle::Linear, 2.0f}));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    const CameraId c = rig.addCamera("C", 30, 20.0f);
    rig.system.setCameraEnabled(b, false);
    rig.system.setCameraEnabled(c, false);

    rig.step(0.0f);
    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    rig.system.setCameraEnabled(c, true);
    rig.step(0.5f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(c));
    REQUIRE(rig.brain->isBlending());
    REQUIRE(rig.brain->isLiveChild(&rig.camera(a)));
    REQUIRE(rig.brain->isLiveChild(&rig.camera(b)));
    REQUIRE(rig.brain->isLiveChild(&rig.camera(c)));

    rig.step(2.0f);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(c));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {20.0f, 0.0f, 0.0f}));
}
