#pragma once

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/math/geometry.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

enum class PixelPerfectCropFrame
{
    None,
    Pillarbox,
    Letterbox,
    Windowbox,
    StretchFill,
};

enum class PixelPerfectGridSnapping
{
    None,
    PixelSnapping,
    UpscaleRenderTexture,
};

struct PixelPerfectSettings
{
    int assetsPPU = 100;
    int refResolutionX = 320;
    int refResolutionY = 180;
    PixelPerfectCropFrame cropFrame = PixelPerfectCropFrame::None;
    PixelPerfectGridSnapping gridSnapping =
        PixelPerfectGridSnapping::None;

    void validate();
};

struct PixelPerfectLayout
{
    math::Vec2 screenSize{};
    int zoom = 1;
    bool cropFrameX = false;
    bool cropFrameY = false;
    bool useStretchFill = false;
    bool useOffscreenRT = false;
    int offscreenRTWidth = 0;
    int offscreenRTHeight = 0;
    math::Rect pixelRect{};
    float orthographicSize = 1.0f;
    float unitsPerPixel = 0.0f;
    bool requiresUpscaling = false;
    bool valid = false;
};

struct PixelPerfectCorrection
{
    float orthographicSize = 0.0f;
    int cinemachineVCamZoom = 1;
    int pixelRatio = 1;
    float unitsPerPixel = 0.0f;
    bool valid = false;
};

struct PixelPerfectFinalBlit
{
    math::Rect pixelRect{};
    bool requiresUpscaling = false;
};

PixelPerfectLayout calculatePixelPerfectLayout(
    const PixelPerfectSettings& settings,
    math::Vec2 screenSize) noexcept;

PixelPerfectCorrection correctPixelPerfectOrthoSize(
    const PixelPerfectSettings& settings,
    const PixelPerfectLayout& layout,
    float targetOrthoSize) noexcept;

PixelPerfectFinalBlit calculatePixelPerfectFinalBlit(
    const PixelPerfectSettings& settings,
    const PixelPerfectLayout& layout) noexcept;

class PixelPerfectExtension : public CameraExtension
{
public:
    PixelPerfectSettings settings;
    math::Vec2 screenSize{};

    void validate();

    const PixelPerfectLayout& layout() const noexcept { return layout_; }
    const PixelPerfectCorrection& correction() const noexcept
    {
        return correction_;
    }

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    PixelPerfectLayout layout_{};
    PixelPerfectCorrection correction_{};
};

} // namespace cinerig
