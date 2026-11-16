# P4-02 Confiner2D

Status: completed  
Depends on: P1-02, P3-08, P4-01  
Blocks: P4 completion, 2D camera workflows

## Goal

Port `CinemachineConfiner2D`. The extension constrains the camera position so
that the viewport at the bounding-shape plane stays inside a closed 2D polygon.
It supports orthographic and perspective lenses, aspect-aware baking, edge
slowing, corner damping, displacement reporting, and cache invalidation.

## Scope

- Add `components::Confiner2D` deriving from `CameraExtension`:
  - closed polygon paths with a shape offset and shape-to-world transform
  - aspect-aware `ConfinerOven` shape cache
  - world-to-baked/current delta for transform changes after baking
  - half-frustum height from orthographic size or perspective FOV/distance
  - closest-point confinement at the `Body` stage
  - forward-axis preservation
  - slowing distance with the original `t^3 + 0.05` easing
  - corner damping with the 10 degree threshold
  - per-camera baked solution, frustum height, previous displacement,
    damped displacement, and previous camera position
  - displacement queries, lens-cache invalidation, shape-cache invalidation,
    bake request/status, and followed-target warp adjustment
  - reset and validation
- Add unit tests for defaults, containment, viewport shrinking, slowing,
  damping, invalidation, transform deltas, missing shapes, and warp handling.
- Add a `24_confiner2d` SDL example with a visible 2D room, screen-edge
  rectangle, selectable shapes, damping/slowing controls, HUD, and smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity `Collider2D` types and inspector integration. The host supplies paths
  and a transform directly.
- Incremental/budgeted skeleton baking and padding. `ConfinerOven` is
  synchronous and collapses oversize solutions to the shape midpoint.
- Editor gizmos, serialization, SaveDuringPlay, layer masks, or Unity tags.
- Exact Clipper2 integer results. Polygon erosion is the internal float
  approximation delivered by P4-01.
- Automatic cache rebuilding when path points change. Call
  `invalidateBoundingShapeCache()` as in the original API.

## Design

```text
Body stage
  validate shape cache for current aspect/oversize settings
  transform corrected camera position into baked shape space
  frustumHeight = halfFrustumHeight(lens, localZ) * deltaScale
  get/reuse baked solution for frustumHeight
  confine point in baked XY, preserve camera forward component
  apply slowing from cached previous position
  compute desired displacement and corner damping
  store previous camera position/displacement
  apply position correction
```

The shape cache bakes world-space paths at the current shape transform.
Translation, rotation, and uniform scale changes are handled by a delta matrix
without rebaking. Non-uniform scale or aspect changes rebuild the oven. The
extension stores per-`CameraId` state.

## API Sketch

```cpp
class Confiner2D : public CameraExtension
{
public:
    struct OversizeWindowSettings
    {
        bool enabled = false;
        float maxWindowSize = 0.0f;
        float padding = 0.0f;
    };

    std::vector<std::vector<math::Vec2>> boundingShape;
    math::Vec2 boundingShapeOffset{};
    math::Mat4 boundingShapeToWorld = math::Mat4::identity();
    float damping = 0.0f;
    float slowingDistance = 0.0f;
    OversizeWindowSettings oversizeWindow{};

    void reset();
    void validate();
    bool isValid() const noexcept;
    bool boundingShapeIsBaked() const;
    bool bakeBoundingShape(VirtualCamera& vcam, float maxTimeInSeconds);
    void invalidateBoundingShapeCache();
    void invalidateLensCache();
    bool cameraWasDisplaced(CameraId cameraId) const;
    float cameraDisplacementDistance(CameraId cameraId) const;
    static float calculateHalfFrustumHeight(
        const LensSettings& lens,
        float cameraPositionLocalZ);

    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
    float maxDampTime() const override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineConfiner2D.cs`
  - shape cache, delta transforms, frustum calculation, slowing, damping,
    displacement state, invalidation, baking status, and defaults.
- `com.unity.cinemachine/Runtime/Core/ConfinerOven.cs`
  - baked solutions and nearest-edge confinement.
- `com.unity.cinemachine/Tests/Runtime/Confiner2DUnitTests.cs`
  - winding independence and simple square containment.

## Acceptance

- Defaults/reset match the original values: damping `0.5`, slowing `5`, and
  empty oversize settings.
- A point inside the baked shape is unchanged; an outside point moves to the
  nearest allowed edge.
- The frozen lens/aspect cache shrinks the shape by the half-frustum height.
- Slowing uses `t^3 + 0.05`; no previous state snaps directly.
- Corner damping is bypassed for invalid previous state, negative delta time,
  or non-positive damping.
- Shape transform translation/uniform scale uses the delta; aspect or
  non-uniform changes rebake.
- Missing or empty shape produces no correction.
- Target warps adjust cached previous camera positions.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/confiner2d.hpp` and
  `src/components/confiner2d.cpp`
  - local polygon paths with offset and shape-to-world transform
  - synchronous `ConfinerOven` shape cache
  - world-to-baked delta with uniform scale/rotation/translation reuse
  - non-uniform delta and aspect-change rebaking
  - lens cache invalidation and shape cache invalidation
  - orthographic/perspective half-frustum calculation
  - `Body` stage closest-point confinement and forward-axis preservation
  - slowing with `t^3 + 0.05`, snapped first frame, and per-camera state
  - corner damping at the original 10 degree threshold
  - raw/damped displacement state and displacement queries
  - followed-target warp adjustment
  - reset, validation, bake status, and defaults `0.5` / `5`
- `CMakeLists.txt` and `include/cinerig/cinerig.hpp` build/export wiring.
- `tests/unit/confiner2d_test.cpp`
  - 11 cases and 52 assertions covering defaults, frustum math, winding
    independence, viewport shrinking, slowing, first-frame snapping, corner
    damping, transform deltas, lens/shape invalidation, missing shapes, and
    warp handling.
- `examples/24_confiner2d`
  - L-room/box/none shapes, overview camera, raw and confined screen
    footprints, target path, damping/slowing controls, HUD, and smoke test.
- Task index, feature matrix, architecture, and examples README updated.

### Differences

- The host supplies paths, offset, and shape-to-world transform directly;
  Unity `Collider2D` types and hierarchy checks are not modeled.
- Path point edits require `invalidateBoundingShapeCache()` as in the original
  API. Translation, rotation, and uniform scale changes use the delta cache.
- `ConfinerOven` is synchronous and has no skeleton/padding behavior; oversize
  solutions collapse to the shape midpoint.
- The warp callback adjusts cached positions for all evaluating cameras whose
  owner follows the warped entity, because the CineRig extension callback does
  not identify the source virtual camera.
- Editor gizmos, serialization, SaveDuringPlay, and Unity tags are not ported.
- Exact Clipper2 results and pathological polygon inputs are not guaranteed.

### Verification

- `[confiner2d]`: **11 cases / 52 assertions passed**.
- Core Debug and Release: **358/358 passed** (277 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **397/397 passed** (358 core + 39 example tests,
  including 24 smoke tests).
- `cinerig_24_confiner2d --frames 180 --hidden --no-vsync` exits 0; the
  no-tests Debug build exits 0 at 2 frames.
- Visible overview shows the raw footprint outside the L-room and the green
  confined footprint inside it at the same target position. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_confiner2d.png`.
- Install includes `cinerig/components/confiner2d.hpp` and
  `cinerig/components/confiner_oven.hpp`.
- Windows was verified; Linux was not tested on hardware.



