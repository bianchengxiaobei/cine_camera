# P6-02 Confiner2D Fidelity

Status: completed
Depends on: P4-01, P4-02
Blocks: P6-03, precise 2D confinement for large windows and complex polygons

## Goal

Close the remaining behavioral gap between CineRig's synchronous
`ConfinerOven` and the reference `ConfinerOven`/`CinemachineConfiner2D` baking
pipeline. The result must use incremental skeleton baking, honor skeleton
padding, and make `OversizeWindow.Padding` observable instead of storing an
unused value.

The current synchronous solver is adequate for simple shapes. It is not
equivalent for shapes that produce multiple containment states, for large
frustums where the eroded polygon collapses to disconnected dots, or for
callers that expect `BoundingShapeIsBaked` to become true over several frames.

## Scope

- Extend `ConfinerOven` with:
  - reference `BakingState` behavior (`Baking`, `Baked`, `Timeout`);
  - a candidate solution cache built by a binary search over frustum height;
  - incremental `bakeConfiner(maxComputationTimePerFrameInSeconds)` with a
    five-second total bake timeout and `bakeProgress`;
  - `getBakedSolution` returning the current candidate while baking and the
    finalized solution after baking;
  - skeleton extraction from state-change transitions;
  - union of the eroded solution with the baked skeleton;
  - normalized skeleton padding (`max(0, padding) + 1`, matching the reference
    constructor);
  - minimum frustum height with bones and the reference `checkIntersectOriginal`
    nearest-edge behavior;
  - midpoint fallback only when the shape truly exceeds the theoretical maximum
    or no solution exists.
- Add internal polygon operations needed by skeleton construction:
  - offset/expand paths;
  - boolean difference for lost geometry;
  - boolean union for skeleton plus solution;
  - stable even-odd handling for outer paths and holes.
  These operations can remain private to the polygon engine. The core must not
  depend on Clipper2.
- Update `Confiner2D`:
  - construct the oven with `oversizeWindow.enabled ? maxWindowSize : -1` and
    `oversizeWindow.padding`;
  - call `bakeConfiner` with the reference per-frame budget while the shape is
    baking;
  - keep `boundingShapeIsBaked()` false until the oven reaches `Baked`;
  - make `bakeBoundingShape()` consume its time budget and stop at
    `Baked`/`Timeout`;
  - preserve shape-cache invalidation and transform-delta behavior;
  - expose bake progress/status if needed by the example.
- Update the `24_confiner2d` example to show bake status/progress and an
  oversize-window shape with visible padding.
- Update unit tests for the new lifecycle, skeleton behavior, padding,
  timeout/progress, and oversize windows.
- Update the task index, feature matrix, architecture, and example notes.

## Non-goals

- Exact Clipper2 integer rounding, bit-identical polygon vertices, or
  pathological self-intersections outside the supported subset.
- Unity `Collider2D`/`CompositeCollider2D` integration, inspector baking, or
  editor gizmos.
- Using `TargetPositionCache` during baking.
- Changing the Confiner2D slowing, damping, corner-threshold, or warp behavior
  except where the new baked solution requires it.

## Design

The reference pipeline is:

```text
initialize(paths, aspect, maxWindow, padding)
  stretch paths into baked space
  theoreticalMax = max(width / aspect, height) / 2
  if maxWindow < 0:
      state = Baked                  # no skeleton requested
      return
  build candidate at height 0
  state = Baking

bakeConfiner(maxTimePerFrame)
  while state == Baking:
      binary search the next polygon state change
      stop when the per-frame budget is exhausted
      set Timeout when the total bake time exceeds 5 seconds
  compute skeleton from the lost geometry at state transitions
  remove empty solutions
  state = Baked

getBakedSolution(height)
  clamp to user maximum window
  if height >= theoreticalMax: midpoint fallback
  offset the original shape by -height
  if baking or no skeleton: use the offset solution
  else: union(offset solution, skeleton)
```

The current float polygon engine must gain private boolean difference and union
helpers. The exact vertex order is not part of the public contract, but the
result must preserve outer paths, holes, containment, and nearest-edge
selection within the existing tolerance.

### Bake budget

The reference measures bake cost with wall-clock time. CineRig's simulation is
deterministic, but a CPU budget is not simulation time. This task may use
`std::chrono::steady_clock` only to decide when to pause the current bake. The
choice may affect how many frames the bake takes, but must not affect the final
baked geometry for the same inputs. Tests that need deterministic completion
should call `bakeConfiner` with a large budget or loop until `Baked`.

### Skeleton padding

The reference constructor passes `max(0, skeletonPadding) + 1` into the oven.
At each state-change pair, the larger solution is expanded by
`padding * (nextHeight - previousHeight)`, the smaller solution by twice that
amount, and the difference is added to the skeleton. Implement the same
normalization and order so padding has a visible effect on large-window
solutions.

### Oversize windows

`Confiner2D::OversizeWindow.Padding` must reach the oven and be stored in the
shape-cache settings. It must participate in cache validation so changing the
padding invalidates the oven and rebakes. `MaxWindowSize` continues to clamp
the requested frustum height. A zero `MaxWindowSize` means the reference
behavior: compute all possible windows, subject to the theoretical maximum.

## API Sketch

```cpp
class ConfinerOven
{
public:
    enum class BakingState { Baking, Baked, Timeout };

    void initialize(
        const std::vector<std::vector<math::Vec2>>& inputPaths,
        float aspectRatio,
        float maxFrustumHeight = -1.0f,
        float skeletonPadding = 0.0f);

    BakingState state() const noexcept;
    float bakeProgress() const noexcept;
    void bakeConfiner(float maxComputationTimePerFrameInSeconds);
    BakedSolution getBakedSolution(float frustumHeight) const;

private:
    struct PolygonSolution { /* paths, frustumHeight */ };
    struct BakingCache { /* candidates, skeleton, timings, progress */ };

    void computeSkeleton(const std::vector<PolygonSolution>& solutions);
    std::vector<std::vector<math::Vec2>> unionPaths(
        const std::vector<std::vector<math::Vec2>>& a,
        const std::vector<std::vector<math::Vec2>>& b) const;
    std::vector<std::vector<math::Vec2>> subtractPaths(
        const std::vector<std::vector<math::Vec2>>& a,
        const std::vector<std::vector<math::Vec2>>& b) const;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Core/ConfinerOven.cs`
  - `BakingState`, binary-search solution cache, `BakeConfiner`, skeleton
    extraction, padding normalization, timeout, progress, theoretical maximum,
    and `BakedSolution::ConfinePoint`.
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineConfiner2D.cs`
  - shape-cache validation, per-frame bake budget (`1/120`), oversize settings,
    `BoundingShapeIsBaked`, and lens cache invalidation.
- `CineRig/src/components/confiner_oven.cpp`
  - current synchronous erosion, aspect stretching, holes, and midpoint
    fallback.
- `CineRig/src/components/confiner2d.cpp`
  - current shape cache and per-camera baked solution.
- `CineRig/tests/unit/confiner_oven_test.cpp`
  - existing synchronous lifecycle tests that must be updated.
- `CineRig/tests/unit/confiner2d_test.cpp`
  - existing confinement, slowing, damping, transform-delta, and warp tests.

## Acceptance

- A shape with `maxFrustumHeight < 0` still initializes directly as `Baked`.
- A positive maximum window enters `Baking`; repeated `bakeConfiner` calls
  advance progress and eventually return `Baked`.
- A deliberately tiny budget can produce `Baking` or `Timeout` without
  corrupting the current solution.
- `BoundingShapeIsBaked()` is false while `Baking` and true after `Baked`.
- `getBakedSolution` remains usable while baking and after timeout.
- Skeleton padding changes the baked solution for a shape with multiple
  containment states; without padding the same large-window case degrades to
  disconnected points.
- `OversizeWindow.Padding` is passed to the oven, participates in cache
  invalidation, and is not silently ignored.
- Existing simple containment, hole, winding, slowing, damping, transform
  delta, invalidation, and warp tests still pass.
- Core, example, install, and no-tests checks pass on Windows.

## Implementation Record

### Implemented

- `ConfinerOven` now follows the reference incremental lifecycle:
  - a positive maximum window enters `Baking`; a negative maximum initializes
    directly as `Baked`;
  - the initial solution and theoretical midpoint candidate are cached;
  - binary search records topology-change pairs and advances
    `bakeProgress`;
  - `bakeConfiner` consumes a wall-clock budget and stops after five
    accumulated seconds with `Timeout`;
  - finalization extracts lost geometry, unions solution and skeleton, removes
    empty candidates, and marks the result `Baked`.
- Added private float polygon operations for miter offset, self-intersection
  decomposition, even-odd boolean difference, boolean union, and stable loop
  reconstruction. The core still has no Clipper2 dependency.
- Added reference skeleton behavior:
  - `max(0, padding) + 1` normalization;
  - expansion of the larger solution by `step` and the smaller by `2 * step`;
  - skeleton union for oversize solutions;
  - `minFrustumHeightWithBones` and nearest-edge selection with the
    `checkIntersectOriginal` guard.
- `Confiner2D` now:
  - advances baking with the reference `1/120` per-frame budget;
  - consumes the `bakeBoundingShape` time budget;
  - keeps `boundingShapeIsBaked()` false during `Baking` and `Timeout`;
  - invalidates per-camera lens solutions when baking completes outside the
    normal pipeline path;
  - exposes `bakeProgress()`, `bakingState()`, and
    `bakedSolutionPaths()` for diagnostics/examples.
- `examples/24_confiner2d` enables the oversize-window path with padding,
  reports bake state/progress in the HUD, and draws the baked solution
  (including skeleton geometry) in magenta.
- Tests cover incremental progress, usable solutions while baking, budget
  independence, skeleton padding, oversize bake completion, padding cache
  invalidation, and the existing containment/damping/warp behavior.

### Differences From The Reference

- The internal float polygon engine replaces Clipper2. Boolean vertices are not
  bit-identical and the implementation is intended for the supported closed
  path subset, not pathological self-intersections.
- Topology changes are detected by path and vertex counts, matching the
  reference heuristic; changes that preserve both counts do not create a new
  state.
- The bake budget uses `std::chrono::steady_clock` only to pause work. The
  final geometry for a fixed input is independent of the budget split.
- Oversize cache comparison uses the existing small float tolerance rather than
  exact float equality.

## Verification

- `ConfinerOven` filtered tests: **13 cases / 89 assertions passed**.
- `Confiner2D` filtered tests: **13 cases / 65 assertions passed**.
- Full Debug unit: **332 cases / 3029 assertions passed**.
- Full Debug scenario: **141 cases / 1154 assertions passed**.
- Core Debug CTest: **503/503 passed**, including 6/6 performance cases.
- Core Release CTest: **503/503 passed**, including 6/6 performance cases.
- Examples Debug CTest: **556/556 passed**.
- Examples Release CTest: **556/556 passed**.
- AddressSanitizer core CTest: **497/497 passed**, performance excluded.
- Installed `find_package(CineRig)` P6-02 consumer compiled, linked, and
  exited 0.
- `CINERIG_BUILD_TESTS=OFF` Release build passed.
- The `24_confiner2d` headless smoke test passed in Debug and Release.
