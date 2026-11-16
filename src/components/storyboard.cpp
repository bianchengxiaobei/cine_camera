#include <cinerig/components/storyboard.hpp>

#include <algorithm>
#include <cmath>

namespace cinerig
{

namespace
{

float finiteOr(float value, float fallback) noexcept
{
    return std::isfinite(value) ? value : fallback;
}

float finiteOrZero(float value) noexcept
{
    return finiteOr(value, 0.0f);
}

math::Vec2 finiteOr(
    math::Vec2 value,
    math::Vec2 fallback) noexcept
{
    return {
        finiteOr(value.x, fallback.x),
        finiteOr(value.y, fallback.y),
    };
}

math::Vec3 finiteOr(
    math::Vec3 value,
    math::Vec3 fallback) noexcept
{
    return {
        finiteOr(value.x, fallback.x),
        finiteOr(value.y, fallback.y),
        finiteOr(value.z, fallback.z),
    };
}

bool positive(math::Vec2 value) noexcept
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && value.x > 0.0f
        && value.y > 0.0f;
}

} // namespace

StoryboardExtension::StoryboardExtension()
    : settings_(std::make_shared<StoryboardSettings>())
{
    syncSettings();
}

void StoryboardExtension::validate()
{
    alpha = math::clamp01(finiteOr(alpha, 0.0f));
    splitView = math::clamp(finiteOrZero(splitView), -1.0f, 1.0f);
    center = finiteOr(center, math::Vec2::zero);
    rotation = finiteOr(rotation, math::Vec3::zero);
    scale = finiteOr(scale, math::Vec2::one);
    syncSettings();
}

std::shared_ptr<const StoryboardSettings> StoryboardExtension::settings() const noexcept
{
    return settings_;
}

void StoryboardExtension::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)deltaTime;
    if (stage != Stage::Finalize || &vcam != virtualCamera())
        return;

    syncSettings();

    if (muteCamera)
        state.blendHint |= BlendHint::NoTransform | BlendHint::NoLens;

    if (showImage)
        state.addCustomBlendable(CustomBlendable(settings_, 1.0f));
}

void StoryboardExtension::syncSettings()
{
    settings_->image = image;
    settings_->aspect = aspect;
    settings_->alpha = alpha;
    settings_->center = center;
    settings_->rotation = rotation;
    settings_->scale = scale;
    settings_->syncScale = syncScale;
    settings_->splitView = splitView;
}

StoryboardLayout calculateStoryboardLayout(
    const StoryboardSettings& settings,
    float blendWeight,
    math::Vec2 screenSize) noexcept
{
    StoryboardLayout layout;
    if (!positive(screenSize))
        return layout;

    const float split = math::clamp(
        finiteOrZero(settings.splitView), -1.0f, 1.0f);
    const float wipeAmount = -split * screenSize.x;

    const math::Vec2 center = finiteOr(settings.center, math::Vec2::zero);
    const math::Vec3 rotation = finiteOr(settings.rotation, math::Vec3::zero);
    const math::Vec2 userScale = finiteOr(settings.scale, math::Vec2::one);

    math::Vec2 imageScale = math::Vec2::one;
    const bool hasImage = settings.image.isValid()
        && positive(settings.image.size);
    if (hasImage)
    {
        const float f = (screenSize.y * settings.image.size.x)
            / (screenSize.x * settings.image.size.y);
        switch (settings.aspect)
        {
        case StoryboardFillStrategy::BestFit:
            if (f >= 1.0f)
                imageScale.y /= f;
            else
                imageScale.x *= f;
            break;
        case StoryboardFillStrategy::CropImageToFit:
            if (f >= 1.0f)
                imageScale.x *= f;
            else
                imageScale.y /= f;
            break;
        case StoryboardFillStrategy::StretchToFit:
            break;
        }
    }

    imageScale.x *= userScale.x;
    imageScale.y *= settings.syncScale ? userScale.x : userScale.y;

    layout.screenSize = screenSize;
    layout.viewportCenter = {
        screenSize.x * 0.5f - wipeAmount * 0.5f,
        screenSize.y * 0.5f,
    };
    layout.viewportSize = {
        screenSize.x + 1.0f - std::abs(wipeAmount),
        screenSize.y + 1.0f,
    };
    layout.imageCenter = {
        screenSize.x * (0.5f + center.x),
        screenSize.y * (0.5f + center.y),
    };
    layout.imageSize = {
        screenSize.x * imageScale.x,
        screenSize.y * imageScale.y,
    };
    layout.imageScale = imageScale;
    layout.rotation = rotation;
    layout.alpha = math::clamp01(
        finiteOr(settings.alpha, 0.0f)
        * std::max(0.0f, finiteOr(blendWeight, 0.0f)));
    layout.valid = true;
    layout.hasImage = hasImage;
    return layout;
}

} // namespace cinerig
