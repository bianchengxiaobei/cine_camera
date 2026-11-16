# P4-01 ConfinerOven

Status: completed  
Depends on: P0-02, P0-03  
Blocks: P4-02

## Goal

Port the behavior of `ConfinerOven`: bake a closed 2D polygon into an
aspect-aware containment shape and return a solution that can confine a camera
point for a requested half-frustum height.

## Scope

- Add `ConfinerOven` with:
  - closed input paths in world XY
  - aspect-ratio stretching around the polygon bounds center
  - polygon bounding box and theoretical maximum frustum height
  - zero-height and fixed-height containment solutions
  - erosion by a requested frustum height
  - midpoint collapse when the shape no longer contains the frustum
  - user maximum window size
  - baked-solution validity and path access
- Implement internal 2D polygon erosion:
  - closed-path cleanup and orientation normalization
  - even-odd nesting for outer paths and holes
  - miter joins with bevel fallback and self-intersection decomposition
  - orientation/inside-region filtering after decomposition
- Add unit tests for containment, edge clamping, aspect stretching, erosion,
  midpoint collapse, maximum window size, winding independence, and concave
  paths.
- Update build/export wiring and documentation.

## Non-goals

- Clipper2 or another external polygon library. The core remains buildable
  without Clipper2; this task uses the project's internal float polygon engine.
- Incremental/budgeted skeleton baking. `BakingState` is synchronous (`Baked`)
  once initialized; `BakeConfiner` is a compatibility no-op.
- Skeleton padding behavior beyond input validation. When erosion produces no
  area, the solution collapses to the polygon bounds midpoint.
- Exact Clipper integer rounding, overlapping outer paths, or pathological
  self-intersecting input. Simple closed outer paths and non-overlapping holes
  are the supported subset.

## Design

```text
initialize(paths, aspect, maxWindow, padding)
  clean closed paths
  compute bounds and aspect stretcher
  stretch paths into baked space
  normalize winding for even-odd nesting
  theoreticalMax = max(stretchedWidth, height) / 2
  state = Baked

getBakedSolution(height)
  clamp height to user maximum
  if height >= theoreticalMax: midpoint
  if height <= epsilon: stretched original paths
  else: erode each normalized path toward the allowed region
        decompose self-intersections
        keep correctly oriented loops inside the original region
  if empty: midpoint
```

`BakedSolution::confinePoint` returns the input unchanged when it is inside the
solution (even-odd, boundary included). Otherwise it returns the nearest point
on a solution edge, with the original far-away-edge distance penalty.

## API Sketch

```cpp
class ConfinerOven
{
public:
    enum class BakingState { Baking, Baked, Timeout };

    class BakedSolution
    {
    public:
        bool isValid() const noexcept;
        bool isEmpty() const noexcept;
        math::Vec2 confinePoint(math::Vec2 point) const;
    };

    ConfinerOven(
        const std::vector<std::vector<math::Vec2>>& paths,
        float aspectRatio,
        float maxFrustumHeight,
        float skeletonPadding = 0.0f);

    void initialize(...);
    bool isValid() const noexcept;
    BakingState state() const noexcept;
    float bakeProgress() const noexcept;
    void bakeConfiner(float maxComputationTimePerFrameInSeconds);
    BakedSolution getBakedSolution(float frustumHeight) const;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Core/ConfinerOven.cs`
  - aspect stretcher, theoretical maximum, baked solution nearest-edge behavior,
    distance penalty, midpoint fallback, orientation-independent containment.
- `com.unity.cinemachine/Runtime/ThirdParty/clipper.cs`
  - only as a behavioral reference for miter offset and even-odd filling.

## Acceptance

- Clockwise and counter-clockwise square inputs produce the same containment.
- A point inside the baked solution is unchanged; an outside point moves to the
  nearest edge.
- Erosion reduces the containment shape by the requested distance.
- A frustum larger than the shape collapses to the polygon midpoint.
- A positive maximum window size clamps the requested frustum height.
- Concave paths do not crash, produce finite solutions, and keep confinement
  inside the original region.
- Unit tests, Debug/Release builds, and install pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/confiner_oven.hpp` and
  `src/components/confiner_oven.cpp`
  - closed-path cleanup, finite-point filtering, and bounds
  - aspect stretching around the polygon bounds center
  - theoretical maximum frustum height
  - zero-height, eroded, midpoint, and maximum-window solutions
  - even-odd containment with boundary handling
  - nearest-edge confinement with the original far-edge distance penalty
  - winding normalization and nesting depth for outer paths and holes
  - miter offset with a 2x bevel fallback and self-intersection decomposition
  - orientation and inside-region filtering after decomposition
  - synchronous baked lifecycle and compatibility bake method
- `CMakeLists.txt` and `include/cinerig/cinerig.hpp` build/export wiring.
- `tests/unit/confiner_oven_test.cpp`
  - 10 cases and 53 assertions covering lifecycle, winding independence,
    aspect ratio, erosion, midpoint collapse, maximum windows, concave paths,
    holes, hole expansion, and collapsed hole regions.
- `examples/24_confiner2d` consumes the baked solutions through
  `Confiner2D`.

### Differences

- The internal float polygon engine replaces Clipper2. Core does not require
  Clipper2 or any external polygon dependency.
- The bake is synchronous. `BakingState::Baked` is immediate,
  `BakeConfiner` is a compatibility no-op, and there is no incremental
  skeleton or timeout path.
- Skeleton padding is validated but not used. Oversize solutions collapse to
  the polygon bounds midpoint.
- Simple outer loops and non-overlapping holes are supported. Overlapping
  outer paths, pathological self-intersections, and exact Clipper integer
  rounding are not.

### Verification

- `[confineroven]`: **10 cases / 53 assertions passed**.
- Core Debug and Release: **358/358 passed**.
- Examples Debug and Release: **397/397 passed** (358 core + 39 example
  tests).
- Install includes `cinerig/components/confiner_oven.hpp`.
- Windows was verified; Linux was not tested on hardware.



