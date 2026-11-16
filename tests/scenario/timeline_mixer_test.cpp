#include "brain_test_support.hpp"

#include <array>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/brain/timeline_mixer.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig timelineConfig()
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Cut, 0.0f};
    return config;
}

TimelineShot shot(
    ICinemachineCamera& camera,
    float clipTime,
    float duration,
    float weight)
{
    return TimelineShot{&camera, clipTime, duration, weight};
}

} // namespace

TEST_CASE("A single timeline shot overrides gameplay until released", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 10, 0.0f);
    const CameraId shotB = rig.addCamera("B", 1, 10.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));

    ICameraOverrideStack& stack = *rig.brain;
    REQUIRE(math::approximately(stack.defaultWorldUp(), math::Vec3::up));
    TimelineMixer mixer(stack, 5);
    REQUIRE_FALSE(mixer.hasOverride());
    REQUIRE(mixer.overrideId() == -1);

    const std::array<TimelineShot, 1> shots{shot(rig.camera(shotB), 0.5f, 2.0f, 1.0f)};
    const int firstId = mixer.processFrame(shots, true, 0.0f);
    REQUIRE(firstId >= 0);
    REQUIRE(mixer.hasOverride());
    REQUIRE(rig.brain->blendManager().overrideCount() == 1);

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotB));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {10.0f, 0.0f, 0.0f}));

    mixer.release();
    REQUIRE_FALSE(mixer.hasOverride());
    REQUIRE(rig.brain->blendManager().overrideCount() == 0);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));

    const int secondId = mixer.processFrame(shots, true, 0.0f);
    REQUIRE(secondId > firstId);
    mixer.release();
    mixer.release();
    REQUIRE(rig.brain->blendManager().overrideCount() == 0);
}

TEST_CASE("Overlapping timeline shots supply the blend weight", "[scenario][brain][timeline]")
{
    BrainConfig config = timelineConfig();
    config.defaultBlend = BlendDefinition{BlendStyle::EaseInOut, 5.0f};
    BrainTestRig rig(config);
    const CameraId gameplay = rig.addCamera("Gameplay", 5, -10.0f);
    const CameraId shotA = rig.addCamera("A", 1, 10.0f);
    const CameraId shotB = rig.addCamera("B", 2, 20.0f);
    const CameraId shotC = rig.addCamera("C", 3, 30.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));

    TimelineMixer mixer(*rig.brain);
    const std::array<TimelineShot, 3> shots{
        shot(rig.camera(shotA), 1.2f, 3.0f, 1.0f),
        shot(rig.camera(shotB), 0.2f, 3.0f, 0.4f),
        shot(rig.camera(shotC), 0.1f, 3.0f, 0.8f),
    };
    mixer.processFrame(shots, true, 0.0f);
    rig.step(0.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->camA == &rig.camera(shotA));
    REQUIRE(blend->camB == &rig.camera(shotB));
    REQUIRE_FALSE(blend->uses(&rig.camera(shotC)));
    REQUIRE(math::approximately(blend->duration, 1.0f));
    REQUIRE(math::approximately(blend->blendWeight(), 0.4f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {14.0f, 0.0f, 0.0f}));

    mixer.release();
}

TEST_CASE("A single timeline clip fading out blends back to gameplay", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 10, 0.0f);
    const CameraId shotA = rig.addCamera("A", 2, 10.0f);
    rig.step(0.0f);

    TimelineMixer mixer(*rig.brain);
    const std::array<TimelineShot, 1> shots{shot(rig.camera(shotA), 1.2f, 2.0f, 0.5f)};
    mixer.processFrame(shots, true, 0.0f);
    rig.step(0.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->camA == &rig.camera(shotA));
    REQUIRE(blend->uses(&rig.camera(gameplay)));
    REQUIRE(math::approximately(blend->blendWeight(), 0.5f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {5.0f, 0.0f, 0.0f}));

    mixer.release();
}

TEST_CASE("Paused and empty timeline tracks return control to gameplay", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 10, 0.0f);
    const CameraId shotA = rig.addCamera("A", 2, 10.0f);
    rig.step(0.0f);

    TimelineMixer mixer(*rig.brain);
    const std::array<TimelineShot, 1> shots{shot(rig.camera(shotA), 0.5f, 2.0f, 1.0f)};
    mixer.processFrame(shots, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotA));

    mixer.processFrame({}, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(mixer.hasOverride());
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {0.0f, 0.0f, 0.0f}));

    mixer.processFrame(shots, false, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));

    mixer.processFrame(shots, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotA));

    mixer.release();
}

TEST_CASE("Timeline ignores null and zero-weight shots", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 10, 0.0f);
    const CameraId shotA = rig.addCamera("A", 2, 10.0f);
    rig.step(0.0f);

    TimelineMixer mixer(*rig.brain);
    const std::array<TimelineShot, 3> shots{
        TimelineShot{nullptr, 0.0f, 2.0f, 1.0f},
        shot(rig.camera(shotA), 0.0f, 2.0f, 0.0f),
        shot(rig.camera(shotA), 0.0f, 2.0f, 0.0f),
    };
    mixer.processFrame(shots, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));

    mixer.release();
}

TEST_CASE("Timeline tracks honor override priority and recency", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 5, -10.0f);
    const CameraId shotA = rig.addCamera("A", 1, 10.0f);
    const CameraId shotB = rig.addCamera("B", 2, 20.0f);
    rig.step(0.0f);

    const std::array<TimelineShot, 1> aShots{shot(rig.camera(shotA), 0.5f, 2.0f, 1.0f)};
    const std::array<TimelineShot, 1> bShots{shot(rig.camera(shotB), 0.5f, 2.0f, 1.0f)};

    TimelineMixer low(*rig.brain, 0);
    low.processFrame(aShots, true, 0.0f);
    TimelineMixer high(*rig.brain, 10);
    high.processFrame(bShots, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotB));

    high.processFrame({}, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotA));

    TimelineMixer newest(*rig.brain, 10);
    newest.processFrame(aShots, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotA));

    newest.release();
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotA));

    high.processFrame(bShots, true, 0.0f);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotB));

    high.release();
    low.release();
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));
}

TEST_CASE("Equal timeline clip start times use the shorter clip", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 5, -10.0f);
    const CameraId shotA = rig.addCamera("A", 1, 10.0f);
    const CameraId shotB = rig.addCamera("B", 2, 20.0f);
    rig.step(0.0f);

    TimelineMixer mixer(*rig.brain);
    const std::array<TimelineShot, 2> shots{
        shot(rig.camera(shotA), 1.0f, 4.0f, 1.0f),
        shot(rig.camera(shotB), 1.0f, 2.0f, 0.6f),
    };
    mixer.processFrame(shots, true, 0.0f);
    rig.step(0.0f);

    const Blend* blend = rig.brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->camA == &rig.camera(shotB));
    REQUIRE(blend->camB == &rig.camera(shotA));
    REQUIRE(math::approximately(blend->blendWeight(), 0.4f));
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {16.0f, 0.0f, 0.0f}));

    mixer.release();
}

TEST_CASE("Timeline mixer destruction releases its override", "[scenario][brain][timeline]")
{
    BrainTestRig rig(timelineConfig());
    const CameraId gameplay = rig.addCamera("Gameplay", 10, 0.0f);
    const CameraId shotA = rig.addCamera("A", 2, 10.0f);
    rig.step(0.0f);

    {
        TimelineMixer mixer(*rig.brain);
        const std::array<TimelineShot, 1> shots{shot(rig.camera(shotA), 0.5f, 2.0f, 1.0f)};
        mixer.processFrame(shots, true, 0.0f);
        rig.step(0.0f);
        REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(shotA));
        REQUIRE(rig.brain->blendManager().overrideCount() == 1);
    }

    REQUIRE(rig.brain->blendManager().overrideCount() == 0);
    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(gameplay));
}


