#include "brain_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("Camera overrides replace the active camera output", "[scenario][brain][override]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    const int overrideId = rig.brain->setCameraOverride(
        -1, 0, &rig.camera(a), &rig.camera(b), 0.5f, -1.0f);
    REQUIRE(overrideId >= 0);
    REQUIRE(rig.brain->blendManager().overrideCount() == 1);
    REQUIRE(rig.brain->blendManager().overrideWeight(overrideId) == 0.5f);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {5.0f, 0.0f, 0.0f}));

    rig.brain->releaseCameraOverride(overrideId);
    REQUIRE(rig.brain->blendManager().overrideCount() == 0);
    rig.step(0.0f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Override priority selects the topmost active frame", "[scenario][brain][override]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    const CameraId c = rig.addCamera("C", 30, 20.0f);
    rig.system.setCameraEnabled(b, false);
    rig.system.setCameraEnabled(c, false);
    rig.step(0.0f);

    const int low = rig.brain->setCameraOverride(
        1, 0, &rig.camera(a), &rig.camera(b), 0.25f, -1.0f);
    const int high = rig.brain->setCameraOverride(
        2, 10, &rig.camera(a), &rig.camera(c), 0.75f, -1.0f);
    REQUIRE(low == 1);
    REQUIRE(high == 2);
    REQUIRE(rig.brain->blendManager().overridePriority(low) == 0.0f);
    REQUIRE(rig.brain->blendManager().overridePriority(high) == 10.0f);

    rig.step(0.0f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {15.0f, 0.0f, 0.0f}));

    rig.brain->releaseCameraOverride(high);
    rig.step(0.0f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {2.5f, 0.0f, 0.0f}));
}

TEST_CASE("Blend lookup prefers exact, wildcard and global overrides", "[scenario][brain][override]")
{
    VirtualCamera outgoing("A");
    VirtualCamera incoming("B");
    VirtualCamera other("C");
    VirtualCamera fourth("D");

    BlendLookup lookup(BlendDefinition{BlendStyle::EaseInOut, 2.0f});
    lookup.addRule(BlendRule{"A", "B", BlendDefinition{BlendStyle::Linear, 0.1f}});
    lookup.addRule(BlendRule{"**ANY CAMERA**", "C", BlendDefinition{BlendStyle::Linear, 0.2f}});
    lookup.addRule(BlendRule{"A", "**ANY CAMERA**", BlendDefinition{BlendStyle::Linear, 0.3f}});
    lookup.addRule(BlendRule{"**ANY CAMERA**", "**ANY CAMERA**", BlendDefinition{BlendStyle::Cut, 9.0f}});

    REQUIRE(lookup.lookup(&outgoing, &incoming).style == BlendStyle::Linear);
    REQUIRE(lookup.lookup(&outgoing, &incoming).time == 0.1f);
    REQUIRE(lookup.lookup(&other, &fourth).style == BlendStyle::Cut);
    REQUIRE(lookup.lookup(&other, &other).time == 0.2f);

    lookup.clearRules();
    lookup.addRule(BlendRule{"**ANY CAMERA**", "C", BlendDefinition{BlendStyle::Linear, 0.2f}});
    lookup.addRule(BlendRule{"A", "**ANY CAMERA**", BlendDefinition{BlendStyle::Linear, 0.3f}});
    REQUIRE(lookup.lookup(&other, &fourth).style == BlendStyle::EaseInOut);
    REQUIRE(lookup.lookup(&outgoing, &fourth).time == 0.3f);
    REQUIRE(lookup.lookup(&other, &other).time == 0.2f);

    lookup.setOverride([](const ICinemachineCamera*, const ICinemachineCamera*, const BlendDefinition&)
    {
        return BlendDefinition{BlendStyle::HardIn, 1.5f};
    });
    REQUIRE(lookup.lookup(&outgoing, &incoming).style == BlendStyle::HardIn);
}

TEST_CASE("Override delta time is exposed to the brain", "[scenario][brain][override]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);
    rig.step(0.0f);

    const int id = rig.brain->setCameraOverride(
        -1, 0, &rig.camera(a), &rig.camera(b), 0.0f, 0.5f);
    REQUIRE(rig.brain->blendManager().deltaTimeOverride() == 0.5f);

    rig.step(0.0f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));

    rig.brain->releaseCameraOverride(id);
    REQUIRE(rig.brain->blendManager().deltaTimeOverride() == -1.0f);
}
