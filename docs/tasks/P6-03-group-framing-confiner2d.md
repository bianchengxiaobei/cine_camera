# P6-03 GroupFraming + Confiner2D Compatibility

Status: completed
Depends on: P3-02, P4-02, P6-02
Blocks: correct simultaneous group framing and 2D confinement

## Goal

Port the reference `CinemachineGroupFraming` compatibility path for
`CinemachineConfiner2D`:

- when a Confiner2D extension is attached, apply group framing in the Body
  stage;
- invalidate the Confiner2D lens cache when the framed orthographic size
  changes;
- preserve the existing after-Aim body and double-Body-callback behavior when
  no Confiner2D is present.

This task is the integration layer only. The incremental ConfinerOven and
skeleton work belongs to P6-02.

## Scope

- Add a small camera-layer interface for lens-cache invalidation, for example:

```cpp
class ICameraLensCacheInvalidator
{
public:
    virtual ~ICameraLensCacheInvalidator() = default;
    virtual void invalidateLensCache() = 0;
};
```

  The interface must live in `camera` so `targets::GroupFraming` can detect the
  extension without depending on the concrete `components::Confiner2D` type.
- Make `components::Confiner2D` implement the interface.
- Extend `targets::GroupFraming` per-camera cache with:
  - a non-owning `ICameraLensCacheInvalidator*`;
  - the previous orthographic size;
  - the selected framing stage.
- On the first valid/previous-state reset:
  - find an enabled attached extension implementing the invalidator interface;
  - if found, select `Stage::Body`;
  - otherwise keep the current rule: `Body` when the body applies after Aim,
    else `Aim`;
  - record the current orthographic size.
- In the existing Body-stage double-callback path, keep the current
  `bodyCallsThisFrame` suppression so framing does not apply twice. Add a
  scenario test that combines an after-Aim body with Confiner2D to lock the
  ordering.
- After grouping/framing has adjusted the lens, compare the new orthographic
  size with the cached value. If it differs by more than `math::Epsilon`, call
  `invalidateLensCache()` and update the cached value. Match the reference,
  which checks the orthographic size rather than the perspective FOV.
- Reset the cached invalidator and previous size whenever the GroupFraming
  camera cache is reset.
- Add unit/scenario coverage:
  - Confiner2D present selects Body;
  - no Confiner2D keeps the existing Aim/after-Aim body selection;
  - an orthographic-size change invalidates exactly once;
  - an unchanged size does not invalidate;
  - cache reset clears the invalidator and previous size;
  - group framing still applies once with an after-Aim body.
- Update the task index, feature matrix, architecture notes, and example
  documentation.

## Non-goals

- Reimplementing Confiner2D or ConfinerOven baking (P6-02).
- Changing Confiner2D's slowing, damping, corner, or warp behavior.
- Editor gizmos, inspector auto-discovery, or Unity component lookup.
- Automatic invalidation for arbitrary perspective FOV changes beyond the
  reference orthographic-size check. If a caller changes a perspective FOV, it
  must call `invalidateLensCache()` explicitly, as in the reference API.

## Design

Reference behavior:

```text
if !vcam.PreviousStateIsValid:
    if vcam has Confiner2D:
        stage = Body
    else:
        stage = Aim
        if body component applies after Aim:
            stage = Body

if stage != currentStage:
    return

... apply framing ...

if confiner != null and abs(previousOrthoSize - lens.orthographicSize) > epsilon:
    confiner.InvalidateLensCache()
    previousOrthoSize = lens.orthographicSize
```

CineRig's pipeline invokes the Body extension callback both at the normal Body
stage and again after an after-Aim body. The current GroupFraming cache uses
`bodyCallsThisFrame` to skip the first Body callback in that case. P6-03 must
preserve that rule while adding Confiner2D detection.

The lens-cache interface avoids a `targets` -> `components` dependency and
keeps the module boundary documented in `architecture.md`.

## API Sketch

```cpp
class ICameraLensCacheInvalidator
{
public:
    virtual ~ICameraLensCacheInvalidator() = default;
    virtual void invalidateLensCache() = 0;
};

class GroupFraming : public CameraExtension
{
private:
    struct CameraCache
    {
        math::Vec3 positionAdjustment{};
        math::Vec2 rotationAdjustment{};
        float fovAdjustment = 0.0f;
        int bodyCallsThisFrame = 0;
        ICameraLensCacheInvalidator* lensCacheInvalidator = nullptr;
        float previousOrthoSize = 0.0f;
        Stage framingStage = Stage::Aim;
    };
};
```

`Confiner2D` publicly inherits `CameraExtension` and
`ICameraLensCacheInvalidator`; its existing `invalidateLensCache()` method
satisfies the interface.

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineGroupFraming.cs`
  - `VcamExtraState.Stage`, `Confiner`, `PreviousOrthoSize`, Body-stage
    compatibility mode, and `InvalidateLensCache()` call.
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineConfiner2D.cs`
  - `InvalidateLensCache()` contract and per-camera baked solution.
- `CineRig/src/targets/group_framing.cpp`
  - current stage selection and `bodyCallsThisFrame` suppression.
- `CineRig/include/cinerig/camera/camera_extension.hpp`
  - extension discovery and lifecycle.
- `CineRig/include/cinerig/components/confiner2d.hpp`
  - existing public `invalidateLensCache()`.

## Acceptance

- Attaching Confiner2D changes GroupFraming's selected stage to Body.
- Detaching or not attaching Confiner2D preserves the current stage rules.
- Framing that changes the orthographic size invalidates the Confiner2D lens
  cache exactly once per change; unchanged size does not invalidate.
- A reset clears the cached invalidator and previous size.
- The after-Aim body path still applies framing once.
- The implementation introduces no `targets` dependency on the concrete
  `components::Confiner2D` header.
- Existing GroupFraming and Confiner2D tests remain green; new integration tests
  cover the combined case.
- Core, example, install, and no-tests checks pass on Windows.

## Implementation Record

Implemented the camera-layer `ICameraLensCacheInvalidator` in
`camera_lens_cache_invalidator.hpp`; `camera_extension.hpp` includes it and
`cinerig.hpp` exports it. `components::Confiner2D` now derives from both
`CameraExtension` and the interface, and its existing `invalidateLensCache()` is
the override.

`targets::GroupFraming::CameraCache` now stores an enabled-extension
invalidator pointer, the previous orthographic size, and the selected framing
stage. Cache initialization runs on `!previousStateIsValid` and whenever the
cache is uninitialized, so an explicit `GroupFraming::reset()` re-detects the
attached Confiner2D even when the camera's previous state is valid. Discovery
scans enabled extensions only and selects `Stage::Body` when an invalidator is
found; otherwise the existing Aim / after-Aim-body rule is retained. The
`bodyCallsThisFrame` suppression is preserved for after-Aim bodies.

After orthographic or perspective framing, GroupFraming compares the resulting
`state.lens.orthographicSize` with the cached value and uses
`math::Epsilon`. A change calls `invalidateLensCache()` once and records the new
size. Perspective FOV changes do not invalidate automatically, matching the
reference's orthographic-size check. `CameraCache::reset()` clears the
invalidator, previous size, and selected stage, but deliberately leaves
`bodyCallsThisFrame` to the per-frame pre-pipeline reset so the after-Aim body
double-callback path cannot lose its first-call suppression.

Reference differences:

- C++ uses a non-owning interface pointer instead of a Unity component lookup;
  `targets` does not include the concrete `components::Confiner2D` header.
- There is no `Application.isPlaying` condition; initialization is driven by
  `!previousStateIsValid` plus the explicit cache-initialized flag.
- `GroupFraming::reset()` erases the per-camera cache and forces re-detection on
  the next callback, including when the previous state remains valid.

## Verification

- `[group-framing]`: 15 cases / 60 assertions passed.
- `[confiner2d]`: 14 cases / 66 assertions passed.
- Combined scenario: 1 case / 7 assertions passed.
- Unit: 338 cases / 3045 assertions passed.
- Scenario: 142 cases / 1161 assertions passed.
- Core Debug and Release: 510/510 CTest passed.
- Examples Debug and Release: 563/563 CTest passed.
- AddressSanitizer: 504/504 core tests passed, performance excluded.
- Performance: 6/6 cases passed with zero Release steady-state allocations.
- Installed `find_package(CineRig)` P6-03 consumer built and exited 0.
- `CINERIG_BUILD_TESTS=OFF` Release build passed.

The full verification baseline is recorded in
[runtime-parity.md](../runtime-parity.md) and
[release-readiness.md](../release-readiness.md).
