#include <cinerig/components/pixel_perfect.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

namespace
{

constexpr int kMaxPixelDimension = 1000000000;

bool isUpscale(PixelPerfectGridSnapping snapping) noexcept
{
    return snapping == PixelPerfectGridSnapping::UpscaleRenderTexture;
}

bool isPixelSnapping(PixelPerfectGridSnapping snapping) noexcept
{
    return snapping == PixelPerfectGridSnapping::PixelSnapping;
}

int roundedPixelDimension(float value) noexcept
{
    if (!std::isfinite(value) || value <= 0.0f)
        return 0;

    const float rounded = std::nearbyint(value);
    if (rounded < 1.0f)
        return 1;
    return static_cast<int>(std::min(rounded, static_cast<float>(kMaxPixelDimension)));
}

int unityRoundToInt(float value) noexcept
{
    if (std::isnan(value))
        return 1;
    if (!std::isfinite(value))
        return value > 0.0f ? kMaxPixelDimension : 1;

    const float rounded = std::nearbyint(value);
    if (rounded < 1.0f)
        return 1;
    return static_cast<int>(std::min(rounded, static_cast<float>(kMaxPixelDimension)));
}

int clampedPixelRatio(int zoom, int vcamZoom) noexcept
{
    const long long ratio =
        static_cast<long long>(zoom) * static_cast<long long>(vcamZoom);
    return static_cast<int>(std::min<long long>(ratio, kMaxPixelDimension));
}

} // namespace

void PixelPerfectSettings::validate()
{
    assetsPPU = std::max(1, assetsPPU);
    refResolutionX = std::max(1, refResolutionX);
    refResolutionY = std::max(1, refResolutionY);

    switch (cropFrame)
    {
    case PixelPerfectCropFrame::None:
    case PixelPerfectCropFrame::Pillarbox:
    case PixelPerfectCropFrame::Letterbox:
    case PixelPerfectCropFrame::Windowbox:
    case PixelPerfectCropFrame::StretchFill:
        break;
    default:
        cropFrame = PixelPerfectCropFrame::None;
        break;
    }

    switch (gridSnapping)
    {
    case PixelPerfectGridSnapping::None:
    case PixelPerfectGridSnapping::PixelSnapping:
    case PixelPerfectGridSnapping::UpscaleRenderTexture:
        break;
    default:
        gridSnapping = PixelPerfectGridSnapping::None;
        break;
    }
}

PixelPerfectLayout calculatePixelPerfectLayout(
    const PixelPerfectSettings& settings,
    math::Vec2 screenSize) noexcept
{
    PixelPerfectLayout layout;
    if (!std::isfinite(screenSize.x)
        || !std::isfinite(screenSize.y)
        || screenSize.x <= 0.0f
        || screenSize.y <= 0.0f)
    {
        return layout;
    }

    const int screenWidth = roundedPixelDimension(screenSize.x);
    const int screenHeight = roundedPixelDimension(screenSize.y);
    if (screenWidth <= 0 || screenHeight <= 0)
        return layout;

    PixelPerfectSettings sanitized = settings;
    sanitized.validate();

    layout.screenSize = {
        static_cast<float>(screenWidth),
        static_cast<float>(screenHeight),
    };
    layout.cropFrameX =
        sanitized.cropFrame == PixelPerfectCropFrame::Pillarbox
        || sanitized.cropFrame == PixelPerfectCropFrame::Windowbox
        || sanitized.cropFrame == PixelPerfectCropFrame::StretchFill;
    layout.cropFrameY =
        sanitized.cropFrame == PixelPerfectCropFrame::Letterbox
        || sanitized.cropFrame == PixelPerfectCropFrame::Windowbox
        || sanitized.cropFrame == PixelPerfectCropFrame::StretchFill;
    const bool cropFrameXAndY = layout.cropFrameX && layout.cropFrameY;
    const bool cropFrameXOrY = layout.cropFrameX || layout.cropFrameY;
    const bool upscale = isUpscale(sanitized.gridSnapping);
    const bool pixelSnapping = isPixelSnapping(sanitized.gridSnapping);

    layout.useStretchFill = cropFrameXAndY
        && sanitized.cropFrame == PixelPerfectCropFrame::StretchFill;
    layout.requiresUpscaling = layout.useStretchFill;

    const int verticalZoom = screenHeight / sanitized.refResolutionY;
    const int horizontalZoom = screenWidth / sanitized.refResolutionX;
    layout.zoom = std::max(1, std::min(verticalZoom, horizontalZoom));

    if (cropFrameXOrY)
    {
        layout.useOffscreenRT = true;
        if (!upscale)
        {
            if (cropFrameXAndY)
            {
                layout.offscreenRTWidth = layout.zoom * sanitized.refResolutionX;
                layout.offscreenRTHeight = layout.zoom * sanitized.refResolutionY;
            }
            else if (layout.cropFrameY)
            {
                layout.offscreenRTWidth = screenWidth;
                layout.offscreenRTHeight = layout.zoom * sanitized.refResolutionY;
            }
            else
            {
                layout.offscreenRTWidth = layout.zoom * sanitized.refResolutionX;
                layout.offscreenRTHeight = screenHeight;
            }
        }
        else
        {
            if (cropFrameXAndY)
            {
                layout.offscreenRTWidth = sanitized.refResolutionX;
                layout.offscreenRTHeight = sanitized.refResolutionY;
            }
            else if (layout.cropFrameY)
            {
                layout.offscreenRTWidth = screenWidth / layout.zoom / 2 * 2;
                layout.offscreenRTHeight = sanitized.refResolutionY;
            }
            else
            {
                layout.offscreenRTWidth = sanitized.refResolutionX;
                layout.offscreenRTHeight = screenHeight / layout.zoom / 2 * 2;
            }
        }
    }
    else if (upscale && layout.zoom > 1)
    {
        layout.useOffscreenRT = true;
        layout.offscreenRTWidth = screenWidth / layout.zoom / 2 * 2;
        layout.offscreenRTHeight = screenHeight / layout.zoom / 2 * 2;
    }

    if (layout.useOffscreenRT)
    {
        layout.pixelRect = {
            0.0f,
            0.0f,
            static_cast<float>(layout.offscreenRTWidth),
            static_cast<float>(layout.offscreenRTHeight),
        };
    }

    if (layout.cropFrameY)
    {
        layout.orthographicSize =
            (sanitized.refResolutionY * 0.5f) / sanitized.assetsPPU;
    }
    else if (layout.cropFrameX)
    {
        const float aspect = !layout.useOffscreenRT
            ? static_cast<float>(screenWidth) / static_cast<float>(screenHeight)
            : layout.pixelRect.width / layout.pixelRect.height;
        layout.orthographicSize =
            ((sanitized.refResolutionX / aspect) * 0.5f) / sanitized.assetsPPU;
    }
    else if (upscale && layout.zoom > 1)
    {
        layout.orthographicSize =
            (layout.offscreenRTHeight * 0.5f) / sanitized.assetsPPU;
    }
    else
    {
        const float pixelHeight = !layout.useOffscreenRT
            ? static_cast<float>(screenHeight)
            : layout.pixelRect.height;
        layout.orthographicSize = (pixelHeight * 0.5f)
            / (layout.zoom * sanitized.assetsPPU);
    }

    layout.unitsPerPixel = (upscale || pixelSnapping)
        ? 1.0f / sanitized.assetsPPU
        : 1.0f / (layout.zoom * sanitized.assetsPPU);
    layout.valid = std::isfinite(layout.orthographicSize)
        && layout.orthographicSize > 0.0f;
    return layout;
}

PixelPerfectCorrection correctPixelPerfectOrthoSize(
    const PixelPerfectSettings& settings,
    const PixelPerfectLayout& layout,
    float targetOrthoSize) noexcept
{
    PixelPerfectCorrection correction;
    if (!layout.valid
        || !std::isfinite(targetOrthoSize)
        || targetOrthoSize <= 0.0f)
    {
        return correction;
    }

    PixelPerfectSettings sanitized = settings;
    sanitized.validate();
    const bool upscale = isUpscale(sanitized.gridSnapping);
    const bool pixelSnapping = isPixelSnapping(sanitized.gridSnapping);
    const float baseOrthoSize = layout.orthographicSize;
    if (!std::isfinite(baseOrthoSize) || baseOrthoSize <= 0.0f)
        return correction;

    if (upscale)
    {
        correction.cinemachineVCamZoom = unityRoundToInt(
            baseOrthoSize / targetOrthoSize);
        correction.orthographicSize =
            baseOrthoSize / correction.cinemachineVCamZoom;
    }
    else
    {
        correction.cinemachineVCamZoom = unityRoundToInt(
            (layout.zoom * baseOrthoSize) / targetOrthoSize);
        correction.orthographicSize =
            (layout.zoom * baseOrthoSize) / correction.cinemachineVCamZoom;
    }

    correction.pixelRatio = upscale
        ? clampedPixelRatio(layout.zoom, correction.cinemachineVCamZoom)
        : correction.cinemachineVCamZoom;
    correction.unitsPerPixel = (upscale || pixelSnapping)
        ? layout.unitsPerPixel
        : 1.0f / (static_cast<float>(correction.cinemachineVCamZoom)
            * sanitized.assetsPPU);
    correction.valid = std::isfinite(correction.orthographicSize)
        && correction.orthographicSize > 0.0f;
    return correction;
}

PixelPerfectFinalBlit calculatePixelPerfectFinalBlit(
    const PixelPerfectSettings& settings,
    const PixelPerfectLayout& layout) noexcept
{
    PixelPerfectFinalBlit result;
    result.requiresUpscaling = layout.requiresUpscaling;
    if (!layout.valid || !layout.useOffscreenRT)
        return result;

    PixelPerfectSettings sanitized = settings;
    sanitized.validate();
    const int screenWidth = roundedPixelDimension(layout.screenSize.x);
    const int screenHeight = roundedPixelDimension(layout.screenSize.y);
    if (screenWidth <= 0 || screenHeight <= 0)
        return result;

    if (layout.useStretchFill)
    {
        const float screenAspect =
            static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
        const float cameraAspect = static_cast<float>(sanitized.refResolutionX)
            / static_cast<float>(sanitized.refResolutionY);

        if (screenAspect > cameraAspect)
        {
            result.pixelRect.height = static_cast<float>(screenHeight);
            result.pixelRect.width = screenHeight * cameraAspect;
            result.pixelRect.x = static_cast<float>(
                (screenWidth - static_cast<int>(result.pixelRect.width)) / 2);
            result.pixelRect.y = 0.0f;
        }
        else
        {
            result.pixelRect.width = static_cast<float>(screenWidth);
            result.pixelRect.height = screenWidth / cameraAspect;
            result.pixelRect.y = static_cast<float>(
                (screenHeight - static_cast<int>(result.pixelRect.height)) / 2);
            result.pixelRect.x = 0.0f;
        }

        if (screenWidth % sanitized.refResolutionX == 0)
            result.requiresUpscaling = cameraAspect < screenAspect;
        else if (screenHeight % sanitized.refResolutionY == 0)
            result.requiresUpscaling = cameraAspect > screenAspect;
    }
    else
    {
        const float width = isUpscale(sanitized.gridSnapping)
            ? static_cast<float>(layout.zoom * layout.offscreenRTWidth)
            : static_cast<float>(layout.offscreenRTWidth);
        const float height = isUpscale(sanitized.gridSnapping)
            ? static_cast<float>(layout.zoom * layout.offscreenRTHeight)
            : static_cast<float>(layout.offscreenRTHeight);

        result.pixelRect.width = width;
        result.pixelRect.height = height;
        result.pixelRect.x = static_cast<float>(
            (screenWidth - static_cast<int>(width)) / 2);
        result.pixelRect.y = static_cast<float>(
            (screenHeight - static_cast<int>(height)) / 2);
    }

    return result;
}

void PixelPerfectExtension::validate()
{
    settings.validate();
    if (!std::isfinite(screenSize.x) || screenSize.x < 0.0f)
        screenSize.x = 0.0f;
    if (!std::isfinite(screenSize.y) || screenSize.y < 0.0f)
        screenSize.y = 0.0f;
}

void PixelPerfectExtension::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)deltaTime;
    if (stage != Stage::Body)
        return;

    if (!vcam.isLive())
        return;

    layout_ = calculatePixelPerfectLayout(settings, screenSize);
    if (!layout_.valid)
    {
        correction_ = PixelPerfectCorrection{};
        return;
    }

    correction_ = correctPixelPerfectOrthoSize(
        settings, layout_, state.lens.orthographicSize);
    if (correction_.valid)
        state.lens.orthographicSize = correction_.orthographicSize;
}

} // namespace cinerig
