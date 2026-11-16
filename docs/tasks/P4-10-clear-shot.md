# P4-10 ClearShot

Status: completed  
Depends on: P1-04, P3-06  
Blocks: P4-11 Sequencer

## Goal

Port `CinemachineClearShot`. This manager selects the enabled child with the
best shot quality, using priority as a tie-breaker and optional timing gates
before switching the live camera.

## Scope

- Add `managers::ClearShot`:
  - `activateAfter`, `minDuration`, randomized tie-breaking, and a
    deterministic seed
  - shot-quality and priority child ranking
  - pending-camera activation and minimum-duration state machine
  - live-child invalidation and disabled/no-child handling
  - randomized child order cache and `resetRandomization()`
  - transition cache refresh and base blend integration
  - explicit `reset()`/`validate()` and manager cache invalidation
  - child state refresh for invalid child states
- Add scenario tests for defaults/reset, quality/priority selection, disabled
  and no-child handling, activation timing, minimum duration, pending changes,
  deterministic randomization, transitions, cache changes, and blend output.
- Add a `32_clear_shot` SDL example with three cameras, a moving target,
  quality evaluation, live-camera HUD, timing/randomization controls, and a
  smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity legacy look-at/follow upgrade fields and SaveDuringPlay.
- Editor inspectors, hierarchy discovery, and component enable callbacks.
- Global `UnityEngine.Random`; CineRig uses an explicit deterministic seed and
  `std::mt19937`.
- New shot-quality evaluation algorithms. ClearShot consumes
  `CameraState::shotQuality`, which can be produced by Deoccluder or a host
  extension.
- ClearShot-specific child update scheduling. The update manager owns camera
  updates; children with invalid states are refreshed, while stale states
  require the host's standby policy or `StandbyUpdateMode::Always`.
- Sequencer and StateDriven policies.

## Design

```text
managers::ClearShot : VirtualCameraManagerBase
  activateAfter / minDuration
  randomizeChoice / randomSeed / resetRandomization()
  activationTime / pendingActivationTime / pendingCamera
  randomized child order cache

  chooseCurrentCamera(worldUp, dt)
    reset timing when previous state is invalid
    reject no children
    build randomized order if requested
    discard invalid/disabled live child
    choose best by:
      shotQuality desc,
      priority desc,
      randomized alternate when randomizeNow and quality/priority tie
    if best is current live child:
      cancel pending and keep it
    if a pending child has waited long enough and the live child has met
      minDuration:
      activate pending, clear randomization
    if the live child has not met minDuration or activateAfter is nonzero:
      mark best pending and keep the live child
    otherwise activate best immediately
```

`activateAfter` waits after a pending candidate is first observed. The active
child must remain active for `minDuration` before any switch. Both timing
checks are strict (`>`), matching the original. A zero activation time retains
the original sentinel behavior.

Randomization uses `std::shuffle` on a copied child list, seeded by
`randomSeed`; `resetRandomization()` reseeds and requests a fresh tie-break on
the next update. Randomized order is discarded when the child list or
activation changes.

## API Sketch

```cpp
namespace cinerig::managers
{
class ClearShot final : public VirtualCameraManagerBase
{
public:
    float activateAfter = 0.0f;
    float minDuration = 0.0f;
    bool randomizeChoice = false;
    std::uint32_t randomSeed = 1;

    explicit ClearShot(std::string name = "ClearShot");

    void reset();
    void validate();
    void resetRandomization();

    const std::vector<VirtualCamera*>& randomizedChildOrder() const noexcept;

    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
    void invalidateCameraCache() override;
};
}
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineClearShot.cs`
  - activateAfter/minDuration state machine, shot quality + priority ranking,
    randomized order, transition cache reset, and live-child validity checks
- `com.unity.cinemachine/Runtime/Core/CinemachineCameraManagerBase.cs`
  - child cache, previous-state invalidation, live child, and blend integration
- `CineRig/include/cinerig/core/camera_state.hpp`
  - `shotQuality` consumed during selection
- `CineRig/include/cinerig/components/deoccluder.hpp`
  - quality evaluation producer used by the example

## Acceptance

- Defaults are zero timing gates, no randomization, seed `1`, and EaseInOut
  `0.5s` default blend; reset restores them and clears cache state.
- The highest positive `shotQuality` enabled child wins; equal quality uses the
  higher effective priority, then child order.
- Disabled children and invalid live children are excluded.
- With no children, the manager returns no live child and resets timing.
- With `activateAfter`/`minDuration`, the current live child remains until both
  timers permit the pending camera to activate.
- The pending camera is cancelled if the live child becomes best again.
- `resetRandomization()` produces a deterministic order from `randomSeed`;
  random tie-breaking can select a different equal-quality child.
- `onTransitionFromCamera` clears the randomized order when not blending and
  delegates to the base manager.
- Child cache changes clear timing, pending, and randomized state.
- Selection changes use the manager's normal blend path; Finalize and owner
  updates still flow through the base manager.
- Scenario tests, examples Debug/Release, smoke tests, no-tests build, and
  install pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/managers/clear_shot.hpp` and
  `src/managers/clear_shot.cpp`
  - activateAfter/minDuration with pending-camera state machine
  - quality then effective-priority ranking
  - disabled/invalid live-child handling and no-child reset
  - deterministic `std::mt19937` randomized order with `resetRandomization()`
  - transition cache reset, cache invalidation, explicit reset/validation
  - invalid child-state refresh before evaluation
  - inherited manager blending, Finalize, and owner writes
- `tests/scenario/clear_shot_test.cpp`
  - 7 cases and 45 assertions covering defaults/reset, quality/priority
    selection, disabled/no children, activateAfter, minDuration, pending
    cancellation, deterministic refresh of equal-quality shots, and manager
    blend integration
- `examples/32_clear_shot`
  - two shot cameras with Deoccluder quality evaluation, a wall occluding camera
    A, a moving target, timing/randomization controls, quality/live HUD, and a
    smoke test
  - in-application framebuffer capture (`--capture file.bmp`) added to the
    shared example `App` and used for final visual verification
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Children are explicitly registered through `VirtualCameraManagerBase`
  instead of discovered through a Unity Transform hierarchy.
- Unity's global `Random` is replaced by a deterministic `std::mt19937` seeded
  by `randomSeed`; `resetRandomization()` reseeds and requests a fresh
  equal-quality tie-break.
- Unity component active/enabled hierarchy checks are reduced to CineRig
  `isEnabled()`/`isValid()` checks on explicitly owned child cameras.
- Legacy look-at/follow upgrade fields, SaveDuringPlay, editor callbacks, and
  serialized inspectors are not ported.
- The update manager still owns child update scheduling. ClearShot refreshes
  children with invalid states but does not force every candidate to update
  each frame; examples use `StandbyUpdateMode::Always`.
- The manager uses the existing CineRig blend manager for live-camera
  transitions; there is no separate ClearShot blend implementation.
- The shared example app now supports a captured BMP framebuffer so screenshots
  do not depend on desktop window stacking.

### Verification

- `[clearshot]`: **7 cases / 45 assertions passed**.
- Core Debug and Release: **415/415 passed**
  (319 unit + 90 scenario + 6 golden).
- Examples Debug and Release: **462/462 passed**
  (415 core + 47 example tests, including 32 smoke tests).
- `cinerig_32_clear_shot --frames 180 --hidden --no-vsync` exits 0 in Debug
  and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/managers/clear_shot.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, validated, and reset a
  `managers::ClearShot`.
- The captured view at the blocked-target phase shows shot B live, shot A at
  quality `0.2329`, shot B at `1.0557`, the wall, target, and timing HUD.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_clear_shot.png`.
- Windows was verified; Linux was not tested on hardware.
