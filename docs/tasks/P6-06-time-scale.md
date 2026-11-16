# P6-06 Time Scale Semantics

Status: completed
Depends on: P0-05, P1-03, P3-04
Blocks: slow-motion gameplay that must keep camera blends or impulse reactions
on unscaled time

## Goal

Implement the time-scale contracts that are currently exposed but unused:

- `BrainConfig::ignoreTimeScale`;
- impulse event time selection (`IgnoreTimeScale` in the reference manager).

CineRig remains host-driven: the adapter owns scaled and unscaled time. This
task adds the optional world hooks and wiring needed for Brain and impulse
systems to choose between them.

## Scope

- Extend `IEntityWorld` with non-pure virtual hooks:

```cpp
virtual float unscaledDeltaTime(UpdateClock clock) const
{
    return deltaTime(clock);
}

virtual float unscaledTime() const
{
    return time();
}
```

  The defaults preserve current behavior for existing adapters.
- Add corresponding settings to `FakeEntityWorld` for deterministic tests:
  - `setUnscaledDeltaTime(UpdateClock, float)`;
  - `setUnscaledTime(float)`.
- Update `CameraBrain::effectiveDeltaTime` to accept the active `UpdateClock`:
  - a positive `BlendManager` delta-time override still wins;
  - when `config_.ignoreTimeScale` is true, use
    `IEntityWorld::unscaledDeltaTime(clock)`;
  - otherwise use the caller-provided scaled delta.
- Ensure the effective delta is applied once. `updateInternal` and
  `processFrame` must not repeatedly convert an already-effective delta.
- Fixed-clock Brain updates use the world's unscaled fixed delta (the default
  hook returns the normal fixed delta), matching the reference distinction
  between fixed and render clocks.
- Add `bool ignoreTimeScale = false` to `impulse::ImpulseManager`.
  `ImpulseManager::currentTime()` uses `world_.unscaledTime()` when enabled and
  `world_.time()` otherwise.
- Add tests for:
  - Brain blends with `ignoreTimeScale=false` using scaled delta;
  - Brain blends with `ignoreTimeScale=true` using unscaled delta;
  - fixed-clock behavior with the default and overridden hooks;
  - BlendManager delta-time override precedence;
  - impulse expiration and propagation using scaled vs unscaled time;
  - default adapters preserving the existing behavior.
- Update the impulse example or a small scenario to show unscaled impulse time
  while the camera blend remains scaled, if practical.
- Update the task index, feature matrix, architecture, and release notes.

## Non-goals

- Reimplementing Unity's global `Time.timeScale`.
- `CinemachineCore.UniformDeltaTimeOverride`.
- Solo-camera unscaled time. Solo camera is deferred.
- Changing `CameraSystem`'s caller-supplied `fixedUpdate`/`lateUpdate` deltas.
  The host remains responsible for passing the correct clock values.
- Using unscaled time for simulation, target motion, physics, or `TargetGroup`.
- Adding `std::chrono` to the core update path.

## Design

Reference behavior in `CinemachineBrain.GetEffectiveDeltaTime`:

```text
if UniformDeltaTimeOverride >= 0: return it
if SoloCamera != null: return unscaledDeltaTime
if !Application.isPlaying: return blendManager delta override
if IgnoreTimeScale:
    return fixedDelta ? fixedDeltaTime : unscaledDeltaTime
return fixedDelta ? fixedDeltaTime : deltaTime
```

CineRig does not have SoloCamera or `UniformDeltaTimeOverride`. Its effective
order is:

```text
override = blendManager.deltaTimeOverride()
if override > 0:
    return override
if config.ignoreTimeScale:
    return world.unscaledDeltaTime(clock)
return callerDeltaTime
```

`unscaledDeltaTime(UpdateClock::Fixed)` defaults to the ordinary fixed delta.
An adapter that has a different fixed-step time scale can override both hooks.

Impulse time follows the same idea:

```text
ImpulseManager::currentTime():
    return ignoreTimeScale ? world.unscaledTime() : world.time()
```

The reference manager uses `Time.realtimeSinceStartup` for this option. CineRig
uses the host's `unscaledTime()` so multiple worlds remain independent and
deterministic under host control.

The manager does not store a separate clock; it remains a function of the
current world time. Enabling or disabling `ignoreTimeScale` while events are
active is allowed but can change event age/propagation; tests should cover the
switch.

## API Sketch

```cpp
class IEntityWorld
{
public:
    virtual float unscaledDeltaTime(UpdateClock clock) const
    {
        return deltaTime(clock);
    }

    virtual float unscaledTime() const
    {
        return time();
    }
};

class CameraBrain
{
private:
    float effectiveDeltaTime(UpdateClock clock, float deltaTime) const;
};

class ImpulseManager
{
public:
    bool ignoreTimeScale = false;
    float currentTime() const noexcept;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs`
  - `IgnoreTimeScale`, fixed/render delta selection, and effective delta
    precedence.
- `com.unity.cinemachine/Runtime/Impulse/CinemachineImpulseManager.cs`
  - `IgnoreTimeScale` and `CurrentTime`.
- `com.unity.cinemachine/Runtime/Core/CinemachineCore.cs`
  - unscaled-time override used by the reference for editor/test control.
- `CineRig/include/cinerig/engine/entity_world.hpp`
  - current clock/time contract.
- `CineRig/include/cinerig/brain/camera_brain.hpp`
  and `CineRig/src/brain/camera_brain.cpp`
  - current `effectiveDeltaTime` path.
- `CineRig/include/cinerig/impulse/impulse_manager.hpp`
  - current `currentTime()` that always uses scaled world time.
- `CineRig/tests/support/fake_entity_world.hpp`
  - deterministic clock support to extend.

## Acceptance

- Existing adapters compile without implementing new pure virtual methods.
- `ignoreTimeScale=false` preserves current Brain and impulse behavior.
- `ignoreTimeScale=true` uses unscaled delta/time for Brain and impulse.
- The BlendManager override still takes precedence over `ignoreTimeScale`.
- Fixed-clock updates use the world's unscaled fixed delta, not the render
  unscaled delta.
- Effective delta is applied once; tests verify blend progress for a fixed
  number of frames.
- Impulse expiration, propagation, and reaction time use the selected clock.
- Core, example, install, and no-tests checks pass on Windows.

## Implementation Record

- Added non-pure `IEntityWorld::unscaledDeltaTime(UpdateClock)` and
  `IEntityWorld::unscaledTime()` hooks. Their defaults return the existing
  scaled delta and time, so every current adapter keeps its behavior without
  source changes.
- Extended `testing::FakeEntityWorld` with `setUnscaledDeltaTime(clock, delta)`
  and `setUnscaledTime(time)`. Until explicitly set, the fake hooks fall back to
  the scaled clock values; `reset()` clears the overrides.
- Replaced `CameraBrain`'s delta-only helper with
  `effectiveDeltaTime(UpdateClock, float)`. Precedence is now:
  blend-manager override when positive, then `config_.ignoreTimeScale` with the
  world unscaled delta for the active clock, otherwise the caller delta.
- Split blend application into `processFrameInternal()`. Public
  `processFrame()` resolves the effective delta once; `updateInternal()` resolves
  it once and calls the internal path with that value. This prevents an
  already-effective delta from being converted again by the brain.
- Fixed-clock Brain updates use `unscaledDeltaTime(UpdateClock::Fixed)`. The
  default hook returns the normal fixed delta; an adapter can override it for a
  different fixed-step time scale.
- Added `impulse::ImpulseManager::ignoreTimeScale`. `currentTime()` selects
  `IEntityWorld::unscaledTime()` when enabled and `time()` otherwise; event start,
  expiration, propagation, and the reaction sample time all use that value.
- Added scenario coverage in `brain_time_scale_test.cpp` for scaled default
  blends, unscaled blends applied once over four frames, fixed-clock default
  and overridden hooks, blend-override precedence, and independent scaled brain
  and unscaled impulse clocks.
- Added impulse unit coverage for scaled/unscaled selection, switching clocks
  while an event is active, expiration, propagation, and reaction sampling.
- Extended the engine interface and fake-world tests to cover default hooks,
  explicit overrides, and reset fallback behavior.

### Reference Differences

- CineRig has no `SoloCamera` or `UniformDeltaTimeOverride`; the only
  delta-time override is the active `BlendManager` frame.
- Unity has an editor/not-playing branch that returns the blend-manager
  override. CineRig is host-driven and always uses the runtime precedence above.
- Unity uses `Time.fixedDeltaTime` for fixed-clock Brain updates. CineRig uses
  `IEntityWorld::unscaledDeltaTime(UpdateClock::Fixed)`, whose default matches
  the world's fixed delta while allowing deterministic adapters to supply their
  own value.
- Unity's impulse manager uses `Time.realtimeSinceStartup` when ignoring time
  scale. CineRig uses the host world's `unscaledTime()`, keeping multiple worlds
  independent and deterministic.
- The impulse manager does not cache a separate clock. Toggling
  `ignoreTimeScale` while events are active is allowed and immediately changes
  the time used for age, expiration, propagation, and reaction sampling.

## Verification

Windows verification after P6-06:

- Brain time-scale scenario slice: **5 cases / 18 assertions**;
- impulse time-scale unit slice: **3 cases / 25 assertions**;
- impulse suite: **20 cases / 158 assertions**;
- unit: **354 cases / 3168 assertions**;
- scenario: **148 cases / 1188 assertions**;
- parity: **18 cases / 229 assertions**;
- performance: **7/7** cases, zero steady-state allocations;
- core Debug/Release: **533/533** CTest;
- examples Debug/Release: **586/586** CTest;
- AddressSanitizer Release: **526/526** core tests, performance excluded;
- installed-package consumer rebuilt and exited 0;
- `CINERIG_BUILD_TESTS=OFF` Release build passed.
