# P4-03 Spline Abstractions

Status: completed  
Depends on: P0-02, P0-05  
Blocks: P4-04 SplineDolly and the remaining spline features

## Goal

Port the reusable spline layer used by Cinemachine's spline components:
`PathIndexUnit`, `SplineSettings`, Bezier evaluation helpers, and a concrete
engine-independent cubic Bezier spline implementing the shared `ISpline`
sampling contract.

The task establishes exact normalized/distance/knot position conversion,
arc-length evaluation, closest-point queries, tangent/up frames, and smooth
control-point generation. The later SplineDolly, SplineCart, SplineRoll, and
SplineDollyLookAtTargets tasks consume this layer.

## Scope

- Move the shared `ISpline` sampling interface into `core` so
  `core::SplineSettings` can depend on it without violating the layer rules.
  `engine/spline.hpp` keeps exposing `ISpline` and owns `ISplineProvider`.
- Extend `ISpline` with:
  - `knotCount()` and `closed()` metadata with conservative defaults
  - `convertPosition()` for `Distance`, `Normalized`, and `Knot` units
- Add `core::PathIndexUnit` and `core::SplineSettings`:
  - non-owning spline pointer, position, and units
  - unit change while preserving the point on the spline
  - max-position and standardization helpers (open clamping, closed wrapping)
  - validation and normalized-position conversion
- Add `core::BezierSpline`:
  - cubic Bezier knots with incoming/outgoing control tangents and rotation
  - open and closed splines
  - cached curve lengths and per-curve arc-length lookup tables
  - position/tangent/up/rotation evaluation
  - global arc-length normalized parameterization
  - closest-point query with local refinement
  - exact distance/normalized/knot conversion
  - linear and smooth-control-point factories
- Add `core::SplineHelpers` ports of `bezier3`, `bezierTangent3`,
  tangent-weight calculation, and the Thomas-algorithm smooth control points
  for open and looped knots.
- Add unit tests for defaults, arc-length parameters, unit conversion, open and
  closed wrapping, tangents/up, closest points, and smooth control points.
- Add a `25_spline` SDL example that visualizes a smooth closed spline, knots,
  moving point, tangent/up axes, unit switching, HUD, and a smoke test.
- Update the task index, feature matrix, architecture, and example index.

## Non-goals

- `SplineDolly`, `SplineCart`, `SplineRoll`, `SplineDollyLookAtTargets`,
  `SplineSmoother`, or automatic dolly algorithms. Those are separate tasks.
- Unity `SplineContainer`, `NativeSpline`, `Unity.Mathematics`, `Transform`
  scale baking, or editor frame caching.
- Exact Unity Spline up-vector/twist behavior for arbitrary 3D knot rotations.
  `BezierSpline` interpolates knot rotations and orthonormalizes the resulting
  up vector against the tangent.
- Engine-specific asset lookup. Callers own the `ISpline` lifetime and pass a
  stable pointer to `SplineSettings`.
- Editor serialization, gizmos, and SaveDuringPlay.

## Design

```text
core::ISpline
  length()
  knotCount() / closed()
  evaluatePosition/Tangent/Up(normalized)
  closestPoint(world) -> normalized
  convertPosition(value, from, to)

core::SplineSettings
  const ISpline* spline
  float position
  PathIndexUnit units
  changeUnitPreservePosition()

core::BezierSpline : ISpline
  knots + closed
  curves[]: p0, p1, p2, p3, length, arc samples
  normalized parameter -> global distance -> curve + local parameter
  knot parameter -> segment + local arc distance -> normalized
```

`Normalized` is a global arc-length fraction over the entire spline.
`Distance` is world distance from the start. `Knot` is the knot index plus a
fraction in the segment's Bezier parameter, matching Unity Splines semantics.
For closed splines the maximum knot value is `knotCount`; for open splines it
is `knotCount - 1`.

The generic `ISpline::convertPosition()` default maps knot values linearly
across the number of curves. Concrete splines with segment arc lengths can
override it; `BezierSpline` does so for exact conversion.

## API Sketch

```cpp
enum class PathIndexUnit { Distance, Normalized, Knot };

class ISpline
{
public:
    virtual float length() const = 0;
    virtual math::Vec3 evaluatePosition(float t) const = 0;
    virtual math::Vec3 evaluateTangent(float t) const = 0;
    virtual math::Vec3 evaluateUp(float t) const = 0;
    virtual float closestPoint(math::Vec3 worldPosition) const = 0;

    virtual std::size_t knotCount() const;
    virtual bool closed() const;
    virtual float convertPosition(
        float position, PathIndexUnit from, PathIndexUnit to) const;
};

struct SplineSettings
{
    const ISpline* spline = nullptr;
    float position = 0.0f;
    PathIndexUnit units = PathIndexUnit::Distance;

    bool isValid() const noexcept;
    float maxPosition() const;
    float standardizedPosition(float* maxPosition = nullptr) const;
    float normalizedPosition() const;
    void changeUnitPreservePosition(PathIndexUnit newUnits);
};

struct BezierKnot
{
    math::Vec3 position{};
    math::Vec3 tangentIn{};
    math::Vec3 tangentOut{};
    math::Quat rotation = math::Quat::identity();
};

class BezierSpline final : public ISpline
{
public:
    static BezierSpline linear(
        const std::vector<math::Vec3>& points, bool closed = false);
    static BezierSpline smooth(
        const std::vector<math::Vec3>& points, bool closed = false);

    void setKnots(std::vector<BezierKnot> knots, bool closed);
    const std::vector<BezierKnot>& knots() const noexcept;

    float length() const override;
    math::Vec3 evaluatePosition(float t) const override;
    math::Vec3 evaluateTangent(float t) const override;
    math::Vec3 evaluateUp(float t) const override;
    math::Quat evaluateRotation(float t) const;
    float closestPoint(math::Vec3 worldPosition) const override;
    float convertPosition(
        float position, PathIndexUnit from, PathIndexUnit to) const override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Core/SplineSettings.cs`
  - `PathIndexUnit` position semantics, unit-preserving changes, cache behavior
- `com.unity.cinemachine/Runtime/Core/SplineContainerExtensions.cs`
  - max position, closed wrapping, standardization, up/roll frame
- `com.unity.cinemachine/Runtime/Core/SplineHelpers.cs`
  - cubic Bezier evaluation, tangent weights, smooth control points
- `com.unity.cinemachine/Tests/Runtime/SplineDollyCameraTest.cs`
  - normalized/distance/knot behavior on a closed 4-knot square
- `com.unity.cinemachine/Tests/Runtime/SplineCartTests.cs`

## Acceptance

- `SplineSettings` defaults to distance units, position `0`, and no spline.
- Changing units preserves the evaluated position.
- A closed square with side lengths `6, 7, 6, 7` has length `26`.
- Normalized `0.125` evaluates to `3.25` world units from the start; knot
  `0.125` evaluates to `0.75` world units from the first knot.
- Distance `13`, normalized `0.5`, and knot `2` identify the same point.
- Closed distance/knot positions wrap; open positions clamp; NaN becomes `0`.
- Tangents are unit length where the derivative is non-degenerate; up vectors
  are orthonormal to tangents.
- Closest-point queries return normalized arc-length positions.
- Smooth control points produce finite controls for open and looped inputs.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/core/spline.hpp`
  - `PathIndexUnit` (`Distance`, `Normalized`, `Knot`)
  - shared `ISpline` sampling contract with `knotCount()`, `closed()`, and
    `convertPosition()`
  - generic distance/normalized conversion and linear knot-unit fallback
- `include/cinerig/engine/spline.hpp`
  - keeps `ISplineProvider`; includes the core contract so existing engine
    consumers keep the same public include path
- `include/cinerig/core/spline_settings.hpp` and `src/core/spline.cpp`
  - non-owning spline reference, position, and units
  - max-position queries, open clamping, closed wrapping, NaN normalization
  - unit-preserving changes and normalized-position conversion
- `include/cinerig/core/spline_helpers.hpp` and `src/core/spline_helpers.cpp`
  - cubic Bezier position/tangent evaluation and tangent weights
  - scalar Bezier helpers
  - Thomas-algorithm smooth control points for open and looped knots
- `include/cinerig/core/bezier_spline.hpp` and `src/core/bezier_spline.cpp`
  - cubic Bezier knots with incoming/outgoing control offsets and rotation
  - open/closed spline rebuilds
  - 32-sample per-curve arc-length tables and global normalized parameters
  - exact distance/normalized/knot conversion for non-uniform segments
  - position, normalized tangent, orthonormal up, and full rotation evaluation
  - closest-point query with dense per-curve sampling and ternary refinement
  - `linear()` and second-order `smooth()` factories
- `tests/unit/spline_test.cpp`
  - 8 cases and 169 assertions covering defaults, helpers, smooth controls,
    closed-square length/conversion, settings changes, wrapping, tangent/up,
    closest points, and rebuilds.
- `examples/25_spline`
  - smooth closed seven-knot spline, control polygon, knot markers, moving
    marker, tangent/up/right frame, unit switching, HUD, and smoke test.
- Build/export wiring, task index, feature matrix, architecture, coordinate
  convention, and examples README updated.

### Differences

- The `ISpline` declaration moved to `core/spline.hpp` to preserve the
  documented layer rule that `core` depends only on `math`.
  `engine/spline.hpp` remains the engine/provider include path and exposes the
  same flat `cinerig::ISpline` type.
- `SplineSettings` is a non-owning pointer and needs no per-frame spline cache.
  The concrete `BezierSpline` owns and rebuilds its own arc-length tables;
  path edits require calling `setKnots()` again.
- `ISpline` is world-space. Unity `SplineContainer` transform/scale baking and
  the `CachedScaledSpline` lifecycle are adapter concerns and are not modeled.
- The generic `ISpline::convertPosition()` treats knot units as a linear
  interpolation across the curve count. `BezierSpline` overrides it with exact
  segment lengths; other adapters should do the same for non-uniform curves.
- Bezier up vectors interpolate knot rotations and orthonormalize against the
  tangent. Unity NativeSpline twist behavior for arbitrary 3D knot rotations is
  not reproduced exactly.
- Arc lengths use fixed internal sampling. The original one-argument
  closest-point query also uses fixed sampling; P4-04 adds a separate
  resolution/iteration overload for automatic dolly.
- SplineDolly, automatic dolly, cart, roll, look-at targets, and spline
  smoothing components remain in later tasks. Editor serialization, gizmos,
  and SaveDuringPlay are not ported.

### Verification

- `[spline]`: **8 cases / 169 assertions passed**.
- Core Debug and Release: **366/366 passed**
  (285 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **406/406 passed**
  (366 core + 40 example tests, including 25 smoke tests).
- `cinerig_25_spline --frames 180 --hidden --no-vsync` exits 0 in Debug and
  Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/core/spline.hpp`, `spline_settings.hpp`,
  `spline_helpers.hpp`, `bezier_spline.hpp`, and `engine/spline.hpp`. An
  external `find_package(CineRig)` consumer built and ran a `BezierSpline` +
  `SplineSettings` check successfully.
- Visible example shows the smooth closed spline, control polygon, knot
  markers, moving marker, and tangent/up/right axes. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_spline.png`.
- Windows was verified; Linux was not tested on hardware.
