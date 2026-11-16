# P4-06 SplineRoll

Status: completed  
Depends on: P4-03, P4-04, P4-05  
Blocks: P4-07 SplineDollyLookAtTargets

## Goal

Port the data and evaluation behavior of `CinemachineSplineRoll`: a roll-in-
degrees profile keyed along a spline, with linear or smoothstep easing. Apply
the evaluated roll around the spline-local forward axis to SplineDolly and
SplineCart frames.

## Scope

- Add `components::SplineRoll`:
  - normalized roll keys in `[0, 1]`
  - linear interpolation
  - smoothstep easing (`3t^2 - 2t^3`)
  - endpoint clamping
  - validation that clamps keys and sorts them by position
  - roll application around the spline-local forward axis
- Add an optional non-owning `SplineRoll*` to `SplineDolly` and `SplineCart`:
  - Dolly applies roll before offset axes and rotation-mode selection
  - Cart applies roll to the written entity rotation
  - null disables roll
- Add unit tests for defaults/reset, interpolation/easing, endpoint clamping,
  validation, quaternion application, and Dolly/Cart integration.
- Add a `28_spline_roll` SDL example with a visible rolled dolly frustum and
  cart frame, easing/roll toggles, HUD, and smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- `SplineDollyLookAtTargets` (P4-07) and `SplineSmoother` (P4-08).
- Unity `MonoBehaviour` lookup, local-versus-spline roll caches, and
  `ISerializationCallbackReceiver` legacy inversion.
- Unity `SplineData<T>` path-index modes. CineRig roll keys use normalized
  positions only.
- Editor gizmos, serialization, and `SaveDuringPlay`.
- A wrapping roll profile at the closed-spline seam. Endpoints clamp; callers
  should match the first and last values when a continuous loop is desired.

## Design

```text
normalized spline position
  find the surrounding roll keys (or clamp to an endpoint)
  choose linear or smoothstep interpolation
  roll radians -> rotation around the spline-local forward axis
  rolledRotation = baseRotation * localForwardRoll
```

Roll is applied before SplineDolly offset axes are computed, matching the
original `EvaluateSplineWithRoll` order. SplineCart uses the same order when
writing its entity rotation. The roll object is non-owning; callers choose
whether it represents a global spline profile or a per-cart/camera profile.

## API Sketch

```cpp
class SplineRoll
{
public:
    struct RollData
    {
        float position = 0.0f;
        float value = 0.0f;
    };

    bool easing = true;
    std::vector<RollData> roll;

    void reset();
    void validate();
    bool empty() const noexcept;

    float evaluate(float normalizedPosition) const;
    math::Quat apply(
        math::Quat rotation,
        float normalizedPosition) const;
};
```

`RollData::value` is in degrees. `apply()` multiplies the base rotation by a
roll quaternion on the right, so the rotation is around the local spline
forward axis.

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineSplineRoll.cs`
  - `RollData`, easing and linear interpolators, reset, and data ordering
- `com.unity.cinemachine/Runtime/Core/SplineContainerExtensions.cs`
  - `LocalEvaluateSplineWithRoll` and local-forward roll application
- `com.unity.cinemachine/Runtime/Components/CinemachineSplineDolly.cs`
  - evaluation order and offset axes
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineSplineCart.cs`
  - cart transform evaluation and roll cache use

## Acceptance

- Default `SplineRoll` has easing enabled and no keys; evaluation returns `0`
  and application returns the base rotation unchanged.
- Linear interpolation returns the exact keyed values and linear midpoints.
- Easing uses smoothstep and returns the exact keyed values at key positions.
- Positions before the first/after the last key clamp to the endpoint values.
- `validate()` clamps positions to `[0, 1]` and sorts keys.
- Applying a non-zero roll changes the frame orientation by the requested
  angle around forward while preserving the forward axis.
- A Dolly with roll uses rolled offset axes and updates `ReferenceUp` in
  `Spline` rotation mode.
- A Cart with roll writes the rolled tangent/up rotation.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/spline_roll.hpp` and
  `src/components/spline_roll.cpp`
  - normalized `RollData` keys in degrees
  - linear and smoothstep interpolation
  - endpoint clamping and NaN handling
  - validation that clamps positions and sorts keys
  - local-forward roll composition
- `SplineDolly` and `SplineCart` optional non-owning `SplineRoll*`
  - Dolly applies roll before offset axes and rotation-mode selection
  - Cart writes the rolled tangent/up rotation
  - null disables roll, and reset clears the pointer
- `tests/unit/spline_roll_test.cpp`
  - 7 cases and 34 assertions covering defaults/reset, linear/eased
    interpolation, endpoint clamping, validation, quaternion application, and
    Dolly/Cart integration
- `examples/28_spline_roll`
  - six-key eased roll profile, moving dolly camera and cart, rolled frustum
    and cart axes, pause/easing/roll toggles, overview camera, HUD, and smoke
    test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Roll data uses normalized positions only. Unity `SplineData<T>` path-index
  modes are not modeled.
- Roll is a non-owning `SplineRoll*` on each consumer. There is no component
  lookup or local-versus-spline cache; callers choose whether the profile is
  shared globally or scoped to one consumer.
- `validate()` sorts and clamps keys as a convenience. The original runtime
  contract requires strictly increasing keys and does not reorder them.
- Roll is applied around CineRig's local forward axis (`Vec3::forward`, `-Z`) so
  that the composition matches the camera viewing direction. Unity uses its
  local `+Z` forward basis.
- Closed splines clamp at the endpoint keys instead of wrapping across the
  seam. A continuous loop should have matching first/last values.
- Unity's legacy roll inversion, `MonoBehaviour` lookup, gizmos, serialization,
  and SaveDuringPlay are not ported.
- P4-06 only supplies roll data and integration; P4-07 adds the look-at target
  consumer and P4-08 adds spline smoothing.

### Verification

- `[spline-roll]`: **7 cases / 34 assertions passed**.
- Core Debug and Release: **389/389 passed**
  (308 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **432/432 passed**
  (389 core + 43 example tests, including 28 smoke tests).
- `cinerig_28_spline_roll --frames 180 --hidden --no-vsync` exits 0 in Debug
  and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/components/spline_roll.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, and ran roll evaluation
  and application successfully.
- Visible overview shows the blue spline, orange rolled cart, purple rolled
  dolly frustum, profile toggles, and HUD. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_spline_roll.png`.
- Windows was verified; Linux was not tested on hardware.
