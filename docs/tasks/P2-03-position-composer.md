# P2-03 Full Position Composer

Status: completed  
Depends on: P0-04, P1-02, P1-06, P2-01, P2-02  
Blocks: P2 ThirdPersonFollow, FreeLook-style rigs using PositionComposer

## Goal

Implement the full Cinemachine position-composer body component. It maintains a
screen-space relationship to the tracking target by moving the camera along its
local Z axis and X-Y plane, without changing orientation. It must run after the
Aim stage so it can use the final camera orientation.

## Scope

- Add `components::PositionComposer` with:
  - `float cameraDistance = 10.0f`
  - `float deadZoneDepth = 0.0f`
  - `ScreenComposerSettings composition`
  - `bool centerOnActivate = true`
  - `math::Vec3 targetOffset` in target-local space
  - `math::Vec3 damping{1.0f, 1.0f, 1.0f}`
  - `LookaheadSettings lookahead`
  - body-stage registration with `bodyAppliesAfterAim() == true`
- Implement perspective and orthographic screen-space positioning:
  - move along camera local Z to maintain `cameraDistance` with an optional
    `deadZoneDepth` band
  - convert normalized dead-zone/hard-limit rectangles to camera-local units
  - snap on activation or missing history
  - damp the soft-zone correction per camera-space axis
  - hard-clamp the real tracking point when fully attached
  - compensate changes to `composition.screenPosition` without damping
- Apply target-local `targetOffset` using the tracking target's rotation.
- Add lookahead prediction with target-change reset and `ignoreY`.
- Handle target warp, force-position, and InheritPosition transitions.
- Add `04_position_composer` SDL example with a fixed-orientation camera,
  orthographic projection, composition presets, damping modes, target offset,
  pause, and reset.
- Add unit coverage for default values, framing, depth, dead zone, hard limits,
  orthographic projection, lookahead, target offset, invalid targets, warp, and
  force-position.
- Update the task index and feature matrix.

## Non-goals

- RotationComposer changes, OrbitalFollow, PanTilt, ThirdPersonFollow/Aim.
- TargetGroup framing, collision correction, noise, or post-processing.
- Unity serialization or inspector integration.

## API Sketch

```cpp
class PositionComposer : public CameraComponent
{
public:
    float cameraDistance = 10.0f;
    float deadZoneDepth = 0.0f;
    ScreenComposerSettings composition;
    bool centerOnActivate = true;
    math::Vec3 targetOffset{};
    math::Vec3 damping{1.0f, 1.0f, 1.0f};
    LookaheadSettings lookahead;

    Stage stage() const override { return Stage::Body; }
    bool bodyAppliesAfterAim() const override { return true; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    bool onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    float maxDampTime() const override;
};
```

## Behavior Details

- The component is a Body component but applies after Aim. The pipeline already
  supports deferred post-aim body execution.
- The tracking point is `followTargetPosition + followTargetRotation *
  targetOffset`. Lookahead predicts this point, not the raw target.
- `state.referenceLookAt` is left alone unless it is missing or still equals the
  raw tracking target; this mirrors the original component and avoids
  overwriting an Aim component's look-at result.
- Camera-local forward is `-Z`. Target depth is therefore `-targetPos.z`; the
  desired depth is clamped to
  `[cameraDistance - deadZoneDepth/2, cameraDistance + deadZoneDepth/2]`.
- `screenToOrtho()` uses the normalized rectangle directly:
  `x = 2 * size * aspect * (screenX - 0.5)`,
  `y = 2 * size * (screenY - 0.5)`.
  C++ screen Y is positive-up, matching the P2-02 screen convention. The
  original Unity component flips Y in this conversion because its screen-space
  Y convention differs.
- Soft corrections are damped with `DetachedFollowTargetDamp` semantics:
  `damping` is per camera-space axis and is blended toward a minimum of one
  second as follow attachment decreases.
- Hard limits clamp the real target after soft damping when hard limits are
  enabled and `followTargetAttachment` is fully attached. Activation snap uses
  only the soft guide rectangle.
- `centerOnActivate` snaps to the center of the soft guide on activation unless
  position was inherited from an outgoing camera. Otherwise the snap uses the
  soft guide edges.
- A change to `composition.screenPosition` moves the previous camera position
  immediately by the corresponding local X/Y offset and moves the local target
  point in the opposite direction. This prevents the change from being damped
  and avoids a one-frame composition jump.
- `cameraDistance` is clamped to at least `0.01`; `deadZoneDepth` and damping are
  clamped to non-negative values during evaluation.
- `deltaTime < 0`, an invalid previous state, or missing internal history snaps.
  Zero damping also snaps directly to the desired framing.
- Target changes reset the predictor. Target warp shifts the previous camera
  position and predictor. Force-position rotates the predictor and records the
  forced pose/history.
- `onTransitionFromCamera()` marks position as inherited when the incoming
  virtual camera has `BlendHint::InheritPosition`, so activation centering does
  not discard the inherited position.

## Tests

- `tests/unit/position_composer_test.cpp`
  - defaults, stage, post-aim body flag, and `maxDampTime()`
  - center framing and target distance on the first update
  - perspective screen-position framing
  - `deadZoneDepth` avoids Z correction inside its band
  - target-local offset
  - `centerOnActivate` versus soft-guide activation
  - dead-zone hold
  - hard-limit clamping with damping
  - screen-position change immediate compensation
  - perspective and orthographic framing
  - lookahead prediction and `ignoreY`
  - negative delta time and invalid target
  - target warp and force-position history
  - InheritPosition transition behavior
- Pipeline test coverage continues to confirm post-aim Body ordering.
- Existing test suites continue to pass.
- Full core and example test suites pass in Debug and Release.

## Acceptance Checklist

- [x] Component is registered as a post-Aim Body component.
- [x] Perspective and orthographic screen composition work.
- [x] Camera distance and depth dead zone work.
- [x] Dead zone, hard limits, screen-position compensation, and target offset
      work.
- [x] Lookahead, warp, force-position, and InheritPosition are handled.
- [x] `04_position_composer` demonstrates the behavior in SDL.
- [x] Core and example tests pass in Debug and Release.

## Reference Project

### Source

- `com.unity.cinemachine/Runtime/Components/CinemachinePositionComposer.cs`
  - `CameraDistance`, `DeadZoneDepth`, `Composition`, `CenterOnActivate`,
    `TargetOffset`, `Damping`, `Lookahead`
  - `BodyAppliesAfterAim`, `MutateCameraState`
  - `ScreenToOrtho`, `OrthoOffsetToScreenBounds`
  - `OnTransitionFromCamera`, `OnTargetObjectWarped`, `ForceCameraPosition`
- `com.unity.cinemachine/Runtime/Core/ScreenComposerSettings.cs`
  - `DeadZoneRect`, `HardLimitsRect`
- `com.unity.cinemachine/Runtime/Core/LookaheadSettings.cs`
- `com.unity.cinemachine/Runtime/Core/Predictor.cs`
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - `DetachedFollowTargetDamp`

### Documentation

- `com.unity.cinemachine/Documentation~/CinemachinePositionComposer.md`
- `com.unity.cinemachine/Documentation~/Cinemachine2D.md`

### Differences

- CineRig uses right-handed coordinates with `-Z` forward, and normalized screen
  Y is positive-up. Depth and screen-to-ortho Y signs are adapted accordingly.
- The screen-position change compensation updates the local target point in the
  opposite direction of the camera move, so the requested composition change is
  immediate while target motion remains damped.
- Missing internal history forces a snap even if `previousStateIsValid` is true.
- Components are explicit C++ objects installed through
  `VirtualCamera::setBody()`.
- CineRig has no `CinemachineCore.IsLiveInBlend`; the component only checks that
  the camera is not currently live.

## Implementation Record

### Implemented

- `include/cinerig/components/position_composer.hpp` and
  `src/components/position_composer.cpp`
  - Full public settings: `cameraDistance`, `deadZoneDepth`, `composition`,
    `centerOnActivate`, local `targetOffset`, per-axis `damping`, and
    `lookahead`.
  - `Stage::Body` with `bodyAppliesAfterAim() == true`.
  - Camera-local Z distance clamping with the `deadZoneDepth` band.
  - Perspective and orthographic normalized screen-to-ortho conversion.
  - Soft-guide snap/damping, hard-limit clamping, target-local offset,
    activation centering, and undamped screen-position changes.
  - Detached follow-target damping semantics with per-axis attachment blending.
  - Lookahead prediction with target-change reset and `ignoreY`.
  - Target warp, force-position, and InheritPosition transition handling.
- `include/cinerig/cinerig.hpp` and `CMakeLists.txt`
  - Export and build the new component.
- `tests/unit/position_composer_test.cpp`
  - 16 cases covering defaults/post-Aim registration, first-update distance,
    perspective framing, depth dead zone, target offset, activation soft guide,
    dead-zone hold, hard limits, immediate screen-position changes, orthographic
    framing, lookahead, `ignoreY`, negative time, invalid targets, warp,
    force-position, and InheritPosition transitions.
- `examples/04_position_composer`
  - Fixed-orientation camera with a post-Aim PositionComposer.
  - Composition presets, damping modes, lookahead, target offset, projection
    toggle, pause/reset, HUD, and headless smoke registration.
  - Example README and examples index updated.

### Verification

- Core Debug: **199/199 passed** (118 unit + 75 scenario + 6 golden).
- Core Release: **199/199 passed**.
- Examples Debug: **218/218 passed** (199 core + 19 example tests, including 4
  smoke tests); with smoke disabled this is 214 tests.
- Examples Release: **218/218 passed**.
- `cinerig_04_position_composer --frames 2 --hidden --no-vsync` exits 0.
- `cinerig_01_follow --frames 2 --hidden --no-vsync` still exits 0.
- Visible window confirms the P2-03 title, `Composition: Offset (0.2, 0.1)`,
  `Lookahead: off`, `Target offset: off`, and the target placed right/up of
  screen center. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_position_composer.png`.
- `build/examples-no-tests` Debug builds and its examples run headless.
- Linux was not verified on hardware.
