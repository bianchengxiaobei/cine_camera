#include "brain_test_support.hpp"

#include <cmath>
#include <limits>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/storyboard.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct TestImage
{
    int id = 0;
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

StoryboardExtension* addStoryboard(
    VirtualCamera& camera,
    std::shared_ptr<TestImage> image,
    math::Vec2 imageSize = {1000.0f, 1000.0f})
{
    auto extension = std::make_unique<StoryboardExtension>();
    extension->image = StoryboardImage(std::move(image), imageSize);
    StoryboardExtension* result = extension.get();
    camera.addExtension(std::move(extension));
    return result;
}

StoryboardSettings makeSettings(
    math::Vec2 imageSize,
    StoryboardFillStrategy aspect)
{
    StoryboardSettings settings;
    settings.image = StoryboardImage(
        std::make_shared<TestImage>(), imageSize);
    settings.aspect = aspect;
    return settings;
}

} // namespace

TEST_CASE("Storyboard submits one stable settings blendable", "[scenario][storyboard]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    VirtualCamera& camera = rig.camera(cameraId);

    const auto image = std::make_shared<TestImage>(TestImage{12});
    StoryboardExtension* storyboard = addStoryboard(camera, image);
    storyboard->alpha = 0.4f;
    storyboard->center = {0.2f, -0.1f};
    storyboard->validate();

    rig.step(0.0f);

    const CameraState& state = rig.output.applied.state;
    REQUIRE(state.numCustomBlendables() == 1);
    const CustomBlendable& blendable = state.customBlendable(0);
    REQUIRE(blendable.type == typeid(StoryboardSettings));
    REQUIRE(blendable.weight == 1.0f);
    const auto* settings =
        static_cast<const StoryboardSettings*>(blendable.data.get());
    REQUIRE(settings != nullptr);
    REQUIRE(settings == storyboard->settings().get());
    REQUIRE(settings->image.get<TestImage>() == image.get());
    REQUIRE(settings->image.get<int>() == nullptr);
    REQUIRE(math::approximately(settings->alpha, 0.4f));
    REQUIRE(math::approximately(settings->center, math::Vec2{0.2f, -0.1f}));

    rig.step(0.0f);
    REQUIRE(rig.output.applied.state.customBlendable(0).data.get() == settings);
}

TEST_CASE("Storyboard applies only to its owning camera", "[scenario][storyboard]")
{
    BrainTestRig rig;
    const CameraId parentId = rig.addCamera("Parent", 10, 0.0f);
    const CameraId childId = rig.addCamera("Child", 5, 1.0f);
    VirtualCamera& parent = rig.camera(parentId);
    VirtualCamera& child = rig.camera(childId);
    child.setParent(&parent);

    StoryboardExtension* storyboard =
        addStoryboard(parent, std::make_shared<TestImage>());
    storyboard->muteCamera = true;

    child.updateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(child.state().numCustomBlendables() == 0);
    REQUIRE_FALSE(hasBlendHint(child.state().blendHint, BlendHint::NoTransform));
    REQUIRE_FALSE(hasBlendHint(child.state().blendHint, BlendHint::NoLens));

    parent.updateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(parent.state().numCustomBlendables() == 1);
    REQUIRE(hasBlendHint(parent.state().blendHint, BlendHint::NoTransform));
    REQUIRE(hasBlendHint(parent.state().blendHint, BlendHint::NoLens));
}

TEST_CASE("Storyboard visibility and mute are independent", "[scenario][storyboard]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    VirtualCamera& camera = rig.camera(cameraId);

    StoryboardExtension* storyboard =
        addStoryboard(camera, std::make_shared<TestImage>());
    storyboard->showImage = false;
    storyboard->muteCamera = false;

    rig.step(0.0f);
    REQUIRE(rig.output.applied.state.numCustomBlendables() == 0);
    REQUIRE_FALSE(hasBlendHint(
        rig.output.applied.state.blendHint, BlendHint::NoTransform));
    REQUIRE_FALSE(hasBlendHint(
        rig.output.applied.state.blendHint, BlendHint::NoLens));

    storyboard->muteCamera = true;
    rig.step(0.0f);
    REQUIRE(rig.output.applied.state.numCustomBlendables() == 0);
    REQUIRE(hasBlendHint(
        rig.output.applied.state.blendHint, BlendHint::NoTransform));
    REQUIRE(hasBlendHint(
        rig.output.applied.state.blendHint, BlendHint::NoLens));

    storyboard->showImage = true;
    rig.step(0.0f);
    REQUIRE(rig.output.applied.state.numCustomBlendables() == 1);
}

TEST_CASE("Storyboard fill strategies match the original aspect math", "[scenario][storyboard]")
{
    const math::Vec2 screen{1600.0f, 900.0f};
    const math::Vec2 image{1000.0f, 1000.0f};

    const StoryboardLayout bestFit = calculateStoryboardLayout(
        makeSettings(image, StoryboardFillStrategy::BestFit), 1.0f, screen);
    REQUIRE(bestFit.valid);
    REQUIRE(bestFit.hasImage);
    REQUIRE(math::approximately(bestFit.imageScale, {0.5625f, 1.0f}));
    REQUIRE(math::approximately(bestFit.imageSize, {900.0f, 900.0f}));

    const StoryboardLayout crop = calculateStoryboardLayout(
        makeSettings(image, StoryboardFillStrategy::CropImageToFit), 1.0f, screen);
    REQUIRE(math::approximately(crop.imageScale, {1.0f, 16.0f / 9.0f}));
    REQUIRE(math::approximately(crop.imageSize, {1600.0f, 1600.0f}, 1e-3f));

    const StoryboardLayout stretch = calculateStoryboardLayout(
        makeSettings(image, StoryboardFillStrategy::StretchToFit), 1.0f, screen);
    REQUIRE(math::approximately(stretch.imageScale, math::Vec2::one));
    REQUIRE(math::approximately(stretch.imageSize, screen));

    StoryboardSettings missingImage = makeSettings(image, StoryboardFillStrategy::BestFit);
    missingImage.image = StoryboardImage{};
    const StoryboardLayout missing = calculateStoryboardLayout(
        missingImage, 1.0f, screen);
    REQUIRE_FALSE(missing.hasImage);
    REQUIRE(math::approximately(missing.imageScale, math::Vec2::one));

    REQUIRE_FALSE(calculateStoryboardLayout(
        missingImage, 1.0f, {0.0f, 900.0f}).valid);
}

TEST_CASE("Storyboard split view and center produce the expected layout", "[scenario][storyboard]")
{
    StoryboardSettings settings;
    settings.image = StoryboardImage(
        std::make_shared<TestImage>(), {1000.0f, 1000.0f});
    settings.aspect = StoryboardFillStrategy::StretchToFit;
    settings.center = {0.1f, -0.2f};
    settings.alpha = 0.5f;
    settings.splitView = 0.5f;

    const StoryboardLayout layout = calculateStoryboardLayout(
        settings, 0.5f, {1600.0f, 900.0f});
    REQUIRE(layout.valid);
    REQUIRE(math::approximately(layout.viewportCenter, {1200.0f, 450.0f}));
    REQUIRE(math::approximately(layout.viewportSize, {801.0f, 901.0f}));
    REQUIRE(math::approximately(layout.imageCenter, {960.0f, 270.0f}));
    REQUIRE(math::approximately(layout.imageSize, {1600.0f, 900.0f}));
    REQUIRE(math::approximately(layout.imageScale, math::Vec2::one));
    REQUIRE(math::approximately(layout.alpha, 0.25f));
    REQUIRE(layout.hasImage);

    settings.splitView = -0.25f;
    const StoryboardLayout negative = calculateStoryboardLayout(
        settings, 0.5f, {1600.0f, 900.0f});
    REQUIRE(math::approximately(negative.viewportCenter, {600.0f, 450.0f}));
    REQUIRE(math::approximately(negative.viewportSize, {1201.0f, 901.0f}));
}

TEST_CASE("Storyboard validation sanitizes layout inputs", "[scenario][storyboard]")
{
    StoryboardSettings settings;
    settings.image = StoryboardImage(
        std::make_shared<TestImage>(), {1000.0f, 1000.0f});
    settings.alpha = -2.0f;
    settings.splitView = 4.0f;
    settings.center = {
        std::numeric_limits<float>::quiet_NaN(), 0.25f};
    settings.rotation = {
        10.0f, std::numeric_limits<float>::infinity(), -5.0f};
    settings.scale = {
        std::numeric_limits<float>::quiet_NaN(), 3.0f};
    settings.syncScale = true;

    const StoryboardLayout layout = calculateStoryboardLayout(
        settings, 2.0f, {1600.0f, 900.0f});
    REQUIRE(math::approximately(layout.alpha, 0.0f));
    REQUIRE(math::approximately(layout.viewportCenter.x, 1600.0f));
    REQUIRE(math::approximately(layout.viewportSize.x, 1.0f));
    REQUIRE(math::approximately(layout.imageCenter.x, 800.0f));
    REQUIRE(math::approximately(layout.imageCenter.y, 675.0f));
    REQUIRE(math::approximately(layout.rotation.y, 0.0f));

    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    StoryboardExtension* storyboard = addStoryboard(
        rig.camera(cameraId), std::make_shared<TestImage>());
    storyboard->alpha = 4.0f;
    storyboard->splitView = -4.0f;
    storyboard->center = {
        std::numeric_limits<float>::quiet_NaN(), 0.5f};
    storyboard->rotation = {
        std::numeric_limits<float>::infinity(), 2.0f, 3.0f};
    storyboard->scale = {
        std::numeric_limits<float>::quiet_NaN(), 2.0f};
    storyboard->validate();

    REQUIRE(storyboard->alpha == 1.0f);
    REQUIRE(storyboard->splitView == -1.0f);
    REQUIRE(math::approximately(storyboard->center, {0.0f, 0.5f}));
    REQUIRE(math::approximately(storyboard->rotation, {0.0f, 2.0f, 3.0f}));
    REQUIRE(math::approximately(storyboard->scale, {1.0f, 2.0f}));
    REQUIRE(math::approximately(storyboard->settings()->alpha, 1.0f));
}

TEST_CASE("Storyboard weights blend with the camera blend", "[scenario][storyboard]")
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Linear, 1.0f};
    BrainTestRig rig(config);
    const CameraId a = rig.addCamera("A", 10, 0.0f);
    const CameraId b = rig.addCamera("B", 20, 1.0f);
    rig.system.setCameraEnabled(b, false);

    const auto imageA = std::make_shared<TestImage>(TestImage{1});
    const auto imageB = std::make_shared<TestImage>(TestImage{2});
    StoryboardExtension* storyboardA = addStoryboard(rig.camera(a), imageA);
    StoryboardExtension* storyboardB = addStoryboard(rig.camera(b), imageB);

    rig.step(0.0f);
    REQUIRE(rig.output.applied.state.numCustomBlendables() == 1);
    REQUIRE(math::approximately(
        rig.output.applied.state.customBlendable(0).weight, 1.0f));

    rig.system.setCameraEnabled(b, true);
    rig.step(0.4f);

    const CameraState& state = rig.output.applied.state;
    REQUIRE(state.numCustomBlendables() == 2);
    const CustomBlendable* blendableA = findBlendable(
        state, storyboardA->settings().get());
    const CustomBlendable* blendableB = findBlendable(
        state, storyboardB->settings().get());
    REQUIRE(blendableA != nullptr);
    REQUIRE(blendableB != nullptr);
    REQUIRE(math::approximately(blendableA->weight, 0.6f));
    REQUIRE(math::approximately(blendableB->weight, 0.4f));

    const auto* settingsA =
        static_cast<const StoryboardSettings*>(blendableA->data.get());
    const auto* settingsB =
        static_cast<const StoryboardSettings*>(blendableB->data.get());
    REQUIRE(math::approximately(
        calculateStoryboardLayout(
            *settingsA, blendableA->weight, {1600.0f, 900.0f}).alpha,
        0.6f));
    REQUIRE(math::approximately(
        calculateStoryboardLayout(
            *settingsB, blendableB->weight, {1600.0f, 900.0f}).alpha,
        0.4f));
}
