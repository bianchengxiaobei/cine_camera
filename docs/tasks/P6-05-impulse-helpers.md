# P6-05 Impulse Helpers

Status: completed
Depends on: P3-04, P0-05
Blocks: reusable impulse sources and non-camera impulse listeners

## Goal

Add the two reference impulse helpers that are missing from CineRig:

- `impulse::FixedSignal`, a `ISignalSource6D` backed by three authored curves;
- `impulse::ExternalImpulseListener`, a host-driven helper that applies impulse
  reactions to an arbitrary entity instead of only to a camera extension.

These are runtime building blocks, not Unity editor integration. They let a host
create authored impulse signals and shake props, cameras, or other scene
objects through the same `ImpulseManager` as CineRig cameras.

## Scope

- Add `impulse::FixedSignal : ISignalSource6D`:
  - X, Y, and Z curves;
  - curve evaluation in seconds, not normalized time;
  - `signalDuration()` equal to the maximum `(lastKeyTime - firstKeyTime)` of
    the three non-empty curves, or `0` for empty/single-key curves;
  - `getSignal()` returning identity rotation and the three curve values;
  - missing or empty curves evaluate to zero;
  - no clamping of curve values to `[0, 1]`; authored impulses may exceed the
    unit range;
  - no clamping of input time before the first key or after the last key beyond
    returning the first/last key value.
- Add a small raw-curve evaluation path to `math::Curve`:
  - existing normalized `evaluate()` behavior must not change;
  - add `evaluateUnclamped(float time)` (or an equivalent private raw
    evaluator) that supports arbitrary key times and values while still using
    the same cubic Hermite interpolation.
- Add `impulse::ExternalImpulseListener`:
  - non-owning `ImpulseManager*`;
  - `EntityId targetEntity = 0`;
  - `std::uint32_t channelMask = 1`;
  - `float gain = 1.0f`;
  - `bool use2DDistance = false`;
  - `bool useLocalSpace = true`;
  - `ImpulseListener::ImpulseReaction reaction{}`;
  - `void update(float deltaTime)`;
  - `void reset()`;
  - `bool hasAppliedImpulse() const`;
  - `math::Vec3 lastPositionOffset() const`;
  - `math::Quat lastRotationOffset() const`.
- The external listener must:
  - remove the previous applied offset before querying the new impulse when
    write-back succeeded;
  - query at the current entity position using additive signal combination;
  - apply gain, 2D distance, local-space rotation, and the secondary reaction;
  - write the result through `IEntityWorld::trySetTransform()`;
  - keep the last offsets available when the entity is read-only, without
    repeatedly accumulating them;
  - be safe when the target entity is missing, the manager is null, or the
    host does not call `update()`.
- Add unit tests for:
  - fixed signal duration, empty axes, arbitrary curve times, and values above
    one;
  - external listener channel/gain/2D/local-space behavior;
  - applying and unapplying offsets;
  - read-only entities exposing offsets but not changing the transform;
  - secondary reaction;
  - reset behavior and missing target/manager.
- Extend `examples/19_impulse` with a non-camera prop that shakes through the
  external listener and shows its applied offset in the HUD.
- Update the task index, feature matrix, architecture notes, and example docs.

## Non-goals

- Unity `AnimationCurve`, `SignalSourceAsset`, serialization, or inspector
  tooling.
- `CinemachineExternalImpulseListener` as a `MonoBehaviour`; the C++ helper is
  explicitly host-driven.
- A new impulse combination policy. The external listener uses the existing
  additive `getImpulseAt` path.
- `IgnoreTimeScale`; that is P6-06.
- Changing impulse envelope, decay, propagation, or camera listener behavior.

## Design

### Fixed signal

The reference `CinemachineFixedSignal` evaluates an `AnimationCurve` in
seconds. Its duration is the maximum span of the three axes, and its signal
values are not clamped. `math::Curve` currently normalizes time to `[0, 1]` and
clamps values, so it cannot be used directly.

```text
duration = max(axisDuration(x), axisDuration(y), axisDuration(z))
axisDuration(curve):
    if curve.size() <= 1: 0
    else: keys.back().time - keys.front().time

getSignal(t):
    pos = { x(t), y(t), z(t) }
    rot = identity
```

The raw evaluator must keep the reference Hermite behavior for keys inside the
authored range and return the first/last value outside it.

### External listener

The reference `CinemachineExternalImpulseListener` is a Unity `MonoBehaviour`
that unapplies its previous shake in `Update`, queries in `LateUpdate`, and
writes the entity transform. CineRig exposes the same sequence as an explicit
`update(deltaTime)` call so the host controls execution order.

```text
if writeBackSucceeded:
    transform.position -= lastWorldOffset
    transform.rotation = transform.rotation * inverse(lastRotationOffset)

query = manager.getImpulseAt(transform.position, use2DDistance, channelMask)
apply gain
add reaction
if useLocalSpace:
    worldOffset = transform.rotation * localOffset
else:
    worldOffset = localOffset

if writeBackSucceeded:
    transform.position += worldOffset
    transform.rotation = transform.rotation * rotationOffset
    world.trySetTransform(targetEntity, transform)

lastWorldOffset = worldOffset
lastRotationOffset = rotationOffset
```

For read-only entities, `writeBackSucceeded` is false, so the helper never
unapplies an offset that the host did not write. The last computed offsets
remain available for the host adapter to apply.

## API Sketch

```cpp
namespace cinerig::impulse
{

class FixedSignal final : public ISignalSource6D
{
public:
    math::Curve xCurve;
    math::Curve yCurve;
    math::Curve zCurve;

    float signalDuration() const override;
    void getSignal(
        float timeSinceSignalStart,
        math::Vec3& pos,
        math::Quat& rot) const override;
};

class ExternalImpulseListener
{
public:
    explicit ExternalImpulseListener(ImpulseManager* manager = nullptr);

    ImpulseManager* impulseManager = nullptr;
    EntityId targetEntity = 0;
    std::uint32_t channelMask = 1u;
    float gain = 1.0f;
    bool use2DDistance = false;
    bool useLocalSpace = true;
    ImpulseListener::ImpulseReaction reaction{};

    void update(float deltaTime);
    void reset();

    bool hasAppliedImpulse() const noexcept;
    math::Vec3 lastPositionOffset() const noexcept;
    math::Quat lastRotationOffset() const noexcept;
};

} // namespace cinerig::impulse
```

The helper obtains the entity world from `ImpulseManager::entityWorld()`. The
host must keep the manager and its world alive for the listener lifetime; a null
manager makes `update()` a safe no-op.

## Reference

- `com.unity.cinemachine/Runtime/Impulse/CinemachineFixedSignal.cs`
  - three-axis curves, duration, and signal evaluation.
- `com.unity.cinemachine/Runtime/Impulse/CinemachinExternalImpulseListener.cs`
  - channel mask, gain, 2D distance, local space, reaction, unapply/apply
    sequence, and read-only behavior implied by `LateUpdate`.
- `CineRig/include/cinerig/core/signal_source.hpp`
  - `ISignalSource6D`.
- `CineRig/include/cinerig/impulse/impulse_listener.hpp`
  - `ImpulseReaction` and camera-extension listener behavior.
- `CineRig/include/cinerig/impulse/impulse_manager.hpp`
  - `getImpulseAt`.
- `CineRig/include/cinerig/engine/entity_world.hpp`
  - `trySetTransform`.
- `CineRig/src/impulse/impulse_definition.cpp`
  - existing raw signal and curve helpers.

## Acceptance

- A fixed signal with empty curves returns zero position and identity rotation,
  and duration `0`.
- Fixed signal duration and values match the authored keys, including values
  above one and key times outside `[0, 1]`.
- The existing normalized `math::Curve::evaluate()` results are unchanged.
- The external listener applies channel filtering, gain, 2D distance, local
  space, and secondary reaction consistently with the camera listener.
- Consecutive updates do not accumulate the previous offset when write-back is
  enabled.
- A read-only entity remains unchanged, but the last offsets are exposed.
- Missing target/manager and reset are safe no-ops.
- The `19_impulse` example demonstrates an external prop shake.
- Core, example, install, and no-tests checks pass on Windows.

## Implementation Record

- Added `math::Curve::evaluateUnclamped()`. It uses the existing cubic Hermite
  interpolation with arbitrary key times and returns the first/last value
  outside the authored range. `Curve::evaluate()` now clamps time and the raw
  result, preserving all existing normalized behavior.
- Added `impulse::FixedSignal` with `xCurve`, `yCurve`, and `zCurve`. Duration is
  the largest `(lastKeyTime - firstKeyTime)` across the non-empty curves;
  empty and single-key axes contribute zero. Evaluation is in seconds, missing
  axes evaluate to zero, and values are never clamped to the unit range.
- Added `impulse::ExternalImpulseListener` with the requested manager, target,
  channel mask, gain, 2D-distance, local-space, and reaction settings.
- The external listener restores its prior successful write before querying,
  obtains an additive impulse at the base entity position, applies gain and the
  secondary reaction, converts local translation with the base rotation, and
  writes through `IEntityWorld::trySetTransform()`.
- The listener keeps last computed offsets even when the entity is read-only.
  It tracks the last successfully written offset separately, so repeated
  read-only updates expose the current offset without accumulating the prior
  one, and a later writable update can still restore the entity.
- Missing targets, null managers, negative delta time, and target changes are
  safe. `reset()` best-effort restores the last written offset, clears exposed
  offsets, and resets the secondary reaction state.
- Added `ImpulseReaction::reset()` so an external listener can stop a secondary
  reaction without discarding its configured noise or gains.
- Extended `19_impulse` with an orange prop driven by the external listener and
  a HUD readout of its applied translation/rotation offset.

### Reference Differences

- `CinemachineExternalImpulseListener` is a Unity `MonoBehaviour`; the CineRig
  helper is host-driven and exposes `update(deltaTime)`.
- Unity transform writes always succeed. CineRig uses `trySetTransform()`;
  read-only entities keep their transform and expose the computed offsets for a
  host adapter.
- `reset()` performs a best-effort restore of the last written offset. The
  Unity `OnEnable()` path only clears the cached offsets.
- Local-space mode rotates the translation offset. Rotation offsets remain
  world-space, matching the reference listener.
- The fixed-signal evaluator uses CineRig's Hermite curve representation rather
  than Unity `AnimationCurve`; authored key times, no value clamping, and
  endpoint behavior follow the reference contract.

## Verification

Windows verification after P6-05:

- Impulse unit slice: **17 cases / 138 assertions**;
- unit: **351 cases / 3145 assertions**;
- scenario: **142 cases / 1161 assertions**;
- parity: **18 cases / 229 assertions**;
- performance: **7/7** cases, zero steady-state allocations;
- core Debug/Release: **524/524** CTest;
- examples Debug/Release: **577/577** CTest;
- AddressSanitizer: **517/517** core tests, performance excluded;
- installed-package consumer using `FixedSignal` and
  `ExternalImpulseListener` exited 0;
- `CINERIG_BUILD_TESTS=OFF` Release build passed.
