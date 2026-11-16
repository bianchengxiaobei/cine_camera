# P2-02 Full Rotation Composer

Status: completed  
Depends on: P0-04, P1-02, P1-06, P2-01  
Blocks: P2 PositionComposer, PanTilt, ThirdPersonAim

## Goal

Replace the P1 approximate `RotationComposer` with the full Cinemachine
rotation-composer behavior: screen-space composition, target-local offset,
lookahead, dead zone, hard limits, activation framing, damping, orthographic
support, target warping, and force-position history updates.

## Scope

- Extend `ScreenComposerSettings` with normalized dead-zone and hard-limit
  rectangles matching the original settings.
- Expand `components::RotationComposer`:
  - `ScreenComposerSettings composition`
  - `bool centerOnActivate = true`
  - `math::Vec3 targetOffset` in target-local space
  - `math::Vec2 damping`
  - `LookaheadSettings lookahead`
  - `prePipelineMutate()` to apply target offset and lookahead before the body
  - full perspective and orthographic FOV cache
  - screen-position to angular-coordinate conversion
  - soft dead-zone tracking with damping
  - hard-limit clamping for the actual target
  - vertical clamp that prevents upside-down framing
  - smooth screen-position changes
  - target warp and force-position internal history handling
- Migrate existing examples/tests from `screenPosition` to
  `composition.screenPosition`.
- Update `01_follow` with composition presets and lookahead controls so the
  behavior can be observed in the SDL example.
- Add unit coverage for composition geometry, target-local offset, lookahead,
  activation framing, dead zone, hard limits, orthographic FOV, warp, and
  force-position.

## Non-goals

- PositionComposer, OrbitalFollow, PanTilt, ThirdPersonFollow/Aim.
- TargetGroup framing.
- Noise, collision correction, or post-processing.
- Unity serialization or inspector integration.

## API Sketch

```cpp
class RotationComposer : public CameraComponent
{
public:
    ScreenComposerSettings composition;
    bool centerOnActivate = true;
    math::Vec3 targetOffset;
    math::Vec2 damping{0.5f, 0.5f};
    LookaheadSettings lookahead;

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void prePipelineMutate(CameraState& state, float deltaTime) override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    float maxDampTime() const override;
};
```

## Behavior Details

- Screen positions are normalized: `0` is center, `-0.5` and `+0.5` are the
  edges. Positive X is screen-right and positive Y is screen-up.
- `composition.deadZoneRect()` and `composition.hardLimitsRect()` use the
  original normalized 0..1 rectangle convention (center is `0.5, 0.5`).
- The FOV cache converts screen rectangles into angular coordinates using
  `atan` and the vertical/horizontal half-FOV. Orthographic lenses derive an
  effective FOV from `orthographicSize / targetDistance`.
- The soft dead zone moves the target to the nearest dead-zone edge with
  damping. Hard limits clamp the target without damping when hard limits are
  enabled and the look-at attachment is fully attached.
- A separate vertical clamp prevents the camera from crossing the pole and
  producing upside-down spinning.
- `centerOnActivate` centers the target during a snap/activation; otherwise the
  target is placed at the dead-zone/soft guiding rectangle.
- `targetOffset` is rotated by the look-at target rotation, matching the
  original `LookAtTargetRotation * TargetOffset`.
- Lookahead uses `math::PositionPredictor`; it resets on target change or an
  invalid previous state. `ignoreY` projects the predicted delta onto the
  plane defined by the reference up.
- `deltaTime < 0`, an invalid previous state, or missing internal history snaps
  the orientation. Zero damping also snaps directly to the desired framing.
- `onTargetObjectWarped()` shifts the previous camera/look-at history and the
  predictor. `forceCameraPosition()` rotates the predictor history by the
  orientation delta and records the forced pose.
- Screen-position changes are compensated through the previous screen offset,
  avoiding a one-frame orientation jump.

## Tests

- `tests/unit/rotation_composer_test.cpp`
  - defaults and `maxDampTime()`
  - center framing and screen-position offsets
  - target-local target offset
  - activation snap / `centerOnActivate`
  - dead-zone does not move the camera while the target stays inside
  - hard-limit clamping under damping
  - lookahead prediction and `ignoreY`
  - orthographic effective FOV
  - negative delta time and invalid target
  - target warp and force-position history
- `tests/unit/screen_composer_test.cpp`
  - dead-zone and hard-limit rectangles
- Existing scenario/example tests continue to pass after API migration.
- Full core and example test suites pass.

## Acceptance Checklist

- [x] Full composer API replaces the P1 minimal fields.
- [x] Screen composition, dead zone, hard limits, and vertical clamp work.
- [x] Target-local offset and lookahead match the original behavior.
- [x] Orthographic and perspective lenses are supported.
- [x] Warp and force-position update internal history.
- [x] Existing examples/tests build and pass.
- [x] `01_follow` exposes composition and lookahead controls.
- [x] Core and example test suites pass in Debug and Release.

## Reference Project

### Source

- `com.unity.cinemachine/Runtime/Components/CinemachineRotationComposer.cs`
  - `Composition`, `CenterOnActivate`, `TargetOffset`, `Damping`, `Lookahead`
  - `PrePipelineMutate`, `MutateCameraState`
  - `FovCache`, `RotateToScreenBounds`, `ClampVerticalBounds`
  - `OnTargetObjectWarped`, `ForceCameraPosition`
- `com.unity.cinemachine/Runtime/Core/ScreenComposerSettings.cs`
  - `DeadZoneRect`, `HardLimitsRect`, `Validate`, `Lerp`
- `com.unity.cinemachine/Runtime/Core/LookaheadSettings.cs`
- `com.unity.cinemachine/Runtime/Core/Predictor.cs`
- `com.unity.cinemachine/Runtime/Core/UnityVectorExtensions.cs`
  - `GetCameraRotationToTarget`, `ApplyCameraRotation`,
    `SafeFromToRotation`, `ProjectOntoPlane`

### Documentation

- `com.unity.cinemachine/Documentation~/CinemachineRotationComposer.md`

### Differences

- CineRig uses right-handed coordinates with `-Z` forward; the original uses
  Unity's left-handed `+Z` forward. Screen Y is positive-up in CineRig.
- The horizontal angular range is sign-adjusted for the right-handed yaw
  convention; the vertical range keeps the original screen-up semantics.
- Components are explicit C++ objects installed through
  `VirtualCamera::setAim()`.
- `LookAtTargetChanged` is represented by tracking the effective look-at
  `TargetRef` inside the composer.

## Implementation Record

### Implemented

- `include/cinerig/core/screen_composer_settings.hpp`
  - Added `deadZoneRect()` and `hardLimitsRect()` using the original normalized
    rectangle convention.
- `include/cinerig/components/rotation_composer.hpp` and
  `src/components/rotation_composer.cpp`
  - `composition`, `centerOnActivate`, local `targetOffset`, `damping`, and
    `lookahead`.
  - `prePipelineMutate()` applies target-local offset and lookahead before the
    body stage.
  - Perspective/orthographic FOV cache with screen-to-angle conversion.
  - Soft dead-zone damping, hard-limit clamping, vertical pole clamp, activation
    centering, and screen-position change compensation.
  - `onTargetObjectWarped()` and `forceCameraPosition()` internal history
    updates.
- `tests/unit/rotation_composer_test.cpp`
  - Expanded to 14 composer cases covering defaults, perspective/orthographic
    framing, local offset, activation, dead zone, hard limits, lookahead,
    `ignoreY`, negative time, invalid target, warp, and force-position.
- `tests/unit/screen_composer_test.cpp`
  - Added dead-zone and hard-limit rectangle coverage.
- Migrated all `screenPosition` call sites to `composition.screenPosition`.
- `examples/01_follow`
  - Title and HUD updated to P2-02.
  - `C` cycles `Offset`, `Center`, `DeadZone`, and `HardLimits` presets.
  - `L` toggles lookahead (`0.25s`, smoothing `0.15`).
  - README and examples index updated.

### Verification

- Core Debug and Release: **183/183 passed** (102 unit + 75 scenario + 6
  golden).
- Examples Debug and Release: **201/201 passed** (18 example tests including 3
  smoke tests); with smoke disabled this is 198 tests.
- `cinerig_01_follow --frames 2 --hidden --no-vsync` exits 0.
- Visible window confirms the P2-02 title, `Composition: Offset (0.2, 0.12)`,
  `Lookahead: off`, and the target placed right/up of screen center. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_composer.png`.
- Linux was not verified on hardware.
