#include "brain_test_support.hpp"

#include <cmath>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/auto_focus.hpp>
#include <cinerig/components/post_processing.hpp>
#include <cinerig/engine/post_processor.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct TestProfile
{
    int id = 0;
};

class RecordingPostProcessor final : public IPostProcessor
{
public:
    void apply(const AppliedCamera& camera) override
    {
        applied = camera;
        ++applyCount;
    }

    void resetHistory() override
    {
        ++resetCount;
    }

    AppliedCamera applied;
    int applyCount = 0;
    int resetCount = 0;
};

const CustomBlendable* findBlendable(
    const CameraState& state,
    const void* data)
{
    for (std::size_t index = 0; index < state.numCustomBlendables(); ++index)
    {
        const CustomBlendable& blendable = state.customBlendable(index);
        if (blendable.data.get() == data)
            return &blendable;
    }
    return nullptr;
}

std::shared_ptr<TestProfile> makeProfile(int id)
{
    return std::make_shared<TestProfile>(TestProfile{id});
}

PostProcessing* addPostProcessing(
    VirtualCamera& camera,
    std::shared_ptr<TestProfile> profile,
    float weight = 1.0f,
    PostProcessing::FocusTrackingMode focus =
        PostProcessing::FocusTrackingMode::None)
{
    auto extension = std::make_unique<PostProcessing>();
    extension->profile = PostProcessProfile(std::move(profile));
    extension->weight = weight;
    extension->focusTracking = focus;
    extension->validate();
    PostProcessing* result = extension.get();
    camera.addExtension(std::move(extension));
    return result;
}

EntityId addTarget(BrainTestRig& rig, float x)
{
    const EntityId entity = 500 + static_cast<EntityId>(rig.world.entities().size());
    rig.world.add(entity, Transform{{x, 0.0f, 0.0f}, math::Quat::identity()});
    return entity;
}

} // namespace

TEST_CASE("Post-processing profiles add weighted blendables and look-at focus", "[scenario][postprocess]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    const EntityId target = addTarget(rig, 10.0f);
    VirtualCamera& camera = rig.camera(cameraId);
    camera.targets.lookAt = TargetRef::entity(target);
    camera.targets.customLookAt = true;

    const std::shared_ptr<TestProfile> profile = makeProfile(7);
    PostProcessing* extension = addPostProcessing(
        camera,
        profile,
        0.75f,
        PostProcessing::FocusTrackingMode::LookAtTarget);
    extension->focusOffset = -2.0f;
    extension->validate();

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);
    rig.step(0.0f);

    const CameraState& state = processor.applied.state;
    REQUIRE(state.numCustomBlendables() == 1);
    const CustomBlendable* blendable =
        findBlendable(state, profile.get());
    REQUIRE(blendable != nullptr);
    REQUIRE(blendable->type == typeid(TestProfile));
    REQUIRE(math::approximately(blendable->weight, 0.75f));
    REQUIRE(math::approximately(
        state.lens.physical.focusDistance, 8.0f));
    REQUIRE(math::approximately(
        extension->calculatedFocusDistance(camera), 8.0f));

    const PostProcessProfile handle = extension->profile;
    REQUIRE(handle.isValid());
    REQUIRE(handle.get<TestProfile>() == profile.get());
    REQUIRE(handle.get<int>() == nullptr);
}

TEST_CASE("Post-processing focus modes use follow, custom, and camera targets", "[scenario][postprocess]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    VirtualCamera& camera = rig.camera(cameraId);

    const EntityId followTarget = addTarget(rig, 6.0f);
    const EntityId customTarget = addTarget(rig, 4.0f);
    camera.targets.tracking = TargetRef::entity(followTarget);

    const std::shared_ptr<TestProfile> profile = makeProfile(1);
    PostProcessing* extension = addPostProcessing(
        camera, profile, 1.0f,
        PostProcessing::FocusTrackingMode::FollowTarget);
    extension->focusOffset = 1.0f;
    extension->focusTarget = TargetRef::entity(customTarget);
    extension->validate();

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);

    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 7.0f));

    extension->focusTracking = PostProcessing::FocusTrackingMode::CustomTarget;
    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 5.0f));

    extension->focusTracking = PostProcessing::FocusTrackingMode::Camera;
    extension->focusOffset = 2.0f;
    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 2.0f));

    extension->focusOffset = -100.0f;
    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 0.01f));
}

TEST_CASE("Profile weights blend with the camera blend", "[scenario][postprocess]")
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Linear, 1.0f};
    BrainTestRig rig(config);
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 1.0f);
    rig.system.setCameraEnabled(b, false);

    const std::shared_ptr<TestProfile> profileA = makeProfile(1);
    const std::shared_ptr<TestProfile> profileB = makeProfile(2);
    addPostProcessing(rig.camera(a), profileA, 1.0f);
    addPostProcessing(rig.camera(b), profileB, 0.5f);

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);
    rig.step(0.0f);
    REQUIRE(processor.applied.state.numCustomBlendables() == 1);

    rig.system.setCameraEnabled(b, true);
    rig.step(0.4f);

    const CameraState& state = processor.applied.state;
    REQUIRE(state.numCustomBlendables() == 2);
    const CustomBlendable* blendableA = findBlendable(state, profileA.get());
    const CustomBlendable* blendableB = findBlendable(state, profileB.get());
    REQUIRE(blendableA != nullptr);
    REQUIRE(blendableB != nullptr);
    REQUIRE(math::approximately(blendableA->weight, 0.6f));
    REQUIRE(math::approximately(blendableB->weight, 0.2f));
}

TEST_CASE("AutoFocus damps toward a moving target", "[scenario][postprocess]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    const EntityId target = addTarget(rig, 10.0f);
    VirtualCamera& camera = rig.camera(cameraId);
    camera.targets.lookAt = TargetRef::entity(target);
    camera.targets.customLookAt = true;

    auto extension = std::make_unique<AutoFocus>();
    AutoFocus* focus = extension.get();
    focus->focusMode = AutoFocus::FocusMode::LookAtTarget;
    focus->damping = 1.0f;
    focus->validate();
    camera.addExtension(std::move(extension));

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);
    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 10.0f));
    REQUIRE(math::approximately(focus->currentFocusDistance(camera), 10.0f));

    rig.world.set(target, Transform{{20.0f, 0.0f, 0.0f}, math::Quat::identity()});
    rig.step(0.5f);

    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 19.0f, 1e-3f));
    REQUIRE(math::approximately(focus->currentFocusDistance(camera), 19.0f, 1e-3f));
    REQUIRE(math::approximately(focus->maxDampTime(), 1.0f));
}

TEST_CASE("AutoFocus supports follow and custom targets", "[scenario][postprocess]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    const EntityId followTarget = addTarget(rig, 6.0f);
    const EntityId customTarget = addTarget(rig, 3.0f);
    VirtualCamera& camera = rig.camera(cameraId);
    camera.targets.tracking = TargetRef::entity(followTarget);

    auto extension = std::make_unique<AutoFocus>();
    AutoFocus* focus = extension.get();
    focus->focusMode = AutoFocus::FocusMode::FollowTarget;
    focus->customTarget = TargetRef::entity(customTarget);
    focus->damping = 0.0f;
    focus->validate();
    camera.addExtension(std::move(extension));

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);

    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 6.0f));

    focus->focusMode = AutoFocus::FocusMode::CustomTarget;
    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 3.0f));

    focus->focusMode = AutoFocus::FocusMode::Camera;
    focus->focusOffset = 2.0f;
    rig.step(0.0f);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 2.0f));

    focus->damping = -1.0f;
    focus->focusOffset = std::nanf("");
    focus->validate();
    REQUIRE(focus->damping == 0.0f);
    REQUIRE(focus->focusOffset == 0.0f);
}

TEST_CASE("The Brain applies post-processing and resets history on cuts", "[scenario][postprocess]")
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Cut, 0.0f};
    BrainTestRig rig(config);
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 1.0f);
    rig.system.setCameraEnabled(b, false);

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);
    REQUIRE(rig.brain->postProcessor() == &processor);

    rig.step(0.0f);
    REQUIRE(processor.applyCount == 1);
    REQUIRE(processor.resetCount == 1);
    REQUIRE(processor.applied.hasCamera);

    rig.step(0.0f);
    REQUIRE(processor.applyCount == 2);
    REQUIRE(processor.resetCount == 1);

    rig.system.setCameraEnabled(b, true);
    rig.step(0.0f);
    REQUIRE(processor.applyCount == 3);
    REQUIRE(processor.resetCount == 2);

    rig.brain->setPostProcessor(nullptr);
    rig.step(0.0f);
    REQUIRE(processor.applyCount == 3);
}

TEST_CASE("Non-cut blends do not reset post-processing history", "[scenario][postprocess]")
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Linear, 1.0f};
    BrainTestRig rig(config);
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 1.0f);
    rig.system.setCameraEnabled(b, false);

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);
    rig.step(0.0f);
    REQUIRE(processor.resetCount == 1);

    rig.system.setCameraEnabled(b, true);
    rig.step(0.4f);
    REQUIRE(processor.resetCount == 1);
    REQUIRE(rig.brain->isBlending());
}

TEST_CASE("Invalid profiles add no blendables and validation clamps settings", "[scenario][postprocess]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    VirtualCamera& camera = rig.camera(cameraId);

    auto extension = std::make_unique<PostProcessing>();
    PostProcessing* post = extension.get();
    post->focusTracking = PostProcessing::FocusTrackingMode::Camera;
    post->weight = -2.0f;
    post->focusOffset = std::nanf("");
    post->validate();
    REQUIRE(post->weight == 0.0f);
    REQUIRE(post->focusOffset == 0.0f);
    camera.addExtension(std::move(extension));

    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);
    rig.step(0.0f);

    REQUIRE(processor.applied.state.numCustomBlendables() == 0);
    REQUIRE(math::approximately(
        processor.applied.state.lens.physical.focusDistance, 10.0f));
}
TEST_CASE("Post-processing receives frames without an active camera", "[scenario][postprocess]")
{
    BrainTestRig rig;
    RecordingPostProcessor processor;
    rig.brain->setPostProcessor(&processor);

    rig.step(0.0f);

    REQUIRE(processor.applyCount == 1);
    REQUIRE(processor.resetCount == 0);
    REQUIRE_FALSE(processor.applied.hasCamera);
}
