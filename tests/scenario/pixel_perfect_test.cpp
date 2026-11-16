#include "brain_test_support.hpp"

#include <cmath>
#include <limits>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/pixel_perfect.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

PixelPerfectSettings makeSettings(
    PixelPerfectCropFrame cropFrame = PixelPerfectCropFrame::None,
    PixelPerfectGridSnapping gridSnapping =
        PixelPerfectGridSnapping::None)
{
    PixelPerfectSettings settings;
    settings.assetsPPU = 100;
    settings.refResolutionX = 320;
    settings.refResolutionY = 180;
    settings.cropFrame = cropFrame;
    settings.gridSnapping = gridSnapping;
    return settings;
}

PixelPerfectExtension* addPixelPerfect(
    VirtualCamera& camera,
    math::Vec2 screenSize = {1280.0f, 720.0f})
{
    auto extension = std::make_unique<PixelPerfectExtension>();
    extension->screenSize = screenSize;
    PixelPerfectExtension* result = extension.get();
    camera.addExtension(std::move(extension));
    return result;
}

void useOrthographicLens(BrainTestRig& rig, VirtualCamera& camera)
{
    camera.lens.orthographic = true;
    camera.lens.orthographicSize = 1.0f;
    rig.output.setOutputLens(camera.lens);
    rig.output.setAspect(16.0f / 9.0f);
}

} // namespace

TEST_CASE("Pixel perfect properties match the reference resolution", "[scenario][pixel-perfect]")
{
    const PixelPerfectLayout layout = calculatePixelPerfectLayout(
        makeSettings(), {1280.0f, 720.0f});

    REQUIRE(layout.valid);
    REQUIRE(layout.zoom == 4);
    REQUIRE_FALSE(layout.cropFrameX);
    REQUIRE_FALSE(layout.cropFrameY);
    REQUIRE_FALSE(layout.useStretchFill);
    REQUIRE_FALSE(layout.useOffscreenRT);
    REQUIRE_FALSE(layout.requiresUpscaling);
    REQUIRE(math::approximately(layout.orthographicSize, 0.9f));
    REQUIRE(math::approximately(layout.unitsPerPixel, 0.0025f));
    REQUIRE(math::approximately(layout.pixelRect.width, 0.0f));
    REQUIRE(math::approximately(layout.pixelRect.height, 0.0f));

    const PixelPerfectLayout nonInteger = calculatePixelPerfectLayout(
        makeSettings(), {1600.4f, 900.2f});
    REQUIRE(nonInteger.zoom == 5);
    REQUIRE(math::approximately(nonInteger.screenSize, {1600.0f, 900.0f}));
    REQUIRE(math::approximately(nonInteger.orthographicSize, 0.9f));

    const PixelPerfectLayout upscale = calculatePixelPerfectLayout(
        makeSettings(
            PixelPerfectCropFrame::None,
            PixelPerfectGridSnapping::UpscaleRenderTexture),
        {1600.0f, 900.0f});
    REQUIRE(upscale.valid);
    REQUIRE(upscale.useOffscreenRT);
    REQUIRE(upscale.offscreenRTWidth == 320);
    REQUIRE(upscale.offscreenRTHeight == 180);
    REQUIRE(math::approximately(upscale.pixelRect.width, 320.0f));
    REQUIRE(math::approximately(upscale.pixelRect.height, 180.0f));
    REQUIRE(math::approximately(upscale.orthographicSize, 0.9f));
    REQUIRE(math::approximately(upscale.unitsPerPixel, 0.01f));

    const PixelPerfectLayout snapping = calculatePixelPerfectLayout(
        makeSettings(
            PixelPerfectCropFrame::None,
            PixelPerfectGridSnapping::PixelSnapping),
        {1600.0f, 900.0f});
    REQUIRE_FALSE(snapping.useOffscreenRT);
    REQUIRE(math::approximately(snapping.orthographicSize, 0.9f));
    REQUIRE(math::approximately(snapping.unitsPerPixel, 0.01f));
}

TEST_CASE("Pixel perfect crop frames produce the original offscreen layout", "[scenario][pixel-perfect]")
{
    const math::Vec2 screen{1280.0f, 960.0f};

    const PixelPerfectLayout pillarbox = calculatePixelPerfectLayout(
        makeSettings(PixelPerfectCropFrame::Pillarbox), screen);
    REQUIRE(pillarbox.valid);
    REQUIRE(pillarbox.cropFrameX);
    REQUIRE_FALSE(pillarbox.cropFrameY);
    REQUIRE(pillarbox.useOffscreenRT);
    REQUIRE(pillarbox.offscreenRTWidth == 1280);
    REQUIRE(pillarbox.offscreenRTHeight == 960);
    REQUIRE(math::approximately(pillarbox.orthographicSize, 1.2f));

    const PixelPerfectLayout letterbox = calculatePixelPerfectLayout(
        makeSettings(PixelPerfectCropFrame::Letterbox), screen);
    REQUIRE_FALSE(letterbox.cropFrameX);
    REQUIRE(letterbox.cropFrameY);
    REQUIRE(letterbox.useOffscreenRT);
    REQUIRE(letterbox.offscreenRTWidth == 1280);
    REQUIRE(letterbox.offscreenRTHeight == 720);
    REQUIRE(math::approximately(letterbox.orthographicSize, 0.9f));

    const PixelPerfectLayout windowbox = calculatePixelPerfectLayout(
        makeSettings(PixelPerfectCropFrame::Windowbox), screen);
    REQUIRE(windowbox.cropFrameX);
    REQUIRE(windowbox.cropFrameY);
    REQUIRE(windowbox.useOffscreenRT);
    REQUIRE(windowbox.offscreenRTWidth == 1280);
    REQUIRE(windowbox.offscreenRTHeight == 720);

    const PixelPerfectLayout stretch = calculatePixelPerfectLayout(
        makeSettings(
            PixelPerfectCropFrame::StretchFill,
            PixelPerfectGridSnapping::UpscaleRenderTexture),
        screen);
    REQUIRE(stretch.cropFrameX);
    REQUIRE(stretch.cropFrameY);
    REQUIRE(stretch.useStretchFill);
    REQUIRE(stretch.useOffscreenRT);
    REQUIRE(stretch.requiresUpscaling);
    REQUIRE(stretch.offscreenRTWidth == 320);
    REQUIRE(stretch.offscreenRTHeight == 180);
    REQUIRE(math::approximately(stretch.orthographicSize, 0.9f));
}

TEST_CASE("Pixel perfect correction matches the original rounding", "[scenario][pixel-perfect]")
{
    const PixelPerfectSettings settings = makeSettings();
    const PixelPerfectLayout layout = calculatePixelPerfectLayout(
        settings, {1280.0f, 720.0f});

    const PixelPerfectCorrection rounded = correctPixelPerfectOrthoSize(
        settings, layout, 1.1f);
    REQUIRE(rounded.valid);
    REQUIRE(rounded.cinemachineVCamZoom == 3);
    REQUIRE(rounded.pixelRatio == 3);
    REQUIRE(math::approximately(rounded.orthographicSize, 1.2f));
    REQUIRE(math::approximately(rounded.unitsPerPixel, 1.0f / 300.0f));

    const PixelPerfectCorrection halfEven = correctPixelPerfectOrthoSize(
        settings, layout, 0.8f);
    REQUIRE(halfEven.valid);
    REQUIRE(halfEven.cinemachineVCamZoom == 4);
    REQUIRE(halfEven.pixelRatio == 4);
    REQUIRE(math::approximately(halfEven.orthographicSize, 0.9f));

    const PixelPerfectCorrection larger = correctPixelPerfectOrthoSize(
        settings, layout, 0.75f);
    REQUIRE(larger.cinemachineVCamZoom == 5);
    REQUIRE(math::approximately(larger.orthographicSize, 0.72f));

    const PixelPerfectSettings upscaleSettings = makeSettings(
        PixelPerfectCropFrame::None,
        PixelPerfectGridSnapping::UpscaleRenderTexture);
    const PixelPerfectLayout upscaleLayout = calculatePixelPerfectLayout(
        upscaleSettings, {1600.0f, 900.0f});
    const PixelPerfectCorrection upscaleCorrection =
        correctPixelPerfectOrthoSize(upscaleSettings, upscaleLayout, 0.5f);
    REQUIRE(upscaleCorrection.valid);
    REQUIRE(upscaleCorrection.cinemachineVCamZoom == 2);
    REQUIRE(upscaleCorrection.pixelRatio == 10);
    REQUIRE(math::approximately(upscaleCorrection.orthographicSize, 0.45f));
    REQUIRE(math::approximately(upscaleCorrection.unitsPerPixel, 0.01f));

    const PixelPerfectCorrection snappingCorrection =
        correctPixelPerfectOrthoSize(
            makeSettings(
                PixelPerfectCropFrame::None,
                PixelPerfectGridSnapping::PixelSnapping),
            calculatePixelPerfectLayout(
                makeSettings(
                    PixelPerfectCropFrame::None,
                    PixelPerfectGridSnapping::PixelSnapping),
                {1600.0f, 900.0f}),
            1.1f);
    REQUIRE(snappingCorrection.cinemachineVCamZoom == 4);
    REQUIRE(math::approximately(snappingCorrection.orthographicSize, 1.125f));
    REQUIRE(math::approximately(snappingCorrection.unitsPerPixel, 0.01f));
}

TEST_CASE("Pixel perfect final blit matches centered and stretch geometry", "[scenario][pixel-perfect]")
{
    const PixelPerfectSettings letterboxSettings =
        makeSettings(PixelPerfectCropFrame::Letterbox);
    const PixelPerfectLayout letterboxLayout = calculatePixelPerfectLayout(
        letterboxSettings, {1280.0f, 960.0f});
    const PixelPerfectFinalBlit letterbox = calculatePixelPerfectFinalBlit(
        letterboxSettings, letterboxLayout);
    REQUIRE_FALSE(letterbox.requiresUpscaling);
    REQUIRE(math::approximately(letterbox.pixelRect.x, 0.0f));
    REQUIRE(math::approximately(letterbox.pixelRect.y, 120.0f));
    REQUIRE(math::approximately(letterbox.pixelRect.width, 1280.0f));
    REQUIRE(math::approximately(letterbox.pixelRect.height, 720.0f));

    const PixelPerfectSettings stretchSettings = makeSettings(
        PixelPerfectCropFrame::StretchFill,
        PixelPerfectGridSnapping::UpscaleRenderTexture);
    const PixelPerfectLayout stretchLayout = calculatePixelPerfectLayout(
        stretchSettings, {1920.0f, 800.0f});
    const PixelPerfectFinalBlit stretch = calculatePixelPerfectFinalBlit(
        stretchSettings, stretchLayout);
    REQUIRE(stretch.requiresUpscaling);
    REQUIRE(math::approximately(stretch.pixelRect.x, 249.0f));
    REQUIRE(math::approximately(stretch.pixelRect.y, 0.0f));
    REQUIRE(math::approximately(stretch.pixelRect.width, 1422.2222f, 1e-3f));
    REQUIRE(math::approximately(stretch.pixelRect.height, 800.0f));

    const PixelPerfectLayout sameAspectLayout = calculatePixelPerfectLayout(
        stretchSettings, {1600.0f, 900.0f});
    const PixelPerfectFinalBlit sameAspect = calculatePixelPerfectFinalBlit(
        stretchSettings, sameAspectLayout);
    REQUIRE_FALSE(sameAspect.requiresUpscaling);
    REQUIRE(math::approximately(sameAspect.pixelRect.x, 0.0f));
    REQUIRE(math::approximately(sameAspect.pixelRect.y, 0.0f));
    REQUIRE(math::approximately(sameAspect.pixelRect.width, 1600.0f));
    REQUIRE(math::approximately(sameAspect.pixelRect.height, 900.0f));

    const PixelPerfectSettings upscaleSettings = makeSettings(
        PixelPerfectCropFrame::None,
        PixelPerfectGridSnapping::UpscaleRenderTexture);
    const PixelPerfectLayout upscaleLayout = calculatePixelPerfectLayout(
        upscaleSettings, {1600.0f, 900.0f});
    const PixelPerfectFinalBlit upscale = calculatePixelPerfectFinalBlit(
        upscaleSettings, upscaleLayout);
    REQUIRE_FALSE(upscale.requiresUpscaling);
    REQUIRE(math::approximately(upscale.pixelRect.x, 0.0f));
    REQUIRE(math::approximately(upscale.pixelRect.y, 0.0f));
    REQUIRE(math::approximately(upscale.pixelRect.width, 1600.0f));
    REQUIRE(math::approximately(upscale.pixelRect.height, 900.0f));
}

TEST_CASE("Pixel perfect extension corrects only live cameras", "[scenario][pixel-perfect]")
{
    BrainTestRig rig;
    const CameraId cameraId = rig.addCamera("A", 10, 0.0f);
    VirtualCamera& camera = rig.camera(cameraId);
    useOrthographicLens(rig, camera);

    PixelPerfectExtension* extension = addPixelPerfect(camera);

    camera.updateCameraState(math::Vec3::up, 0.0f);
    REQUIRE(math::approximately(camera.state().lens.orthographicSize, 1.0f));
    REQUIRE_FALSE(extension->layout().valid);

    rig.step(0.0f);
    rig.step(0.0f);
    REQUIRE(math::approximately(camera.state().lens.orthographicSize, 0.9f));
    REQUIRE(math::approximately(
        rig.output.applied.state.lens.orthographicSize, 0.9f));
    REQUIRE(extension->layout().valid);
    REQUIRE(extension->correction().valid);
    REQUIRE(extension->correction().cinemachineVCamZoom == 4);

    CameraState manual = camera.state();
    manual.lens.orthographicSize = 1.0f;
    extension->postPipelineStageCallback(camera, Stage::Finalize, manual, 0.0f);
    REQUIRE(math::approximately(manual.lens.orthographicSize, 1.0f));
    extension->postPipelineStageCallback(camera, Stage::Aim, manual, 0.0f);
    REQUIRE(math::approximately(manual.lens.orthographicSize, 1.0f));
    extension->postPipelineStageCallback(camera, Stage::Body, manual, 0.0f);
    REQUIRE(math::approximately(manual.lens.orthographicSize, 0.9f));

    extension->setEnabled(false);
    rig.step(0.0f);
    REQUIRE(math::approximately(camera.state().lens.orthographicSize, 1.0f));

    extension->setEnabled(true);
    rig.step(0.0f);
    REQUIRE(math::approximately(camera.state().lens.orthographicSize, 0.9f));

    extension->screenSize = {0.0f, 720.0f};
    rig.step(0.0f);
    REQUIRE(math::approximately(camera.state().lens.orthographicSize, 1.0f));

    extension->screenSize = {1280.0f, 720.0f};
    camera.lens.orthographicSize = 0.0f;
    rig.step(0.0f);
    REQUIRE(math::approximately(camera.state().lens.orthographicSize, 0.0f));
}

TEST_CASE("Pixel perfect validation sanitizes settings and inputs", "[scenario][pixel-perfect]")
{
    PixelPerfectSettings settings;
    settings.assetsPPU = 0;
    settings.refResolutionX = -5;
    settings.refResolutionY = 0;
    settings.cropFrame =
        static_cast<PixelPerfectCropFrame>(99);
    settings.gridSnapping =
        static_cast<PixelPerfectGridSnapping>(99);
    settings.validate();

    REQUIRE(settings.assetsPPU == 1);
    REQUIRE(settings.refResolutionX == 1);
    REQUIRE(settings.refResolutionY == 1);
    REQUIRE(settings.cropFrame == PixelPerfectCropFrame::None);
    REQUIRE(settings.gridSnapping == PixelPerfectGridSnapping::None);

    REQUIRE_FALSE(calculatePixelPerfectLayout(
        makeSettings(), {0.0f, 720.0f}).valid);
    REQUIRE_FALSE(calculatePixelPerfectLayout(
        makeSettings(),
        {std::numeric_limits<float>::quiet_NaN(), 720.0f}).valid);
    REQUIRE_FALSE(calculatePixelPerfectLayout(
        makeSettings(), {1280.0f, -1.0f}).valid);

    const PixelPerfectLayout validLayout = calculatePixelPerfectLayout(
        makeSettings(), {1280.0f, 720.0f});
    REQUIRE_FALSE(correctPixelPerfectOrthoSize(
        makeSettings(), validLayout, 0.0f).valid);
    REQUIRE_FALSE(correctPixelPerfectOrthoSize(
        makeSettings(), validLayout, -1.0f).valid);
    REQUIRE_FALSE(correctPixelPerfectOrthoSize(
        makeSettings(),
        validLayout,
        std::numeric_limits<float>::infinity()).valid);
    REQUIRE_FALSE(correctPixelPerfectOrthoSize(
        makeSettings(), PixelPerfectLayout{}, 1.0f).valid);

    const PixelPerfectFinalBlit invalidBlit = calculatePixelPerfectFinalBlit(
        makeSettings(), PixelPerfectLayout{});
    REQUIRE_FALSE(invalidBlit.requiresUpscaling);
    REQUIRE(math::approximately(invalidBlit.pixelRect.width, 0.0f));

    PixelPerfectExtension extension;
    extension.screenSize = {
        std::numeric_limits<float>::quiet_NaN(), -4.0f};
    extension.settings.assetsPPU = -3;
    extension.validate();
    REQUIRE(extension.settings.assetsPPU == 1);
    REQUIRE(math::approximately(extension.screenSize, {0.0f, 0.0f}));
}
