# P2-06 Pan Tilt

Status: completed  
Depends on: P0-04, P1-02, P2-04, P2-05  
Blocks: Input-driven FreeLook/PanTilt rigs, ThirdPersonLook

## Goal

Implement the full `components::PanTilt` aim component. It orients the camera
from pan and tilt `InputAxis` values, supports parent/world/tracking/look-at
reference frames, axis recentering, force-position axis inference, and
InputAxisController discovery.

## Scope

- Add `components::PanTilt` in the Aim stage with:
  - `ReferenceFrame::ParentObject`, `World`, `TrackingTarget`,
    `LookAtTarget`
  - `RecenterTargetMode::AxisCenter`, `TrackingTargetForward`,
    `LookAtTargetForward`
  - pan and tilt `InputAxis` members with the original ranges and defaults
  - pan/tilt rotation composition relative to the selected reference frame
  - synchronized recentering when both axis times match
  - previous-orientation rotation-damping bypass
  - target warp independence and force-position/inherit orientation inference
  - `IInputAxisOwner` names `Look X (Pan)` and `Look Y (Tilt)` with X/Y hints
- Preserve the original user-facing angle semantics: positive pan looks right
  and positive tilt looks down, adjusted for CineRig's right-handed `-Z`
  forward convention.
- Add `06_pan_tilt` SDL example with controller-driven look, reference-frame
  switching, recentering toggle, sensitivity, reset, and HUD.
- Add unit coverage for defaults, reference frames, pan/tilt placement,
  recentering targets, damping bypass, force-position, transition inheritance,
  and controller discovery.
- Update the task index, feature matrix, coordinate notes, and example index.

## Non-goals

- ThirdPersonFollow/Aim, TargetGroup, collision, noise, or post-processing.
- Unity parent transform hierarchy or serialization.
- FreeLookModifier interactions.
- Automatic per-frame controller update from CameraSystem.

## Design

### Rotation

The component sets only `CameraState::rawOrientation`; it never changes the
camera position.

For CineRig's right-handed camera forward (`-Z`), the local Euler is:

```cpp
ref * quatFromEuler({
    -tiltAxis.value * DegToRad,
    -panAxis.value * DegToRad,
    0.0f,
});
```

The sign adjustment preserves the original user-facing semantics: positive pan
looks right, positive tilt looks down. `ReferenceFrame` supplies `ref`;
`ParentObject` uses the parent virtual camera's raw orientation, or a world-up
frame when there is no parent.

### Recentering

`getRecenterTarget()` returns pan/tilt angles in degrees:

- `AxisCenter`: `(panAxis.center, tiltAxis.center)`
- `TrackingTargetForward`: convert the tracking target's world forward into the
  selected reference frame, then derive pan/tilt
- `LookAtTargetForward`: same for the look-at target

Recentering is synchronized when the two axis time values match, matching the
original implementation.

### Force and inheritance

`forceCameraPosition()` derives pan/tilt from the target orientation using the
same reference frame and sign convention. `onTransitionFromCamera()` cancels
recentering and, when InheritPosition is active, applies the incoming raw
orientation to the axes.

## API Sketch

```cpp
namespace cinerig
{

class PanTilt : public CameraComponent, public IInputAxisOwner
{
public:
    enum class ReferenceFrame
    {
        ParentObject,
        World,
        TrackingTarget,
        LookAtTarget,
    };

    enum class RecenterTargetMode
    {
        AxisCenter,
        TrackingTargetForward,
        LookAtTargetForward,
    };

    static InputAxis makeDefaultPanAxis();
    static InputAxis makeDefaultTiltAxis();

    ReferenceFrame referenceFrame = ReferenceFrame::ParentObject;
    RecenterTargetMode recenterTarget = RecenterTargetMode::AxisCenter;
    InputAxis panAxis = makeDefaultPanAxis();
    InputAxis tiltAxis = makeDefaultTiltAxis();

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    bool onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    void getInputAxes(std::vector<InputAxisDescriptor>& axes) override;

    math::Vec2 getRecenterTarget() const;
    void validate();

private:
    math::Quat getReferenceFrame(math::Vec3 up) const;
    void setAxesForRotation(math::Quat rotation, math::Vec3 up);
    math::Vec2 anglesForDirection(
        math::Vec3 worldDirection,
        math::Vec3 up) const;

    math::Quat previousRotation_{};
    bool historyValid_ = false;
};

}
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachinePanTilt.cs`
  - reference frames, recenter modes, axes, Euler composition, reset,
    force-position inference, and transitions
- `com.unity.cinemachine/Runtime/Core/InputAxis.cs`
  - axis recentering and owner interface
- `com.unity.cinemachine/Tests/Editor/InputAxisTests.cs`
  - recentering boundary behavior
- `com.unity.cinemachine/Tests/Runtime/CinemachineInputAxisControllerTests.cs`
  - PanTilt controller discovery smoke case

## Acceptance

- PanTilt exposes the two original axis names and hints.
- Pan/tilt orientation works in all four reference frames and uses the
  documented right-handed sign convention.
- Recentering targets and synchronization behave correctly.
- Rotation damping bypass, force-position inference, and InheritPosition
  transitions work.
- The controller can discover and drive both axes.
- Debug/Release builds and the existing test suite pass.

## Implementation Record

### Implemented

- `include/cinerig/components/pan_tilt.hpp` and
  `src/components/pan_tilt.cpp`
  - Aim-stage camera orientation with `ParentObject`, `World`,
    `TrackingTarget`, and `LookAtTarget` reference frames.
  - Pan/tilt `InputAxis` defaults and validation; tilt range clamped to
    `[-90, 90]`.
  - Right-handed sign adjustment so positive pan looks right and positive
    tilt looks down.
  - Synchronized recentering and `AxisCenter`, `TrackingTargetForward`, and
    `LookAtTargetForward` recenter targets.
  - Previous-orientation rotation-damping bypass.
  - Force-position axis inference and InheritPosition transition handling.
  - `IInputAxisOwner` exposure as `Look X (Pan)` and `Look Y (Tilt)`.
- `tests/unit/pan_tilt_test.cpp`
  - 7 cases and 43 assertions covering defaults/validation, right-handed
    placement, all reference frames, recenter targets, synchronized
    recentering, damping bypass, force-position, transition inheritance, and
    InputAxisController discovery.
- `examples/06_pan_tilt`
  - Controller-driven first-person look, rotating target, reference-frame
    switching, recenter-mode switching, recentering toggle, pause, reset, HUD,
    and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the new component and tests.
- `docs/tasks/README.md`, `docs/feature-matrix.md`,
  `docs/coordinate-convention.md`, `docs/architecture.md`, and
  `examples/README.md`
  - Status and coordinate/sign documentation updated.

### Differences

- CineRig has no Unity transform-parent hierarchy. `ParentObject` uses the
  parent virtual camera's raw orientation, or the world-up frame when no parent
  exists.
- The local Euler terms are sign-adjusted for the right-handed `-Z` camera
  forward so the original user-facing pan/tilt directions are preserved.
- `PanTilt` only exposes reset/cancel behavior through its own axes and
  controller reset calls; there is no Unity reset-event registration.
- The controller is updated explicitly by the example before the camera update.

### Verification

- Core Debug: **228/228 passed** (147 unit + 75 scenario + 6 golden).
- Core Release: **228/228 passed**.
- Examples Debug: **249/249 passed** (228 core + 21 example tests, including 6
  smoke tests).
- Examples Release: **249/249 passed**.
- `cinerig_06_pan_tilt --frames 2 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 06 example exits 0.
- Visible window confirms the P2-06 title, pan/tilt axes, `ParentObject`
  reference, recenter state, controller input, target, forward vector, target
  sphere, axes, and grid. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_pan_tilt.png`.
- Linux was not verified on hardware.

