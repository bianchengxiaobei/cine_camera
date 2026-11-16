# P4-08 SplineSmoother

Status: completed  
Depends on: P4-03  
Blocks: P4-09 MixingCamera

## Goal

Port `CinemachineSplineSmoother`. The helper recomputes the knot tangents and
orientations of a `BezierSpline` so that the path has second-order smoothness
and is suitable as a camera path.

## Scope

- Add `components::SplineSmoother`:
  - non-owning `BezierSpline*`
  - `autoSmooth` compatibility setting
  - `reset()` and `isValid()`
  - `smoothNow()` for explicit smoothing
  - `onSplineModified()` for host-driven edit notifications
- Use the P4-03 Thomas-algorithm control-point solver for open and closed
  splines.
- Preserve knot positions and preferred up directions while replacing each
  knot rotation with a forward-aligned rotation.
- Mirror incoming/outgoing tangents where both sides exist, zero the open
  start `tangentIn` and open end `tangentOut`, and use finite fallbacks for
  degenerate tangent/up inputs.
- Add unit tests for defaults/reset, too-few-knots no-op, open and closed
  smoothing, tangent/rotation results, `autoSmooth`, idempotence, and
  degenerate knots.
- Add a `30_spline_smoother` SDL example comparing the raw linear path with the
  smoothed path, tangent handles, knot frames, a moving marker, controls, HUD,
  and a smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity `SplineContainer`, `MonoBehaviour`, `[ExecuteAlways]`, component
  enable/disable state, inspectors, or editor callbacks. `onSplineModified()`
  is an explicit host hook.
- Unity `TangentMode` bookkeeping. CineRig writes the incoming and outgoing
  control vectors directly.
- Smoothing arbitrary `ISpline` implementations. Control-point smoothing
  requires a mutable `BezierSpline`.
- Rebuilding external native spline caches or serializing knot changes.
- Automatic smoothing of every edit without the caller invoking
  `onSplineModified()`.
- Editor serialization, undo, gizmos, and SaveDuringPlay.

## Design

```text
components::SplineSmoother
  BezierSpline* spline
  bool autoSmooth

  smoothNow()
    reject null spline or fewer than 3 knots
    copy knot positions
    solve open or looped smooth control points
    for each knot:
      keep position
      orient forward along outgoing (or incoming at the open end)
      preserve the old up projection
      write control-point tangent offsets
      zero the missing open-end tangent
    commit all knots to the spline

  onSplineModified()
    return autoSmooth && smoothNow()
```

Smoothing is idempotent: positions are kept, control points depend only on
positions, and the replacement rotations are computed from the same path
directions.

Reference control points use:

```text
open:   computeSmoothControlPoints(knots, ctrl1, ctrl2)
closed: computeSmoothControlPointsLooped(knots, ctrl1, ctrl2)

forward = ctrl1[i] - position[i]
back    = ctrl2[i - 1] - position[i]
length  = (|back| + |forward|) * 0.5
tangentOut[i] = normalize(forward) * length
tangentIn[i]  = -normalize(forward) * length
```

For an open spline the first `tangentIn` and last `tangentOut` are zero, and
the endpoint length uses only the available side. The last knot orientation
follows `position[last] - ctrl2[last - 1]` instead of the original
implementation's unused end control point, which is zero in the solver output.
The mirrored construction also removes the small closed-seam mismatch that
exists when the two looped-solver boundary copies are used directly.

## API Sketch

```cpp
class SplineSmoother
{
public:
    BezierSpline* spline = nullptr;
    bool autoSmooth = true;

    void reset();
    bool isValid() const noexcept;
    bool smoothNow();
    bool onSplineModified();
};
```

## Reference

- `com.unity.cinemachine/Runtime/Helpers/CinemachineSplineSmoother.cs`
  - manual smoothing entry point, minimum knot count, open/closed selection,
    preservation of knot up direction, forward rotation, mirrored tangent
    lengths, and open endpoint tangent zeros
- `com.unity.cinemachine/Runtime/Core/SplineHelpers.cs`
  - `ComputeSmoothControlPoints` and `ComputeSmoothControlPointsLooped`
- `CineRig/include/cinerig/core/bezier_spline.hpp`
  - mutable knot list, world-space tangent offsets, and rotation contract

## Acceptance

- Defaults are a null spline and `autoSmooth = true`; reset restores them.
- A null or fewer-than-three-knot spline is unchanged and `smoothNow()` returns
  `false`.
- Smoothing an open spline preserves positions, zeros the endpoint tangents,
  mirrors interior tangents, and orients knots along the path.
- Smoothing a closed spline preserves positions, mirrors every knot tangent,
  produces a path close to `BezierSpline::smooth` at sampled positions, and has
  a continuous tangent across the loop seam.
- Preferred up directions are projected onto the new tangent frame; parallel
  or zero up inputs still produce finite rotations.
- Coincident knots produce finite rotations and control vectors.
- `onSplineModified()` respects `autoSmooth`; direct `smoothNow()` ignores it.
- Repeated smoothing is idempotent.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/spline_smoother.hpp` and
  `src/components/spline_smoother.cpp`
  - non-owning BezierSpline reference and `autoSmooth` compatibility setting
  - open/looped smooth control-point solving
  - mirrored tangent offsets built from the average available side lengths
  - open endpoint tangent zeros and robust endpoint orientation
  - preferred-up projection and forward-aligned knot rotations
  - finite fallbacks for degenerate tangents and up vectors
  - `reset()`, `smoothNow()`, and host-driven `onSplineModified()`
- `tests/unit/spline_smoother_test.cpp`
  - 5 cases and 208 assertions covering defaults/reset, short-spline no-op,
    open and closed paths, tangent mirroring, endpoint orientation, preferred
    up, autoSmooth, idempotence, coincident knots, and parallel up inputs
- `examples/30_spline_smoother`
  - raw linear versus smoothed path, tangent handles, knot up vectors, moving
    marker, smoothing/pause/reset controls, HUD, and smoke test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- The Unity MonoBehaviour, `[ExecuteAlways]`, enable state, and editor
  modification callbacks are not ported. `onSplineModified()` is an explicit
  host hook; direct `smoothNow()` ignores `autoSmooth`.
- Only mutable `BezierSpline` instances are supported. CineRig writes incoming
  and outgoing tangent vectors directly instead of tracking Unity
  `TangentMode`.
- Open endpoint lengths use only the available side. The original reads
  solver arrays whose unused end entries are zero, which can produce a
  direction and length unrelated to the actual endpoint segment.
- Closed-seam tangents use the averaged available side lengths. This avoids
  the small mismatch between the two looped-solver boundary copies and keeps
  the loop tangent continuous.
- No editor serialization, undo, gizmos, or external native spline cache
  invalidation is performed.

### Verification

- `[smoother]`: **5 cases / 208 assertions passed**.
- Core Debug and Release: **400/400 passed**
  (319 unit + 75 scenario + 6 golden).
- Examples Debug and Release: **445/445 passed**
  (400 core + 45 example tests, including 30 smoke tests).
- `cinerig_30_spline_smoother --frames 180 --hidden --no-vsync` exits 0 in
  Debug and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/components/spline_smoother.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, and smoothed a three-knot
  spline.
- Visible overview shows the raw orange path, smoothed cyan path, yellow
  knots, orange/blue tangent handles, green up vectors, moving marker, and HUD.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_spline_smoother.png`.
- Windows was verified; Linux was not tested on hardware.
