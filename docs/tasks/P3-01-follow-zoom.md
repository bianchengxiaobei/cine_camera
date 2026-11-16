# P3-01 Follow Zoom

Status: completed  
Depends on: P0-04, P1-02, P2-02, P2-03  
Blocks: constant-apparent-size framing, group framing share of damping helpers

## Goal

Implement the `components::FollowZoom` extension. It adjusts the lens FOV at the
Body stage so that the look-at target keeps a constant apparent size as the
camera-to-target distance changes.

## Scope

- Add `components::FollowZoom` with:
  - target shot width
  - damping time
  - clamped FOV range
  - per-camera previous-frame zoom cache
  - validation and max-damping reporting
- Add target-attachment-aware scalar damping helpers on `VirtualCamera` for
  follow and look-at targets.
- Port the original Body-stage calculation, including:
  - distance measured from the corrected camera position to the reference
    look-at point
  - shot width clamped by the FOV range at the current distance
  - damping against the previous frame's equivalent width
  - FOV clamped to the configured range
- Add unit coverage for defaults, validation, FOV calculation, width/FOV
  clamping, damping, attachment relaxation, stage selection, missing look-at
  targets, and cache reset behavior.
- Add a `16_follow_zoom` SDL example with a depth-moving target, a reference
  gauge at the target plane, damping/width controls, HUD, and smoke test.
- Update the task index, feature matrix, architecture, and example index.

## Non-goals

- Orthographic follow zoom. The original extension only writes perspective FOV.
- Automatic target discovery or Unity `RequiredTarget` attributes.
- Serialization, editor inspectors, or preset assets.
- Additional width/height framing constraints beyond the original FOV formula.

## Design

FollowZoom is a `CameraExtension`. Its state is cached per virtual camera so a
parent extension invoked for child cameras keeps independent zoom histories.

```text
on every stage:
    if deltaTime < 0 or previous state is invalid:
        previousFrameZoom = state.lens.fieldOfView

at Body:
    if a look-at point exists and distance > epsilon:
        minWidth = d * 2 * tan(minFov / 2)
        maxWidth = d * 2 * tan(maxFov / 2)
        targetWidth = clamp(width, minWidth, maxWidth)
        currentWidth = d * 2 * tan(previousFrameZoom / 2)
        if damping > 0 and the previous state is valid:
            targetWidth = currentWidth + damp(targetWidth - currentWidth)
        fov = 2 * atan(targetWidth / (2d))
    else:
        fov = maximum FOV

    previousFrameZoom = clamp(fov, minFov, maxFov)
    state.lens.fieldOfView = previousFrameZoom
```

The scalar damping helper follows the original Cinemachine semantics:

```text
effectiveDampTime = lerp(max(1, dampTime), dampTime, targetAttachment)
effectiveDeltaTime = lerp(0, deltaTime, targetAttachment)
result = standardDamp(initial, effectiveDampTime, effectiveDeltaTime)
```

## API Sketch

```cpp
class FollowZoom : public CameraExtension
{
public:
    float width = 2.0f;
    float damping = 1.0f;
    math::Vec2 fovRange{3.0f, 60.0f};

    void validate();
    float maxDampTime() const override;
    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineFollowZoom.cs`
  - field defaults, validation, Body-stage callback, and damping formula
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - detached look-at/follow damping semantics
- `com.unity.cinemachine/Runtime/Core/Predictor.cs`
  - `Damper.Damp` implementation
- `Documentation~/CinemachineFollowZoom.md`
  - width, damping, and FOV range behavior

## Acceptance

- Defaults match the original: width 2, damping 1, FOV range 3..60.
- Validation clamps width and FOV range to valid values.
- FOV produces the configured shot width at the target distance.
- Near/far distances clamp the shot width and FOV to the configured range.
- Damping uses the previous frame's FOV and respects look-at attachment.
- Only the Body stage writes the lens FOV.
- A missing look-at point does not produce NaNs and resolves to the maximum FOV.
- A negative delta time or invalid previous state resets the zoom cache.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/follow_zoom.hpp` and
  `src/components/follow_zoom.cpp`
  - Body-stage FOV calculation from corrected position to the reference
    look-at point
  - width clamp derived from the FOV range at the current distance
  - damping against the previous frame's equivalent width
  - per-camera previous-zoom caches for parent/child extension sharing
  - validation, explicit cache reset, and `maxDampTime`
- `VirtualCamera::detachedFollowTargetDamp` and
  `detachedLookAtTargetDamp`
  - scalar and `Vec3` overloads
  - target-attachment relaxation of damping time and delta time
- `HardLockToTarget` now shares the reusable follow-target damping helper.
- `tests/unit/follow_zoom_test.cpp`
  - 5 cases and 26 assertions covering defaults, validation, FOV calculation,
    near/far clamping, Body-only updates, missing look-at points, damping,
    previous-frame cache use, target attachment, explicit reset, and invalid
    deltas.
- `examples/16_follow_zoom`
  - fixed camera, depth-sweeping rotating target, orange gauge at the target
    plane, width/damping controls, FOV HUD, and headless smoke test.
- Export, build, task index, feature matrix, architecture, and example index
  updated.

### Differences

- Unity stores extension history in `VcamExtraState`; CineRig uses a map keyed
  by virtual camera so inherited parent extensions keep independent histories.
- CineRig adds an explicit `reset(VirtualCamera&)` that seeds the cache from the
  virtual camera's base lens.
- A missing look-at point resolves to the maximum FOV, matching the original
  invalid-distance branch without propagating NaNs.
- No editor attributes, serialization, or preset integration.

### Verification

- Core Debug: **275/275 passed** (194 unit + 75 scenario + 6 golden).
- Core Release: **275/275 passed**.
- Examples Debug: **306/306 passed** (275 core + 31 example tests, including 16
  smoke tests).
- Examples Release: **306/306 passed**.
- `cinerig_16_follow_zoom --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 16 example exits 0.
- Visible window confirms the title, target distance, constant target/gauge
  ratio, FOV change, FOV range, damping mode, motion state, controls, and frame
  count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_follow_zoom.png`.
- Linux was not verified on hardware.
