# P3-04 Impulse Core

Status: completed  
Depends on: P0-02, P0-04, P1-02, P3-03  
Blocks: collision impulse source, impulse-driven camera reactions

## Goal

Implement the core Cinemachine impulse system in CineRig: a per-world impulse
manager owning active events and envelopes, a definition that creates shaped
impulse events, an event source helper, and a camera extension that listens for
and combines impulses.

## Scope

- Add `ISignalSource6D` as the raw six-dimensional signal interface.
- Add `impulse::ImpulseManager` with:
  - active and recycled event ownership
  - `EnvelopeDefinition` attack, sustain, decay, hold, and stop-time behavior
  - event expiration, distance decay, propagation delay, and direction modes
  - additive and strongest-signal retrieval with channel masks
  - deterministic time from `IEntityWorld`
- Add `impulse::ImpulseDefinition` with:
  - Uniform, Dissipating, Propagating, and Legacy impulse types
  - Recoil, Bump, Explosion, Rumble, and Custom signal shapes
  - duration, channel, dissipation, propagation, and impact radius settings
  - deterministic legacy randomization and gain/time scaling
- Add `impulse::ImpulseSource` helper methods for generating events from a
  position, velocity, force, or defaults.
- Add `impulse::ImpulseListener` `CameraExtension` with:
  - ApplyAfter stage selection
  - additive/strongest signal filtering and channel masks
  - gain, 2D distance, and camera-space options
  - optional secondary noise reaction with deterministic re-seeding
- Add `math::slerpUnclamped` for impulse amplitude scaling above one.
- Expose the manager from `CameraSystem`.
- Add unit coverage for envelopes, manager retrieval and expiration, distance
  decay, propagation, rotation toward source, modern and legacy definitions,
  source helpers, listener reactions, and camera-system ownership.
- Add a `19_impulse` SDL example with manual/automatic impulses, shape selection,
  clear, HUD, and smoke test.
- Update the task index, feature matrix, architecture, and example index.

## Non-goals

- Unity collision callbacks or a collision impulse source; that remains a
  separate task.
- Global singleton state, editor attributes, or serialization.
- `IgnoreTimeScale`; CineRig impulse time always follows `IEntityWorld` time.
- Target-position cache playback and animation-curve editor tooling.

## Design

The manager is owned by `CameraSystem`, not global. Definitions and sources take
a manager pointer so multiple worlds remain independent.

```text
source -> definition.createEvent -> manager.addImpulseEvent
listener (ApplyAfter stage)
  -> manager.getImpulseAt or getStrongestImpulseAt
  -> optional secondary noise reaction
  -> positionCorrection / orientationCorrection
```

Impulse signals are authored in the original six-dimensional form. Position
impulses use world axes. Camera-space application is selected by the listener
and rotates the result by the raw camera orientation, matching Unity.

Signal shapes use cubic Hermite curves without the value clamping used by
`math::Curve::evaluate`, because Explosion and other impulse shapes may exceed
or cross the unit range. Envelope and custom envelope curves remain normalized.

Legacy definitions keep the original raw signal, gain, repeat, randomization,
time-envelope, radius, direction, and dissipation behavior. Legacy randomization
uses the same deterministic 32-bit mixer foundation as P3-03.

## API Sketch

```cpp
class ISignalSource6D
{
public:
    virtual ~ISignalSource6D() = default;
    virtual float signalDuration() const = 0;
    virtual void getSignal(float time, math::Vec3& pos, math::Quat& rot) const = 0;
};

class ImpulseManager
{
public:
    explicit ImpulseManager(IEntityWorld& world);

    std::unique_ptr<ImpulseEvent> newImpulseEvent();
    void addImpulseEvent(std::unique_ptr<ImpulseEvent> event);
    bool getImpulseAt(
        math::Vec3 listener, bool distance2D, std::uint32_t channelMask,
        math::Vec3& pos, math::Quat& rot);
    bool getStrongestImpulseAt(
        math::Vec3 listener, bool distance2D, std::uint32_t channelMask,
        math::Vec3& pos, math::Quat& rot);
    void clear();
};

class ImpulseListener : public CameraExtension
{
public:
    ImpulseManager* impulseManager = nullptr;
    Stage applyAfter = Stage::Noise;
    std::uint32_t channelMask = 1;
    float gain = 1.0f;
    bool use2DDistance = false;
    bool useCameraSpace = true;
    SignalCombinationMode signalCombinationMode = SignalCombinationMode::Additive;
    ImpulseReaction reaction{};
};
```

## Reference

- `com.unity.cinemachine/Runtime/Impulse/CinemachineImpulseManager.cs`
  - envelopes, events, decay, propagation, retrieval, and recycling
- `com.unity.cinemachine/Runtime/Impulse/CinemachineImpulseDefinition.cs`
  - modern shapes, legacy signals, event construction, and validation
- `com.unity.cinemachine/Runtime/Impulse/CinemachineImpulseSource.cs`
  - source helper APIs and default invocation
- `com.unity.cinemachine/Runtime/Impulse/CinemachineImpulseListener.cs`
  - filtering, gain, camera space, and secondary reaction

## Acceptance

- Envelope defaults and attack/sustain/decay behavior match the original.
- Expired events are pruned and recycled.
- Channel masks and additive/strongest combination work.
- Distance decay supports linear, soft, exponential, and custom dissipation.
- Propagation speed delays the signal with distance.
- RotateTowardSource rotates position impulses toward the listener.
- Modern shapes create valid events; invalid custom/duration/distance cases do
  not.
- Legacy signal gain, time scaling, stretch/loop, and randomization are
  deterministic.
- Source position/velocity/force/default methods generate the expected events.
- Listener applies at the selected stage, uses gain, camera space, and 2D
  distance, and combines primary and secondary reactions.
- `CameraSystem` owns an impulse manager whose time follows `IEntityWorld`.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record


### Implemented

- `include/cinerig/core/signal_source.hpp`
  - six-dimensional raw signal interface with duration and random-access sample.
- `include/cinerig/impulse/impulse_manager.hpp` and
  `src/impulse/impulse_manager.cpp`
  - envelope attack/sustain/decay, hold-forever, stop-time, and validation
  - owned active/recycled impulse events with unique ownership
  - channel-masked additive and strongest-signal retrieval
  - distance decay: linear, soft, exponential, and custom Bezier dissipation
  - propagation delay, 2D distance, rotate-toward-source, expiration, cancel
  - manager time from `IEntityWorld`.
- `include/cinerig/impulse/impulse_definition.hpp` and
  `src/impulse/impulse_definition.cpp`
  - Recoil, Bump, Explosion, Rumble, and Custom shapes with unclamped Hermite
    evaluation
  - Uniform, Dissipating, and Propagating event construction
  - legacy raw signal source, gain/frequency scaling, stretch/loop, deterministic
    randomization, envelope scaling, and dissipation settings
- `include/cinerig/impulse/impulse_source.hpp` and
  `src/impulse/impulse_source.cpp`
  - source defaults, position, default velocity, force, velocity, and explicit
    position APIs.
- `include/cinerig/impulse/impulse_listener.hpp` and
  `src/impulse/impulse_listener.cpp`
  - ApplyAfter stage, channel mask, gain, 2D distance, camera space, additive or
    strongest combination
  - secondary noise reaction with deterministic re-seeding and Unity-to-CineRig
    axis conversion.
- `math::slerpUnclamped` for impulse/gain extrapolation.
- `CameraSystem` owns the per-world `ImpulseManager` and exposes it publicly.
- `tests/unit/impulse_test.cpp`
  - 12 cases and 96 assertions covering envelopes, pooling, filtering,
    distance/propagation/rotation, modern/legacy definitions, deterministic
    randomization, source helpers, listener stage/space/gain, secondary
    reaction, cancellation, clear, and camera-system ownership.
- `examples/19_impulse`
  - propagating Recoil/Bump/Explosion/Rumble events, auto/manual fire, wave ring,
    camera shake, secondary reaction, reset/clear, HUD, and smoke test.

### Differences

- The Unity singleton is replaced by one manager per `CameraSystem` / world.
- Impulse time follows `IEntityWorld::time()`; `IgnoreTimeScale` is omitted.
- Events use `std::unique_ptr` ownership. `ImpulseDefinition` and the listener
  store non-owning manager pointers; legacy raw signals must outlive active
  events that sample them.
- Legacy randomization uses a deterministic 32-bit mixer instead of Unity's
  global random generator.
- Custom shape curve editor normalization is not implemented; supplied keyframes
  are evaluated directly.
- The collision impulse source remains a separate planned task.

### Verification

- Core Debug: **305/305 passed** (224 unit + 75 scenario + 6 golden).
- Core Release: **305/305 passed**.
- Examples Debug: **339/339 passed** (305 core + 34 example tests, including 19
  smoke tests).
- Examples Release: **339/339 passed**.
- `cinerig_19_impulse --frames 120 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 19 example exits 0.
- Visible window shows active/recycled events, expanding propagation ring,
  impulse correction, listener settings, and controls. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_impulse.png`.
- Linux was not verified on hardware.
