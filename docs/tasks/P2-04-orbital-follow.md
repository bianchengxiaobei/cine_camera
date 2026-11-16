# P2-04 Orbital Follow and Input Axis

Status: completed  
Depends on: P0-04, P1-02, P1-06, P2-01, P2-02  
Blocks: P2 FreeLook-style rigs, ThirdPersonFollow, InputAxisController

## Goal

Implement the full Cinemachine orbital-follow body and the input-axis value
type it requires. The camera travels on a sphere or on the surface defined by
the original three-ring rig, with horizontal, vertical, and radial input axes,
recentering, tracker binding modes, force-position inference, and
InheritPosition transitions.

## Scope

- Add `core::InputAxis` with:
  - value, center, range, wrap, recentering settings, and restriction flags
  - clamped/normalized value helpers, validation, reset, and
    `DefaultMomentary`
  - value-change tracking and recentering driven by the virtual-camera time
  - trigger/cancel recentering
- Add a float `math::smoothDamp` helper used by axis recentering.
- Add `core::OrbitRigSettings` and `core::OrbitSplineCache` for the original
  three-ring Bezier surface.
- Add `components::OrbitalFollow` with:
  - target-local `targetOffset`, `TrackerSettings`, `OrbitStyle::Sphere`,
    `OrbitStyle::ThreeRing`, `radius`, `orbits`, and `recenteringTarget`
  - horizontal, vertical, and radial `InputAxis` members with the original
    defaults
  - target tracking through `Tracker`, look-at offset handling, minimum target
    distance, and rotation-damping bypass
  - lazy-follow axis reset
  - horizontal recentering center from axis center, parent object, tracking
    target, or look-at target
  - recentering synchronization when axis times match
  - target warp, force-position axis inference, and InheritPosition transitions
- Expose the camera system time on `VirtualCamera` for input-axis recentering.
- Add `05_orbital_follow` SDL example with sphere/three-ring switching, axis
  input, radial zoom, recentering, binding mode, damping, pause, and reset.
- Add unit coverage for axes, the orbit cache, sphere/three-ring placement,
  binding modes, target offset, recentering, warp, force-position, and
  transitions.
- Update the task index and feature matrix.

## Non-goals

- InputAxisController actions, input readers, or the default axis driver.
- FreeLookModifier, TargetGroup framing, collision, noise, or post-processing.
- PanTilt, ThirdPersonFollow/Aim, or other body/aim components.
- Unity serialization or inspector integration.

## API Sketch

```cpp
namespace cinerig
{

enum class InputAxisRestriction : std::uint32_t
{
    None = 0,
    RangeIsDriven = 1u << 0,
    NoRecentering = 1u << 1,
    Momentary = 1u << 2,
};

struct InputAxis
{
    float value = 0.0f;
    float center = 0.0f;
    math::Vec2 range{};
    bool wrap = false;
    InputAxisRecenteringSettings recentering{};
    InputAxisRestriction restrictions = InputAxisRestriction::None;

    float clampValue(float value) const;
    float getNormalizedValue() const;
    float getClampedValue() const;
    void validate();
    void reset();
    bool trackValueChange(float currentTime);
    void updateRecentering(float deltaTime, bool forceCancel, float currentTime);
    void updateRecentering(
        float deltaTime,
        bool forceCancel,
        float currentTime,
        float center);
    void triggerRecentering();
    void cancelRecentering();
};

struct OrbitRigOrbit
{
    float radius = 0.0f;
    float height = 0.0f;
};

struct OrbitRigSettings
{
    OrbitRigOrbit top{2.0f, 5.0f};
    OrbitRigOrbit center{4.0f, 2.25f};
    OrbitRigOrbit bottom{2.5f, 0.1f};
    float splineCurvature = 0.5f;
};

class OrbitalFollow : public CameraComponent
{
public:
    enum class OrbitStyle { Sphere, ThreeRing };
    enum class ReferenceFrame
    {
        AxisCenter,
        ParentObject,
        TrackingTarget,
        LookAtTarget,
    };

    math::Vec3 targetOffset{};
    TrackerSettings trackerSettings{};
    OrbitStyle orbitStyle = OrbitStyle::Sphere;
    float radius = 10.0f;
    OrbitRigSettings orbits{};
    ReferenceFrame recenteringTarget = ReferenceFrame::TrackingTarget;
    InputAxis horizontalAxis = makeDefaultHorizontalAxis();
    InputAxis verticalAxis = makeDefaultVerticalAxis();
    InputAxis radialAxis = makeDefaultRadialAxis();

    Stage stage() const override { return Stage::Body; }
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

}
```

## Behavior Details

- The default horizontal axis is `value = 0`, `center = 0`,
  `range = [-180, 180]`, and wrapping; the default vertical axis is
  `value = center = 17.5`, `range = [-10, 45]`; the default radial axis is
  `value = center = 1`, `range = [1, 1]`. Recentering is disabled by default.
- Sphere placement uses
  `quatFromEuler({verticalDegrees, horizontalDegrees, 0}) * (0, 0, -radius *
  radialScale)`, matching Unity's `Quaternion.Euler(y, x, 0)` offset.
- ThreeRing placement uses the cached Bezier surface. The normalized vertical
  axis selects `t` from bottom (`0`) through center (`0.5`) to top (`1`).
  Radial scaling multiplies the spline point; horizontal rotation is around the
  reference up.
- `LazyFollow` clamps the local Z offset to the negative side and resets the
  horizontal axis every frame. Its horizontal center is zero.
- The target point is `followPosition + followRotation * targetOffset`. The
  tracker receives the orbit offset; after tracking, the body applies the
  damped reference orientation to the offset and updates `referenceUp`.
- When a look-at target exists, the difference between the look-at point and
  the target point is removed from the orbit offset so an Aim component's
  composition does not create spurious damping.
- The rotation bypass compares the current offset with the previous frame's
  axis offset, both relative to the look-at point. It is only active with a
  valid previous state and internal axis history.
- The horizontal center for recentering is computed from the selected
  reference frame. `AxisCenter` keeps the center static, except that LazyFollow
  clears it. The other frames align the center behind the selected forward
  vector.
- Recentering is updated after all three axes have been sampled. Axes with the
  same recentering time share input cancellation so a combined input gesture
  does not recenter one axis while another is active.
- `deltaTime < 0` or an invalid previous state snaps target tracking. Missing
  internal axis history only disables the rotation-damping bypass; target
  changes are handled by `Tracker` and retain the previous tracked position.
- `onTargetObjectWarped()` shifts the tracker when the tracking target moved by
  the same delta.
- `forceCameraPosition()` rebuilds tracker history and infers sphere axes from
  the forced camera direction, or the closest three-ring vertical value using
  the original steepest-descent search. It leaves the radial axis alone.
- `onTransitionFromCamera()` cancels recentering and, for an InheritPosition
  transition that is not already live, forces the incoming camera position and
  infers axes from it.
- Axis values are clamped for evaluation. Radius is non-negative; radial scale
  is clamped to its range.
- CineRig has no transform parent hierarchy. `ReferenceFrame::ParentObject`
  uses the parent virtual camera's final orientation when available, otherwise
  world forward.

## Tests

- `tests/unit/input_axis_test.cpp`
  - defaults, clamping, wrapping, normalized values, validation, reset, and
    momentary axes
  - value-change tracking, recentering wait/time, wrap shortest path,
    trigger/cancel, and disabled/no-recentering restrictions
  - float smooth damp behavior
- `tests/unit/orbital_follow_test.cpp`
  - default settings, stage, validity, and `maxDampTime()`
  - sphere placement at default and modified axes/radius/radial scale
  - three-ring placement at bottom/center/top and cache invalidation
  - binding modes and target-local offset
  - lazy follow side and horizontal reset
  - horizontal recentering from tracking target and combined-axis cancellation
  - target warp, force-position sphere/three-ring inference, and InheritPosition
  - invalid target and negative delta time
- Existing core/scenario/example tests continue to pass.
- Full core and example test suites pass in Debug and Release.

## Acceptance Checklist

- [x] `InputAxis` clamps, wraps, recenters, and honors restrictions.
- [x] Sphere and three-ring orbital surfaces match the original formulas.
- [x] Tracker binding, offsets, look-at handling, and minimum distance work.
- [x] Force-position and InheritPosition infer axes without jumps.
- [x] `05_orbital_follow` demonstrates the behaviors in SDL.
- [x] Core and example tests pass in Debug and Release.

## Reference Project

### Source

- `com.unity.cinemachine/Runtime/Components/CinemachineOrbitalFollow.cs`
  - `OrbitStyles`, `Radius`, `Orbits`, `ReferenceFrames`, `RecenteringTarget`
  - `HorizontalAxis`, `VerticalAxis`, `RadialAxis`, `AxisValues`,
    `GetCameraPoint`
  - `MutateCameraState`, `UpdateHorizontalCenter`, `OnTransitionFromCamera`,
    `ForceCameraPosition`, `InferAxesFromPosition_Sphere`,
    `InferAxesFromPosition_ThreeRing`
- `com.unity.cinemachine/Runtime/Core/InputAxis.cs`
  - `ClampValue`, `GetNormalizedValue`, `GetClampedValue`, `Validate`, `Reset`
  - `TrackValueChange`, `UpdateRecentering`, `TriggerRecentering`,
    `CancelRecentering`, `DefaultMomentary`
- `com.unity.cinemachine/Runtime/Core/SplineHelpers.cs`
  - `Bezier1`, `Bezier3`, `ComputeSmoothControlPoints`
- `com.unity.cinemachine/Runtime/Core/TargetTracking.cs`
  - `Tracker.TrackTarget`, `GetReferenceOrientation`,
    `GetOffsetForMinimumTargetDistance`, `OnForceCameraPosition`
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`

### Tests

- `com.unity.cinemachine/Tests/Runtime/FreelookForcePositionTests.cs`
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
- `com.unity.cinemachine/Tests/Editor/InputAxisTests.cs`

### Documentation

- `com.unity.cinemachine/Documentation~/CinemachineOrbitalFollow.md`

### Differences

- CineRig uses right-handed coordinates with `-Z` forward. The sphere offset
  keeps the Unity `-Z` direction because it is a target-relative back offset;
  orbit angles and the three-ring knots map directly.
- `InputAxis` takes the camera system time explicitly instead of using a global
  unscaled-time clock.
- CineRig has no transform parent hierarchy; parent-object recentering uses the
  parent virtual camera or world forward.
- Axis input is supplied directly by the host/example. The input-axis
  controller and default driver are separate follow-up work.

## Implementation Record

### Implemented

- `include/cinerig/core/input_axis.hpp` and `src/core/input_axis.cpp`
  - `InputAxis`, recentering settings, and restriction flags.
  - Wrapping/clamping, normalized values, validation, reset, momentary defaults,
    value-change tracking, recentering, and trigger/cancel.
- `include/cinerig/core/orbit_rig.hpp` and `src/core/orbit_rig.cpp`
  - `OrbitRigSettings`, default three-ring values, and `OrbitSplineCache` with
    smooth Bezier control points and bottom/center/top sampling.
- `include/cinerig/components/orbital_follow.hpp` and
  `src/components/orbital_follow.cpp`
  - Sphere and three-ring surfaces with horizontal/vertical/radial axes.
  - Tracker binding, target-local offset through the tracker reference
    orientation, look-at offset handling, minimum target distance, and
    rotation-damping bypass.
  - Lazy-follow horizontal reset and axis side clamping.
  - Tracking-target, look-at-target, parent-camera, and fixed-axis recentering
    centers, plus synchronized recentering cancellation.
  - Target warp, force-position axis inference (sphere and three-ring), and
    InheritPosition transitions.
  - Actual target point tracking after force-position so forced and inherited
    poses stay on the orbit surface.
- `include/cinerig/camera/virtual_camera.hpp`
  - `VirtualCamera::currentTime()` exposes the camera system clock for input
    recentering.
- `include/cinerig/core/tracker.hpp`
  - `Tracker::setPreviousTargetPosition()` for force-position target-point
    recovery.
- `include/cinerig/cinerig.hpp` and `CMakeLists.txt`
  - Export and build the new core and component files.
- `tests/unit/input_axis_test.cpp`
  - 5 cases covering clamping/wrapping, momentary/default reset, wait/time
    recentering, wrap shortest-path recentering, restrictions, and negative
    time.
- `tests/unit/orbital_follow_test.cpp`
  - 10 cases covering cache endpoints/invalidation, defaults, sphere and
    three-ring placement, binding modes, target offset, lazy follow,
    recentering, warp, sphere/three-ring force-position, InheritPosition,
    negative time, and invalid targets.
- `examples/05_orbital_follow`
  - Sphere/three-ring switching, A/D and W/S orbit input, Q/E/wheel radial
    zoom, recentering, binding modes, damping modes, pause/reset, orbit ring
    visualization, HUD, and headless smoke test.

### Verification

- Core Debug: **214/214 passed** (133 unit + 75 scenario + 6 golden).
- Core Release: **214/214 passed**.
- Examples Debug: **234/234 passed** (214 core + 20 example tests, including 5
  smoke tests); with smoke disabled this is 229 tests.
- Examples Release: **234/234 passed**.
- `cinerig_05_orbital_follow --frames 2 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 05 example exits 0.
- Visible window confirms the P2-04 title, `Axes H ~25 / V 22 / R 1`,
  `Orbit Sphere (r 10)`, `Binding WorldSpace`, `Damping low`, `Recenter on
  (tracking target)`, and a centered target with the orbit ring. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_orbital.png`.
- Linux was not verified on hardware.
