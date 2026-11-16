# P4-07 SplineDollyLookAtTargets

Status: completed  
Depends on: P1-02, P4-03, P4-04, P4-06  
Blocks: P4-08 SplineSmoother

## Goal

Port `CinemachineSplineDollyLookAtTargets`. The Aim-stage component looks at
world points or entity-relative points keyed along the spline position used by
a SplineDolly body, with per-key easing.

## Scope

- Add `components::SplineDollyLookAtTargets`:
  - normalized look-at keys with per-item easing
  - entity targets plus world/entity-relative offsets
  - cubic interpolation with the original per-item control-point easing
  - endpoint clamping
  - associated SplineDolly lookup from the `Body` stage
  - aim orientation from the body position to the interpolated target
  - `ReferenceLookAt` updates
  - parallel-look-direction up-vector fallbacks
  - validation and reset
- Add unit tests for defaults/reset, interpolation, easing, world targets,
  entity offsets, endpoint clamping, up fallbacks, and pipeline order.
- Add a `29_spline_look_at` SDL example with a moving dolly camera, interpolated
  target markers, aim frustum, target/easing controls, HUD, and smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- `SplineSmoother`. That is P4-08.
- Unity `SplineData<T>` path-index modes. CineRig look-at keys use normalized
  positions only.
- Unity transform hierarchy and local scale. Entity offsets are rotated into
  world space but entity scale is ignored.
- Editor serialization, gizmos, `SaveDuringPlay`, and component lookup by
  GameObject.
- Camera-relative damping or smoothing of the aim beyond the original
  orientation write. Dolly body damping remains in the Body stage.

## Design

```text
Aim stage
  find SplineDolly in the Body stage
  read its current standardized spline position
  evaluate surrounding look-at keys at normalized position
  resolve each key to a world point
  interpolate with per-item cubic easing
  look from CameraState.RawPosition toward that point
  update RawOrientation and ReferenceLookAt
```

The interpolation uses the original control-point adjustment:

```text
t2 = t * t
d = 1 - t
eased = 3*d^2*t*lerp(0.3333, 0, a.easing)
      + 3*d*t2*lerp(0.6666, 1, b.easing)
      + t*t2
world = lerp(a.world, b.world, eased)
```

When the look direction is parallel to `ReferenceUp`, the fallback up vectors
come from the incoming camera orientation (`back`, then `left`), matching the
original.

## API Sketch

```cpp
class SplineDollyLookAtTargets : public CameraComponent
{
public:
    struct Item
    {
        EntityId lookAt = 0;
        math::Vec3 offset{};
        float easing = 1.0f;
    };

    struct Key
    {
        float position = 0.0f;
        Item item{};
    };

    std::vector<Key> targets;
    Item defaultItem{};

    void reset();
    void validate();

    Stage stage() const override;
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineSplineDollyLookAtTargets.cs`
  - item data, cubic easing interpolator, Aim stage, world-point resolution,
    look rotation, and reference look-at
- `com.unity.cinemachine/Tests/Runtime/SplineDollyLookatTarget.cs`
  - normalized interpolation between two world look-at points
- `com.unity.cinemachine/Runtime/Components/CinemachineSplineDolly.cs`
  - current spline position and body-stage integration

## Acceptance

- Defaults/reset use normalized positions, empty targets, and default item
  easing `1`.
- A keyed world target at normalized `0.5` makes the camera forward point at
  that target after a body update.
- Two keys at `0` and `0.5` interpolate at `0.25` to the world midpoint with
  easing `1`.
- Easing `0` changes the interpolation curve; easing values are clamped to
  `[0, 1]`.
- Entity targets resolve to entity position plus rotated offset; missing
  entities fall back to the offset as a world point.
- Positions outside the key range clamp to the first/last key.
- A look direction parallel to `ReferenceUp` produces a finite orientation
  using the orientation fallback up.
- `ReferenceLookAt` is always updated to the resolved world point.
- The component is invalid without an attached SplineDolly body and spline.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/spline_dolly_look_at_targets.hpp` and
  `src/components/spline_dolly_look_at_targets.cpp`
  - normalized look-at keys, per-item easing, and default item
  - entity target plus rotated offset resolution
  - world-offset fallback for missing entities
  - original cubic control-point easing interpolation
  - endpoint clamping and key sorting/clamping in `validate()`
  - SplineDolly body lookup at the Aim stage
  - look rotation, parallel-up fallbacks, and `ReferenceLookAt`
  - reset and validity checks
- `tests/unit/spline_dolly_look_at_targets_test.cpp`
  - 6 cases and 33 assertions covering defaults/reset, validation,
    interpolation, per-item easing, entity/world targets, endpoint clamping,
    up/zero direction fallbacks, and pipeline Aim order
- `examples/29_spline_look_at`
  - five-key look-at profile, moving dolly camera, entity and world targets,
    key/current markers, aim frustum and line, easing/pause controls, HUD, and
    smoke test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Keys are normalized positions only. Unity `SplineData<T>` path-index modes
  are not modeled.
- Entity offsets are rotated into world space but entity scale is ignored;
  missing entities turn the offset into a world point, matching the original
  null-target behavior.
- Associated dolly lookup uses the `Body` component on the same CineRig
  `VirtualCamera`, not a Unity GameObject lookup.
- `validate()` sorts and clamps keys as a convenience; the original runtime
  contract expects strictly increasing data.
- With no keys, the default item is evaluated directly. Its offset defaults to
  world origin, matching the original default item shape.
- Editor serialization, gizmos, and SaveDuringPlay remain out of scope.
  `SplineSmoother` is the separate P4-08 task.

### Verification

- `[spline-look-at]`: **6 cases / 33 assertions passed**.
- Core Debug and Release: **395/395 passed**
  (314 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **439/439 passed**
  (395 core + 44 example tests, including 29 smoke tests).
- `cinerig_29_spline_look_at --frames 180 --hidden --no-vsync` exits 0 in
  Debug and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes
  `cinerig/components/spline_dolly_look_at_targets.hpp`. An external
  `find_package(CineRig)` consumer compiled and linked the new component.
- Visible overview shows the blue spline, yellow key targets, green current
  look-at, orange target line, purple aim frustum, and HUD. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_spline_look_at.png`.
- Windows was verified; Linux was not tested on hardware.
