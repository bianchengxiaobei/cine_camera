# P4-04 SplineDolly

Status: completed  
Depends on: P1-02, P4-03  
Blocks: P4-05 SplineCart and later spline features

## Goal

Port `CinemachineSplineDolly` and `SplineAutoDolly`. The body component places
a virtual camera on a spline, applies a spline-local offset, chooses the camera
rotation mode, and optionally damps the spline position, offset, and rotation.
Automatic dolly supports constant-speed travel and nearest-point-to-target
positioning.

## Scope

- Add `core::SplineAutoDolly`:
  - enabled flag and replaceable `ISplineAutoDolly` method
  - fixed-speed travel in the current position units
  - nearest-point-to-target with position offset and configurable search
    resolution/iteration
  - validation, reset, and tracking-target requirement reporting
- Extend `ISpline::closestPoint` with a resolution/iteration overload. The
  default implementation samples the normalized spline; `BezierSpline`
  overrides it so the automatic-dolly settings have a concrete effect.
- Add `components::SplineDolly` deriving from `CameraComponent`:
  - normalized default position units, position, and spline
  - spline-local X/Y/Z offset
  - `Default`, `Spline`, `SplineNoRoll`, `FollowTarget`, and
    `FollowTargetNoRoll` rotation modes
  - spline-direction damping with closed-spline shortest-path handling
  - spline-local X/Y offset damping
  - angular damping
  - previous spline position/position/rotation state
  - validation, reset, `Body` stage, and maximum damping time
- Add unit tests for defaults, manual interpolation in all units, fixed-speed
  and nearest-point auto-dolly, offsets, rotation modes, damping/snap behavior,
  and validation.
- Add a `26_spline_dolly` SDL example with a visible spline, moving target,
  automatic/manual modes, unit and damping controls, camera frustum, HUD, and
  smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- `SplineRoll`, `SplineCart`, `SplineDollyLookAtTargets`, and
  `SplineSmoother`. Those are separate tasks, completed in P4-05 through P4-08.
- Unity `SplineContainer`, `Transform` scale baking, `CachedScaledSpline`, and
  editor-only legacy upgrade fields.
- Passing the originating `MonoBehaviour` to `ISplineAutoDolly`. CineRig's
  built-in methods are context-free; custom methods may capture their own
  context.
- Editor serialization of `SerializeReference` methods, gizmos, and
  SaveDuringPlay.
- Exact Unity Splines nearest-point sampling order. Search resolution and
  iterations are honored, but the CineRig spline samples arc-length parameters.

## Design

```text
Body stage
  standardize the configured spline position
  snap/cache previous spline position, camera position, and rotation
  run the automatic-dolly method when enabled
  damp movement along the spline (closed loop uses the shortest path)
  update SplineSettings.position
  evaluate spline position and rotation
  add the spline-local offset and damp X/Y camera displacement
  choose the rotation mode and damp angular motion
  write RawPosition/RawOrientation and ReferenceUp
```

The spline is world-space as established by P4-03. The component's `Default`
rotation mode uses the incoming camera-state forward and `ReferenceUp`, which
is the CineRig equivalent of the original virtual-camera transform fallback.
`Spline` uses the spline tangent/up orientation; the `NoRoll` variants rebuild
a look rotation from the desired forward and the current reference up.

## API Sketch

```cpp
namespace cinerig
{

struct SplineAutoDolly
{
    class ISplineAutoDolly
    {
    public:
        virtual ~ISplineAutoDolly() = default;
        virtual void validate() {}
        virtual void reset() {}
        virtual bool requiresTrackingTarget() const = 0;
        virtual float getSplinePosition(
            const ISpline& spline,
            const std::optional<math::Vec3>& targetPosition,
            float currentPosition,
            PathIndexUnit positionUnits,
            float deltaTime) = 0;
    };

    class FixedSpeed : public ISplineAutoDolly { /* speed */ };
    class NearestPointToTarget : public ISplineAutoDolly
    {
    public:
        float positionOffset = 0.0f;
        int searchResolution = 4;
        int searchIteration = 2;
    };

    bool enabled = false;
    std::shared_ptr<ISplineAutoDolly> method;

    void validate();
    void reset();
    bool requiresTrackingTarget() const;
    float getSplinePosition(/* same arguments as the method */) const;
};

class SplineDolly : public CameraComponent
{
public:
    enum class RotationMode
    {
        Default,
        Spline,
        SplineNoRoll,
        FollowTarget,
        FollowTargetNoRoll,
    };

    struct DampingSettings
    {
        bool enabled = false;
        math::Vec3 position{};
        float angular = 0.0f;
    };

    SplineSettings spline{nullptr, 0.0f, PathIndexUnit::Normalized};
    math::Vec3 splineOffset{};
    RotationMode cameraRotation = RotationMode::Default;
    DampingSettings damping{};
    SplineAutoDolly automaticDolly{};

    void reset();
    void validate();
    bool isValid() const override;
    Stage stage() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    float maxDampTime() const override;
};

}
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineSplineDolly.cs`
  - defaults, standardize/auto-dolly order, shortest-path damping, offset axes,
    rotation modes, angular damping, reference-up behavior, and validation
- `com.unity.cinemachine/Runtime/Core/SplineAutoDolly.cs`
  - method interface, fixed speed, nearest point, search settings, and offset
- `com.unity.cinemachine/Tests/Runtime/SplineDollyCameraTest.cs`
  - closed linear-square distance/normalized/knot positions and unit changes

## Acceptance

- Defaults match the original dolly: normalized units, zero position/offset,
  `Default` rotation, disabled damping/auto-dolly, and zero damping values.
- The closed square from the original test evaluates distance `0.5` to
  `(7.5, 1, -6)`, distance `10` to `(13, 1, -2)`, normalized `0.5` to
  `(13, 1, 1)`, and knot `1.5` to `(13, 1, -2.5)`.
- Fixed-speed auto-dolly advances by `speed * deltaTime` and does not advance
  for non-positive delta time.
- Nearest-point auto-dolly places the camera at the closest spline point,
  converts units correctly, and applies the position offset.
- A positive spline offset uses the spline-local right/up/forward axes.
- `Spline` rotation follows the spline tangent/up; non-default modes update
  `ReferenceUp`; missing follow targets fall back to default.
- Damping snaps on negative time or invalid previous state, and uses the
  shortest path on closed splines.
- Validation clamps damping values to `[0, 20]` and search settings to at
  least `1`.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/core/spline_auto_dolly.hpp` and
  `src/core/spline_auto_dolly.cpp`
  - `SplineAutoDolly` enabled flag and replaceable `ISplineAutoDolly` method
  - `FixedSpeed` travel in the active position units
  - `NearestPointToTarget` with offset, resolution, and iteration settings
  - validation, reset, and tracking-target reporting
- `ISpline::closestPoint(worldPosition, searchResolution, searchIterations)`
  with a generic normalized-parameter fallback; `BezierSpline` overrides it so
  automatic-dolly search settings affect the concrete curve.
- `include/cinerig/components/spline_dolly.hpp` and
  `src/components/spline_dolly.cpp`
  - normalized default units, position, spline, and zero spline-local offset
  - `Default`, `Spline`, `SplineNoRoll`, `FollowTarget`, and
    `FollowTargetNoRoll` rotation modes
  - automatic-dolly evaluation before damping
  - spline-direction damping with closed-loop shortest-path wrapping
  - spline-local X/Y offset damping and angular damping
  - previous spline position, camera position, and rotation caches
  - reference-up updates only for non-default rotation modes
  - validation, reset, `Body` stage, and maximum damping time
- `tests/unit/spline_dolly_test.cpp`
  - 10 cases and 86 assertions covering defaults/reset, validation, all units,
    fixed speed, nearest target, offsets, rotation modes, damping, closed-loop
    shortest path, negative-time snapping, and angular damping
- `examples/26_spline_dolly`
  - smooth closed track, moving interior target, nearest-point automatic mode,
    constant-motion manual mode, unit/damping controls, spline camera frustum,
    overview camera, HUD, and smoke test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Splines remain world-space and non-owning; Unity `SplineContainer` transform
  baking, `NativeSpline`, and cache lifecycle are adapter concerns.
- P4-04 itself uses the tangent/up frame without authored roll. P4-06 adds an
  optional `SplineRoll` pointer that is applied before this component's offset
  axes; `SplineNoRoll` still re-levels the frame to `ReferenceUp`.
- `Default` rotation uses the incoming camera-state forward and
  `ReferenceUp`. In CineRig the virtual-camera owner transform is also the
  camera-state backing store, so there is no separate authored transform read.
- `ISplineAutoDolly` does not receive a sender `MonoBehaviour`. Custom methods
  may capture their own context; the built-in methods need none.
- CineRig has no play-mode gate, so `FixedSpeed` advances whenever delta time
  is positive instead of checking `Application.isPlaying`.
- Nearest-point search samples arc-length normalized positions rather than
  Unity Splines' native parameter search, so exact sample order and worst-case
  results can differ.
- `reset()` also clears the auto-dolly enabled flag and cached previous state.
  The original editor `Reset()` only cleared the method.
- There is no component enable/disable lifecycle. Callers explicitly invoke
  `automaticDolly.reset()` when a stateful custom method needs it.
- Editor serialization of `SerializeReference` methods, gizmos,
  SaveDuringPlay, and legacy upgrade fields are not ported.

### Verification

- `[spline-dolly]`: **10 cases / 86 assertions passed**.
- Core Debug and Release: **376/376 passed**
  (295 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **417/417 passed**
  (376 core + 41 example tests, including 26 smoke tests).
- `cinerig_26_spline_dolly --frames 180 --hidden --no-vsync` exits 0 in Debug
  and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/core/spline_auto_dolly.hpp` and
  `cinerig/components/spline_dolly.hpp`. An external `find_package(CineRig)`
  consumer compiled, linked, and ran `SplineDolly` plus nearest-point
  auto-dolly successfully.
- Visible overview shows the blue closed spline, green target, orange dolly
  frustum, unit/damping controls, and HUD. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_spline_dolly.png`.
- Windows was verified; Linux was not tested on hardware.
