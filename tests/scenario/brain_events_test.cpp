#include "brain_test_support.hpp"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig eventConfig()
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Linear, 1.0f};
    return config;
}

} // namespace

TEST_CASE("Brain emits activation, blend and camera update events in order", "[scenario][brain][events]")
{
    BrainTestRig rig(eventConfig());
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);

    std::vector<std::string> log;
    int activated = 0;
    int deactivated = 0;
    int blendCreated = 0;
    int blendFinished = 0;
    int updated = 0;
    bool lastActivationWasCut = false;
    ICinemachineCamera* lastDeactivated = nullptr;
    ICinemachineCamera* lastFinished = nullptr;
    Blend* createdBlend = nullptr;

    rig.brain->events().addCameraActivated(
        [&](const ActivationEventParams& event)
        {
            ++activated;
            lastActivationWasCut = event.isCut;
            log.push_back("activated:" + std::string(event.incomingCamera->name()));
        });
    rig.brain->events().addCameraDeactivated(
        [&](ICinemachineMixer*, ICinemachineCamera* camera)
        {
            ++deactivated;
            lastDeactivated = camera;
            log.push_back("deactivated:" + std::string(camera->name()));
        });
    rig.brain->events().addBlendCreated(
        [&](const BlendEventParams& event)
        {
            ++blendCreated;
            createdBlend = event.blend;
            log.push_back("blend-created");
        });
    rig.brain->events().addBlendFinished(
        [&](ICinemachineMixer*, ICinemachineCamera* camera)
        {
            ++blendFinished;
            lastFinished = camera;
            log.push_back("blend-finished:" + std::string(camera->name()));
        });
    rig.brain->events().addCameraUpdated(
        [&](CameraBrain&)
        {
            ++updated;
            log.push_back("updated");
        });

    rig.step(0.0f);
    REQUIRE(activated == 1);
    REQUIRE(lastActivationWasCut);
    REQUIRE(deactivated == 0);
    REQUIRE(blendCreated == 0);
    REQUIRE(blendFinished == 0);
    REQUIRE(updated == 1);
    REQUIRE(log == std::vector<std::string>{"activated:A", "updated"});

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);
    REQUIRE(activated == 2);
    REQUIRE_FALSE(lastActivationWasCut);
    REQUIRE(createdBlend != nullptr);
    REQUIRE(blendCreated == 1);
    REQUIRE(deactivated == 0);
    REQUIRE(blendFinished == 0);
    REQUIRE(log.size() == 5);
    REQUIRE(log[2] == "blend-created");
    REQUIRE(log[3] == "activated:B");
    REQUIRE(log[4] == "updated");

    rig.step(0.5f);
    REQUIRE(deactivated == 1);
    REQUIRE(lastDeactivated == &rig.camera(a));
    REQUIRE(blendFinished == 1);
    REQUIRE(lastFinished == &rig.camera(b));
    REQUIRE_FALSE(rig.brain->isBlending());
    REQUIRE(log.size() == 8);
    REQUIRE(log[5] == "deactivated:A");
    REQUIRE(log[6] == "blend-finished:B");
    REQUIRE(log[7] == "updated");
}

TEST_CASE("Blend created listeners cannot corrupt the active blend", "[scenario][brain][events]")
{
    BrainTestRig rig(eventConfig());
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 10.0f);
    rig.system.setCameraEnabled(b, false);
    rig.step(0.0f);

    VirtualCamera& incoming = rig.camera(b);
    rig.brain->events().addBlendCreated(
        [&](const BlendEventParams& event)
        {
            event.blend->camA = nullptr;
            event.blend->camB = &incoming;
        });

    rig.system.setCameraEnabled(b, true);
    rig.step(0.5f);

    REQUIRE(rig.brain->activeVirtualCamera() == &incoming);
    REQUIRE(rig.brain->isBlending());
    REQUIRE(math::approximately(rig.output.applied.state.rawPosition, {5.0f, 0.0f, 0.0f}));
}
