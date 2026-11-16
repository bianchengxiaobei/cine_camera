# P5-04 Pixel Perfect

Status: completed  
Depends on: P1-03, P5-03  
Blocks: P5-05

## Goal

Port the engine-neutral behavior of `CinemachinePixelPerfect`. A virtual camera
can attach a pixel-perfect extension that corrects its orthographic lens size at
the Body stage so the rendered view lands on the nearest pixel-perfect
resolution for the host output.

The Unity extension delegates the math to the output camera's
`PixelPerfectCamera.CorrectCinemachineOrthoSize()`. CineRig has no Unity pixel
perfect component, so the core also ports the reference-resolution, crop-frame,
grid-snapping, offscreen-size, and final-blit math needed by adapters.

## Scope

- Add `components::PixelPerfectExtension`:
  - `PixelPerfectSettings` with `assetsPPU`, reference resolution, crop frame,
    and grid snapping.
  - Host-supplied output `screenSize` in pixels.
  - Body-stage correction only for live cameras that invoke the extension.
  - Store the last calculated layout and correction for host inspection.
  - Validation for all settings and non-finite screen sizes.
- Add `calculatePixelPerfectLayout(settings, screenSize)`:
  - Pixel-perfect `zoom` from the reference resolution.
  - `cropFrame` x/y flags and `StretchFill`.
  - Offscreen render-target size and camera-pixel-rect size.
  - Base orthographic size and units-per-pixel.
  - `requiresUpscaling` for `StretchFill`.
- Add `correctPixelPerfectOrthoSize(settings, layout, targetOrthoSize)`:
  - Original `cinemachineVCamZoom` calculation.
  - Corrected orthographic size, pixel ratio, and units-per-pixel.
  - Unity-compatible round-half-to-even when choosing the VCam zoom.
- Add `calculatePixelPerfectFinalBlit(settings, layout)`:
  - Centered final-blit rectangle for offscreen rendering.
  - Stretch-fill aspect matching and final upscale requirement.
- Add scenario tests for default/cropped/upscaled properties, correction
  rounding, live gating, validation, and final-blit geometry.
- Add a `38_pixel_perfect` SDL example: a 2D pixel-art test pattern, an
  orthographic camera, correction on/off, crop/grid/target-lens controls, live
  HUD, and a smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Creating render textures, executing an upscale/final-blit pass, drawing crop
  bars, or applying point filtering. Those are renderer-adapter concerns.
- Camera-position pixel snapping, `PixelPerfectRendering.pixelSnapSpacing`,
  `Camera.worldToCameraMatrix`, and sprite renderer integration.
- Unity `PixelPerfectCamera` discovery, URP/U2D package types,
  `isActiveAndEnabled`, `runInEditMode`, `targetTexture`, and `pixelRect`
  queries.
- Editor inspectors, `SaveDuringPlay`, and domain-reload/hot-reload handling.
- Replacing the host camera's viewport, render target, or filter mode.

## Design

```text
PixelPerfectSettings
  int assetsPPU = 100
  int refResolutionX = 320
  int refResolutionY = 180
  CropFrame cropFrame = None
  GridSnapping gridSnapping = None

PixelPerfectLayout
  Vec2 screenSize
  int zoom
  bool cropFrameX / cropFrameY / useStretchFill
  bool useOffscreenRT
  int offscreenRTWidth / offscreenRTHeight
  Rect pixelRect
  float orthographicSize
  float unitsPerPixel
  bool requiresUpscaling
  bool valid

calculatePixelPerfectLayout(settings, screenSize)
  zoom = max(1, min(floor(screenH / refH), floor(screenW / refW)))
  crop flags from cropFrame
  offscreen size and pixelRect from crop/upscale rules
  base orthographic size from crop mode, else from pixelRect/screen height
  unitsPerPixel = 1 / assetsPPU (upscale or pixel snapping)
                   else 1 / (zoom * assetsPPU)

correctPixelPerfectOrthoSize(settings, layout, targetOrthoSize)
  if upscale:
    vcamZoom = max(1, round(orthoSize / target))
    corrected = orthoSize / vcamZoom
  else:
    vcamZoom = max(1, round(zoom * orthoSize / target))
    corrected = zoom * orthoSize / vcamZoom
  if !upscale && !pixelSnapping:
    unitsPerPixel = 1 / (vcamZoom * assetsPPU)
  pixelRatio = upscale ? zoom * vcamZoom : vcamZoom

PixelPerfectExtension : CameraExtension
  PixelPerfectSettings settings
  Vec2 screenSize
  on Body, for a live vcam:
    layout = calculatePixelPerfectLayout(settings, screenSize)
    correction = correctPixelPerfectOrthoSize(settings, layout,
                                               state.lens.orthographicSize)
    state.lens.orthographicSize = correction.orthographicSize
```

The layout and final-blit helpers mirror the URP/U2D
`PixelPerfectCameraInternal` calculations. `gridSnapping == UpscaleRenderTexture`
maps to the original `upscaleRT`; `gridSnapping == PixelSnapping` maps to
`pixelSnapping`. `cropFrame` maps to the legacy `cropFrameX`/`cropFrameY`/
`stretchFill` properties.

The extension runs at Body, matching `CinemachinePixelPerfect`. CineRig invokes
extensions in registration order, so hosts that combine it with `Confiner2D`
should add the pixel-perfect extension before the confiner; the confiner then
reads the corrected lens. The correction is idempotent if the callback is
invoked more than once for the same state.

`screenSize` is supplied by the host each frame because the core library does
not query an output camera. The host also owns the offscreen render target,
final blit, black bars, point filtering, and camera-transform pixel snapping.

## Differences

- Settings are first-class fields on the extension instead of being read from a
  `PixelPerfectCamera` component on the Unity output camera.
- The core exposes the property/correction/final-blit math directly; it does not
  create or resize render textures.
- The output screen size is explicit and per-frame. There is no `targetTexture`
  or output-camera `pixelRect` lookup.
- The extension applies to any live camera whose pipeline invokes it, including
  a child that inherits a parent extension, matching the CameraExtension chain.
  Extension registration order determines its order relative to `Confiner2D`.
- The full framebuffer viewport is used; hosts with sub-viewports can scale the
  returned pixel rectangles.
- Render texture creation, crop bars, filtering, and pixel snapping of camera
  position remain adapter responsibilities.

## API Sketch

```cpp
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
    const PixelPerfectLayout& layout() const noexcept;
    const PixelPerfectCorrection& correction() const noexcept;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachinePixelPerfect.cs`
  - Body-stage live-camera gating and delegation to
    `CorrectCinemachineOrthoSize()`
- URP/U2D `PixelPerfectCameraInternal`
  - `CalculateCameraProperties()`, `CorrectCinemachineOrthoSize()`, and
    `CalculateFinalBlitPixelRect()` math
- `com.unity.cinemachine/Documentation~/CinemachinePixelPerfect.md`
  - Per-camera extension usage, blend behavior, and upscale limitations

## Acceptance

- Default settings at `1280x720` produce a reference zoom of 4 and the original
  base orthographic size for 320x180 at 100 PPU.
- None, Pillarbox, Letterbox, Windowbox, and StretchFill produce the original
  crop/offscreen/pixel-rect flags and sizes.
- `UpscaleRenderTexture` and `PixelSnapping` produce the original offscreen and
  units-per-pixel behavior.
- Correction picks the same `cinemachineVCamZoom` as Unity, including round-half-
  to-even cases, and returns the corrected ortho size, pixel ratio, and units
  per pixel.
- The extension corrects only live cameras at the Body stage; non-live cameras,
  disabled extensions, invalid screen sizes, and invalid target lens sizes are
  left unchanged.
- Invalid PPU, reference resolutions, and enum values are sanitized.
- Final-blit rectangles match centered and stretch-fill geometry.
- The SDL example demonstrates corrected versus uncorrected framing and has a
  passing smoke test.
- Unit/scenario tests, examples Debug/Release, no-tests build, install, and an
  external `find_package(CineRig)` consumer pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/pixel_perfect.hpp`
  - `PixelPerfectSettings`, crop-frame/grid-snapping enums, layout,
    correction, and final-blit result types.
  - `calculatePixelPerfectLayout()`, `correctPixelPerfectOrthoSize()`, and
    `calculatePixelPerfectFinalBlit()`.
  - `PixelPerfectExtension` with host-supplied screen size, validation, last
    layout/correction accessors, and Body-stage live-camera correction.
- `src/components/pixel_perfect.cpp`
  - URP/U2D reference-resolution zoom, crop/offscreen/pixel-rect math,
    base ortho size, units-per-pixel, camera-property correction, and final-blit
    geometry.
  - Unity-compatible round-half-to-even VCam zoom selection and sanitization
    for PPU, reference resolution, enums, screen size, and target lens size.
- `tests/scenario/pixel_perfect_test.cpp`
  - 6 cases / 115 assertions covering properties, crop modes, upscale/pixel
    snapping, correction rounding, final-blit geometry, live/Body gating, and
    validation.
- `examples/38_pixel_perfect`
  - Orthographic pixel-art card with 1/32-unit pixel grid, reference frame,
    correction on/off, grid/crop/target-lens controls, live property HUD, and
    smoke test.
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Verification

- `[pixel-perfect]`: **6 cases / 115 assertions passed**.
- Core Debug and Release: **465/465 passed**
  (319 unit + 140 scenario + 6 golden; 4114 assertions).
- Examples Debug and Release: **518/518 passed**
  (465 core + 53 example/smoke tests, including the 38 pixel-perfect smoke
  test).
- `cinerig_38_pixel_perfect --frames 180 --hidden --no-vsync` exits 0 in Debug
  and Release. Core no-tests and examples-no-tests Debug builds pass.
- Install includes `components/pixel_perfect.hpp`. An external
  `find_package(CineRig)` consumer used `PixelPerfectExtension`,
  `PixelPerfectSettings`, the layout/correction helpers, and final-blit math;
  the crop offscreen size, VCam zoom, corrected lens, and final rectangle
  verified, and the consumer exits 0.
- The captured view at frame 160 shows `Pixel Perfect ON`, `1280x720` screen,
  `320x180` reference, `32` PPU, crop `none`, grid `pixel-snap`, zoom `4`, VCam
  zoom `4`, pixel ratio `4`, target `3.000 -> applied 2.812`, and
  `units/pixel 0.03125` over the 1/32-unit reference grid and pixel-art heart.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p5_pixelperfect.png`.
