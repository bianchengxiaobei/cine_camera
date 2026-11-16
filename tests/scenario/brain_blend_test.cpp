#include "brain_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig makeConfig(BlendDefinition blend)
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = blend;
    return config;
}

} // namespace

TEST_CASE("A zero-duration blend cuts immediately", "[scenario][brain][blend]")
{
    BrainTestRig rig(makeConfig(BlendDefinition{BlendStyle::Cut, 2.0f}));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeBlend() == nullptr);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("EaseInOut blending advances, applies and completes", "[scenario][brain][blend]")
{
    const BlendDefinition definition{BlendStyle::EaseInOut, 2.0f};
    BrainTestRig rig(makeConfig(definition));
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(a));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);

    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(rig.brain->isBlending());
    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->duration == 2.0f);
    REQUIRE(math::approximately(blend->timeInBlend, 0.5f));
    const float weight = definition.curve().evaluate(0.25f);
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition.x, 10.0f * weight));

    rig.step(1.5f);
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(b));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));
}

TEST_CASE("Blend hints reach the applied camera state", "[scenario][brain][blend]")
{
    BrainTestRig rig;
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    rig.camera(a).blendHint = BlendHint::NoPosition | BlendHint::NoOrientation | BlendHint::NoLens;

    rig.step(0.0f);

    REQUIRE(rig.output.applied.hasCamera);
    REQUIRE(hasBlendHint(rig.output.applied.state.blendHint, BlendHint::NoPosition));
    REQUIRE(hasBlendHint(rig.output.applied.state.blendHint, BlendHint::NoOrientation));
    REQUIRE(hasBlendHint(rig.output.applied.state.blendHint, BlendHint::NoLens));
}

TEST_CASE("Brain applies lens inheritance and lens mode override", "[scenario][brain][blend]")
{
    BrainConfig config;
    config.useWorldUpOverride = true;
    config.worldUpOverride = math::Vec3::up;
    config.lensModeOverride.enabled = true;
    config.lensModeOverride.defaultMode = LensSettings::OverrideMode::Orthographic;
    BrainTestRig rig(config);

    LensSettings outputLens = LensSettings::Default;
    outputLens.orthographic = true;
    outputLens.physicalCamera = true;
    rig.output.setOutputLens(outputLens);
    rig.output.setAspect(2.0f);

    const CameraId a = rig.addCamera("A", 10, 0.0f);
    rig.step(0.0f);

    REQUIRE(rig.output.applied.state.lens.aspect == 2.0f);
    REQUIRE(rig.output.applied.state.lens.orthographic);
    REQUIRE_FALSE(rig.output.applied.state.lens.physicalCamera);

    rig.camera(a).lens.modeOverride = LensSettings::OverrideMode::Perspective;
    rig.step(0.0f);
    REQUIRE_FALSE(rig.output.applied.state.lens.orthographic);
    REQUIRE_FALSE(rig.output.applied.state.lens.physicalCamera);
}
