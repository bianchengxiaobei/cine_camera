# P4-05 SplineCart

Status: completed  
Depends on: P0-05, P4-03, P4-04  
Blocks: P4-06 SplineRoll and later spline helpers

## Goal

Port `CinemachineSplineCart`. The helper moves an entity transform along a
spline, either from a manually specified spline position or from an automatic
dolly method, and can serve as a tracking target for virtual cameras.

## Scope

- Add `components::SplineCart`, a non-camera helper working with
  `IEntityWorld`:
  - non-owning `SplineSettings` with normalized default units
  - `Update`, `FixedUpdate`, and `LateUpdate` dispatch
  - manual spline position and unit-preserving evaluation
  - `SplineAutoDolly` with fixed-speed and nearest-point methods
  - an entity tracking target for methods that require one
  - position/tangent/up frame evaluation and transform write-back
  - preserve the existing entity scale
  - reset, validation, and validity reporting
- Add unit tests for defaults/reset, manual positions and units, transform
  rotation/scale preservation, update-method gating, fixed-speed and
  nearest-point auto-dolly, missing targets, and read-only worlds.
- Add a `27_spline_cart` SDL example with a visible moving cart, target,
  automatic/manual modes, update-method and unit controls, HUD, and smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- `SplineRoll`, `SplineDollyLookAtTargets`, and `SplineSmoother`. Those are
  separate tasks, completed in P4-06 through P4-08.
- Unity `MonoBehaviour` lifecycle, `ExecuteAlways`, editor-time updates, and
  `Transform.ConservativeSetPositionAndRotation`.
- Unity `SplineContainer` transform/scale baking, `NativeSpline`, and cache
  invalidation.
- Spline roll data itself. P4-06 adds an optional `SplineRoll` pointer; when
  null, rotation uses the spline tangent/up frame.
- Passing the cart object to `ISplineAutoDolly`. The CineRig method signature is
  context-free; custom methods may capture context.
- Serialization of `SerializeReference` methods and legacy upgrade fields.

## Design

```text
caller selects update / fixedUpdate / lateUpdate
  if the configured UpdateMethod does not match: return false
  if automatic dolly is enabled:
    resolve TrackingTarget in IEntityWorld (optional)
    ask the auto-dolly method for the new spline position
  standardize the position for the selected units
  convert to normalized, evaluate position/tangent/up
  preserve the entity scale and write position/rotation
```

`SplineCart` is not a camera component. It is a helper owned by the caller and
updated explicitly, which mirrors CineRig's explicit clock model. The `Update`
method is intended for the host's normal update step; `FixedUpdate` and
`LateUpdate` are available for the two CineRig clocks.

## API Sketch

```cpp
class SplineCart
{
public:
    enum class UpdateMethod
    {
        Update = 0,
        FixedUpdate,
        LateUpdate,
    };

    SplineSettings spline{nullptr, 0.0f, PathIndexUnit::Normalized};
    UpdateMethod updateMethod = UpdateMethod::Update;
    SplineAutoDolly automaticDolly{};
    EntityId trackingTarget = 0;

    void reset();
    void validate();
    bool isValid() const noexcept;

    bool update(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);
    bool fixedUpdate(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);
    bool lateUpdate(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);

    bool setCartPosition(
        IEntityWorld& world,
        EntityId cartEntity,
        float position);
};
```

Each update method returns true only when it is selected and the entity
transform was written. `setCartPosition` returns the `trySetTransform` result.

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineSplineCart.cs`
  - spline settings/defaults, update methods, automatic dolly, tracking target,
    position standardization, evaluation, and transform write
- `com.unity.cinemachine/Runtime/Core/SplineAutoDolly.cs`
  - shared automatic-dolly methods and settings
- `com.unity.cinemachine/Tests/Runtime/SplineCartTests.cs`
  - closed square distance/normalized position checks

## Acceptance

- Defaults match the original cart: normalized units, position `0`,
  `Update`, disabled auto-dolly, and no tracking target.
- On the original closed square, distance `0` and normalized `1` both place the
  cart at `(7, 1, -6)`.
- Manual updates preserve the entity scale and set the spline tangent/up
  rotation.
- `update`, `fixedUpdate`, and `lateUpdate` only run when their method matches.
- Fixed-speed auto-dolly advances by `speed * deltaTime` in the selected units.
- Nearest-point auto-dolly uses the tracking target and position offset; a
  missing target leaves the position unchanged.
- Read-only worlds return false and are not modified.
- Validation delegates to the automatic-dolly method.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/spline_cart.hpp` and
  `src/components/spline_cart.cpp`
  - non-owning normalized `SplineSettings`
  - `Update`, `FixedUpdate`, and `LateUpdate` gated dispatch
  - manual standardization/evaluation and entity transform write-back
  - preserved entity scale
  - automatic dolly before position evaluation
  - entity tracking target resolution through `IEntityWorld`
  - reset, validation, and `trySetTransform` failure reporting
- `tests/unit/spline_cart_test.cpp`
  - 6 cases and 56 assertions covering defaults/reset, validation, original
    closed-square positions, scale/rotation preservation, update gating,
    fixed-speed and nearest-target auto-dolly, missing targets/entities, and
    read-only worlds
- `examples/27_spline_cart`
  - smooth closed track, moving interior target, automatic/manual cart motion,
    three update methods, unit switching, cart axes, overview camera, HUD, and
    smoke test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- The helper writes an `EntityId` transform through `IEntityWorld` instead of a
  Unity `Transform`; it preserves the existing scale and returns false when the
  world is read-only.
- There is no `MonoBehaviour` lifecycle or `ExecuteAlways`. Callers explicitly
  invoke `update`, `fixedUpdate`, or `lateUpdate`; `Update` represents the
  host's normal update step because CineRig only exposes Fixed and Late clocks.
- Automatic dolly uses the caller-supplied delta time instead of reading Unity's
  `Time.deltaTime`.
- P4-05 itself used the spline tangent/up frame. P4-06 adds an optional
  `SplineRoll` pointer that is applied to the written cart rotation.
- Tracking targets are entity handles. Camera and target-group handles are not
  resolved by this standalone helper.
- `ISplineAutoDolly` does not receive the cart as a sender. Custom methods may
  capture their own context.
- `reset()` also clears the automatic-dolly enabled flag. The original editor
  `Reset()` only cleared the method and tracking target.
- Editor serialization, legacy upgrade fields, and gizmos are not ported.

### Verification

- `[spline-cart]`: **6 cases / 56 assertions passed**.
- Core Debug and Release: **382/382 passed**
  (301 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **424/424 passed**
  (382 core + 42 example tests, including 27 smoke tests).
- `cinerig_27_spline_cart --frames 180 --hidden --no-vsync` exits 0 in Debug
  and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/components/spline_cart.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, and ran `SplineCart` reset
  and validation successfully.
- Visible overview shows the blue closed spline, green target, oriented orange
  cart, update-method/unit controls, and HUD. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_spline_cart.png`.
- Windows was verified; Linux was not tested on hardware.
