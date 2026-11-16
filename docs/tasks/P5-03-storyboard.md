# P5-03 Storyboard

Status: completed  
Depends on: P1-03, P5-02  
Blocks: P5-04

## Goal

Port the engine-neutral behavior of `CinemachineStoryboard`. A virtual camera
can attach a storyboard extension that contributes a weighted overlay handle to
`CameraState` at the Finalize stage. The handle carries an opaque image, image
size, and the screen-space placement settings. The camera adapter computes the
same normalized/pixel geometry as the Unity extension and renders the image
with its engine-native canvas, sprite, or immediate-mode renderer.

The core library does not create canvases, textures, or UI objects. It only
submits the overlay and provides deterministic layout math that adapters can
reuse.

## Scope

- Add `components::StoryboardExtension`:
  - `showImage`, opaque image handle, fill strategy, alpha, screen-space center,
    rotation, scale, synchronized scale, camera mute, and split view.
  - Finalize-stage submission only for the owning virtual camera, not for child
    cameras that inherit the extension.
  - `MuteCamera` adds `BlendHint::NoTransform | BlendHint::NoLens` to the owner
    state, matching the Unity extension.
  - Validation for alpha, split view, center, rotation, and scale.
- Add an opaque `StoryboardImage` shared handle with type recovery and pixel
  dimensions; the host owns the texture/image and keeps it alive.
- Add `StoryboardSettings` as the shared custom-blendable payload. The extension
  reuses one settings object per instance so custom-blendable identity remains
  stable.
- Add `calculateStoryboardLayout(settings, blendWeight, screenSize)`:
  - `BestFit`, `CropImageToFit`, and `StretchToFit`.
  - Pixel image center/size, viewport clip center/size, 2D rotation, and final
    alpha.
  - Split view wipe and the original one-pixel viewport expansion.
  - The adapter may use only the Z rotation for a flat 2D overlay.
- Add scenario tests for submission, owner-only behavior, mute hints, fill
  strategies, split view, alpha/weight, validation, and camera-blend weights.
- Add a `37_storyboard` SDL example with two opaque procedural images, two
  storyboard cameras, blended overlay weights, show/mute/split/fill/global-mute
  controls, an actual textured screen-space overlay adapter, HUD, and smoke
  test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity `Canvas`, `RawImage`, `RectMask2D`, `RenderMode`, `planeDistance`, and
  `sortingOrder` objects. Those are renderer-adapter concerns.
- Unity `CameraUpdatedEvent`, `SaveDuringPlay`, editor canvas cleanup, and the
  global static `s_StoryboardGlobalMute`. The host adapter owns global mute so
  multiple `CameraSystem` instances remain independent.
- Image importing, texture asset management, waveform monitor, and editor
  preview tools.
- Rendering 3D-tilted storyboard planes. Core preserves the full Euler rotation;
  the SDL example adapter uses the Z component for its flat overlay.

## Design

```text
StoryboardImage
  shared_ptr<const void> data
  type_index type
  Vec2 pixelSize
  isValid() / get<T>()

StoryboardSettings
  StoryboardImage image
  FillStrategy aspect
  float alpha
  Vec2 center
  Vec3 rotation (degrees)
  Vec2 scale
  bool syncScale
  float splitView

StoryboardExtension : CameraExtension
  public configuration fields
  one shared StoryboardSettings snapshot
  on Finalize, only when vcam == owner:
    sync configuration into snapshot
    if muteCamera: state.blendHint |= NoTransform | NoLens
    if showImage: add snapshot as CustomBlendable with weight 1

calculateStoryboardLayout(settings, weight, screenSize)
  wipeAmount = -clamp(splitView, -1, 1) * screenWidth
  viewportCenter = (screenWidth/2 + s*screenWidth/2, screenHeight/2)
  viewportSize = (screenWidth + 1 - abs(wipeAmount), screenHeight + 1)
  imageCenter = (screenWidth*(0.5 + center.x), screenHeight*(0.5 + center.y))
  effectiveScale = aspect-fit scale * (scale.x, syncScale ? scale.x : scale.y)
  imageSize = (screenWidth*effectiveScale.x, screenHeight*effectiveScale.y)
  alpha = clamp01(settings.alpha * blendWeight)
```

All geometry is in framebuffer pixels with the origin at the bottom-left.
`viewportCenter`/`viewportSize` describe the clipping rectangle. The image
center is independent of the split wipe, matching the Unity implementation.

The Brain already blends custom blendables by scaling their weights. For a
camera blend from A to B, the adapter receives one entry per storyboard with
weights equal to the camera blend weights. The adapter multiplies each entry's
weight by its own alpha and renders them in state order.

## Differences

- The image is an opaque shared handle instead of a Unity `Texture`. The adapter
  reads the concrete image type and renderer resource from the handle.
- There is no Canvas hierarchy. The adapter uses the computed viewport/image
  geometry and its native clipping mechanism.
- `RenderMode`, `SortingOrder`, and `PlaneDistance` are not core fields; the
  adapter controls compositing order and depth.
- The layout uses the full output viewport rather than a Unity camera
  `pixelRect` sub-rect. Hosts with sub-viewports can scale the returned
  geometry to their output rectangle.
- Global mute is adapter-owned rather than a process-wide static. This avoids
  hidden cross-system state in the core library.
- The SDL example renders a textured screen-space quad and uses the layout's
  Z rotation. Full X/Y Euler rotation is retained in the payload for other
  adapters.

## API Sketch

```cpp
namespace cinerig
{

enum class StoryboardFillStrategy
{
    BestFit,
    CropImageToFit,
    StretchToFit,
};

struct StoryboardImage
{
    std::shared_ptr<const void> data;
    std::type_index type = typeid(void);
    math::Vec2 size{};

    StoryboardImage() = default;
    StoryboardImage(std::shared_ptr<const void> value,
                    std::type_index valueType,
                    math::Vec2 pixelSize);
    template <typename T>
    StoryboardImage(std::shared_ptr<T> value, math::Vec2 pixelSize);

    bool isValid() const noexcept;
    template <typename T> const T* get() const noexcept;
};

struct StoryboardSettings
{
    StoryboardImage image;
    StoryboardFillStrategy aspect = StoryboardFillStrategy::BestFit;
    float alpha = 1.0f;
    math::Vec2 center{};
    math::Vec3 rotation{}; // degrees
    math::Vec2 scale = math::Vec2::one;
    bool syncScale = true;
    float splitView = 0.0f;
};

struct StoryboardLayout
{
    math::Vec2 screenSize{};
    math::Vec2 viewportCenter{};
    math::Vec2 viewportSize{};
    math::Vec2 imageCenter{};
    math::Vec2 imageSize{};
    math::Vec2 imageScale{};
    math::Vec3 rotation{}; // degrees
    float alpha = 0.0f;
    bool valid = false;
    bool hasImage = false;
};

StoryboardLayout calculateStoryboardLayout(
    const StoryboardSettings& settings,
    float blendWeight,
    math::Vec2 screenSize) noexcept;

class StoryboardExtension : public CameraExtension
{
public:
    bool showImage = true;
    StoryboardImage image;
    StoryboardFillStrategy aspect = StoryboardFillStrategy::BestFit;
    float alpha = 1.0f;
    math::Vec2 center{};
    math::Vec3 rotation{};
    math::Vec2 scale = math::Vec2::one;
    bool syncScale = true;
    bool muteCamera = false;
    float splitView = 0.0f;

    void validate();
    std::shared_ptr<const StoryboardSettings> settings() const noexcept;
    void postPipelineStageCallback(
        VirtualCamera& vcam, Stage stage, CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineStoryboard.cs`
  - Finalize-stage owner-only submission, custom blendable weight, mute blend
    hints, aspect strategies, split view, and screen-space placement math
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - custom blendable merge and blend-weight scaling
- `com.unity.cinemachine/Documentation~/CinemachineStoryboard.md`
  - user-facing behavior and property semantics

## Acceptance

- A visible storyboard attached to the owning camera adds one custom blendable
  of type `StoryboardSettings` with weight 1.
- A storyboard attached to a parent camera is not submitted while a child camera
  is processed and does not change the child's blend hints.
- `showImage == false` suppresses the overlay but `muteCamera` still adds
  `NoTransform | NoLens`.
- Best-fit, crop, and stretch produce the expected image scale for mismatched
  image/screen aspects.
- Split view moves and narrows the clip viewport while leaving the image center
  at the configured screen-space position.
- Alpha is multiplied by the blend weight and clamped to `[0, 1]`.
- Invalid/non-finite configuration is sanitized by `validate()`, and missing or
  zero-sized images do not crash layout calculation.
- During a camera blend, the two storyboard weights scale with the camera blend
  weights and remain separately readable by an adapter.
- The SDL example renders the overlay above the 3D scene, supports global mute,
  and has a passing smoke test.
- Unit/scenario tests, examples Debug/Release, no-tests build, install, and an
  external `find_package(CineRig)` consumer pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/storyboard.hpp`
  - `StoryboardFillStrategy`, opaque `StoryboardImage` handle with type
    recovery and pixel dimensions.
  - Shared `StoryboardSettings` custom-blendable payload and
    `StoryboardLayout`.
  - `calculateStoryboardLayout()` with BestFit/Crop/Stretch, split-view wipe,
    center, rotation, scale, alpha, and the original viewport expansion.
  - `StoryboardExtension` with owner-only Finalize submission, stable settings
    identity, visibility/mute independence, and validation.
- `src/components/storyboard.cpp`
  - Aspect-fit math, split viewport geometry, sanitization of non-finite
    inputs, image/weight alpha blending, and owner camera gating.
- `examples/common/gl_renderer.*`
  - Reusable RGBA texture creation and textured screen-space drawing with
    rotation, alpha, tint, and scissor clipping.
- `tests/scenario/storyboard_test.cpp`
  - 7 cases / 65 assertions covering blendable identity, owner-only behavior,
    mute hints, visibility, fill strategies, split/center geometry, validation,
    and camera-blend weights.
- `examples/37_storyboard`
  - Two procedural storyboard images, two storyboard cameras, blended overlay
    weights, actual textured screen-space rendering, show/mute/split/fill/
    alpha/global-mute/reset controls, live settings/layout HUD, and smoke test.
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Verification

- `[storyboard]`: **7 cases / 65 assertions passed**.
- Core Debug and Release: **459/459 passed**
  (319 unit + 134 scenario + 6 golden; 3994 assertions).
- Examples Debug and Release: **511/511 passed**
  (459 core + 52 example/smoke tests, including the 37 storyboard smoke test).
- `cinerig_37_storyboard --frames 180 --hidden --no-vsync` exits 0 in Debug and
  Release. Core no-tests and examples-no-tests Debug builds pass.
- Install includes `components/storyboard.hpp`. An external
  `find_package(CineRig)` consumer used `StoryboardExtension`,
  `StoryboardImage`, `StoryboardSettings`, and layout math; the mute hints,
  crop aspect, split clip, and alpha values verified, and the consumer exits 0.
- The captured view at frame 160 shows `Shot A` live, `Storyboard A 100%`,
  image `922x691`, clip `1281x721` at `(640,360)`, BestFit, split `0`, alpha
  `0.9`, and a textured rotated overlay above the 3D scene. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p5_storyboard.png`.
