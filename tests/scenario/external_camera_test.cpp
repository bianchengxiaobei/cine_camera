#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"
#include "pipeline_test_components.hpp"

#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/camera/external_camera.hpp>
#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("External camera reads entity state, output lens, and projects look-at", "[scenario][external]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity()});
    world.add(2, Transform{{11.0f, 2.0f, -7.0f}, math::Quat::identity()});

    FakeCameraOutput output;
    LensSettings inherited;
    inherited.fieldOfView = 70.0f;
    output.setOutputLens(inherited);

    const CameraId id = system.createCamera<ExternalCamera>("external");
    auto* external = static_cast<ExternalCamera*>(system.camera(id));
    external->cameraEntity = 1;
    external->output = &output;
    external->lookAtTarget = TargetRef::entity(2);
    external->blendHint = BlendHint::NoPosition;
    external->lens.fieldOfView = 20.0f;

    auto extension = std::make_unique<RecordingExtension>("external-extension");
    RecordingExtension* extensionPtr = extension.get();
    external->addExtension(std::move(extension));

    external->internalUpdateCameraState(math::Vec3::up, 0.016f);

    REQUIRE(math::approximately(external->state().rawPosition, {1.0f, 2.0f, 3.0f}));
    REQUIRE(external->effectiveLookAtTarget() == TargetRef::entity(2));
    REQUIRE(math::approximately(external->state().rawOrientation, math::Quat::identity()));
    REQUIRE(math::approximately(external->state().referenceUp, math::Vec3::up));
    REQUIRE(math::approximately(external->state().referenceLookAt, {1.0f, 2.0f, -7.0f}));
    REQUIRE(external->state().lens.fieldOfView == 70.0f);
    REQUIRE(external->state().blendHint == BlendHint::NoPosition);
    REQUIRE(external->previousStateIsValid);
    REQUIRE(extensionPtr->lastStage == Stage::Finalize);
    REQUIRE(extensionPtr->callbackCamera == external);
}

TEST_CASE("External camera falls back to owner entity and local lens", "[scenario][external]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(7, Transform{{4.0f, 5.0f, 6.0f}, math::Quat::identity()});

    const CameraId id = system.createCamera<ExternalCamera>("owner-external");
    auto* external = static_cast<ExternalCamera*>(system.camera(id));
    external->ownerEntity = 7;
    external->lens.fieldOfView = 33.0f;

    external->internalUpdateCameraState(math::Vec3::up, 0.016f);

    REQUIRE(math::approximately(external->state().rawPosition, {4.0f, 5.0f, 6.0f}));
    REQUIRE(external->state().lens.fieldOfView == 33.0f);
    REQUIRE_FALSE(external->state().hasLookAt());
}

TEST_CASE("External camera participates in brain selection and output", "[scenario][external]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{{2.0f, 3.0f, 4.0f}, math::Quat::identity()});

    FakeCameraOutput output;
    const BrainId brainId = system.createBrain(output, BrainConfig{});
    auto* brain = system.brain(brainId);

    const CameraId externalId = system.createCamera<ExternalCamera>("brain-external");
    auto* external = static_cast<ExternalCamera*>(system.camera(externalId));
    external->cameraEntity = 1;
    external->priority = PrioritySettings(5);

    const CameraId lowerId = system.createCamera<VirtualCamera>("lower");
    system.camera(lowerId)->priority = PrioritySettings(1);

    brain->manualUpdate(0.016f, 1);

    REQUIRE(brain->activeVirtualCamera() == external);
    REQUIRE(output.applyCount == 1);
    REQUIRE(output.applied.hasCamera);
    REQUIRE(math::approximately(output.applied.state.rawPosition, {2.0f, 3.0f, 4.0f}));
}
