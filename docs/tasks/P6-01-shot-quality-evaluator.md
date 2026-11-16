# P6-01 Shot Quality Evaluator

Status: completed
Depends on: P0-05, P2-01, P3-06, P4-10
Blocks: complete quality-driven ClearShot behavior

## Goal

Port `CinemachineShotQualityEvaluator` as an engine-neutral camera extension.
The extension evaluates target occlusion and optional target distance in the
Finalize stage and writes `CameraState::shotQuality`. ClearShot then has a
first-class quality producer instead of relying on Deoccluder's private
evaluation.

## Scope

- Add `core::IShotQualityEvaluator` as an empty marker interface in
  `include/cinerig/core/shot_quality.hpp`.
- Make the existing `Deoccluder` implement the marker interface.
- Add `components::ShotQualityEvaluator : CameraExtension,
  IShotQualityEvaluator` with:
  - `LayerMask occlusionLayers{1u}`
  - `EntityId ignoreEntity = 0`
  - `float minimumDistanceFromTarget = 0.2f`
  - `float cameraRadius = 0.0f`
  - `DistanceEvaluationSettings distanceEvaluation{}` with defaults
    `enabled=false`, `optimalDistance=10`, `nearLimit=5`, `farLimit=30`,
    `maxQualityBoost=0.2`
  - `reset()` and `validate()`
  - Finalize-stage evaluation
  - offscreen and spherecast occlusion checks
  - distance-based quality boost
  - no-op behavior when there is no look-at target, no physics adapter, or an
    empty layer mask
- Export the new header from `cinerig.hpp`, add it to CMake, and add unit and
  scenario coverage.
- Update `examples/32_clear_shot` to use the evaluator for its quality source
  and disable Deoccluder quality evaluation so the same shot is not penalized
  twice.
- Update the task index, feature matrix, architecture notes, and example
  documentation.

## Non-goals

- Unity tags, `TagField`, `MonoBehaviour`, `ExecuteAlways`, or serialization.
- A global shot-quality registry. ClearShot continues to consume
  `CameraState::shotQuality` from the child state.
- New physics adapters. Occlusion uses the existing `IPhysicsWorld`.
- Changing ClearShot's selection or timing state machine.
- Editor debug visualization of the evaluated ray or spherecast.

## Design

The reference extension runs at `CinemachineCore.Stage.Finalize` and only acts
when the state has a look-at point:

```text
if stage != Finalize or !state.hasLookAt():
    return

targetObscured = state.isTargetOffscreen() or targetIsObscured(state)
if targetObscured:
    state.shotQuality *= 0.2

if distanceEvaluation.enabled and optimalDistance > 0:
    distance = length(state.referenceLookAt - state.finalPosition())
    if distance <= optimalDistance:
        if distance >= nearLimit:
            boost = maxQualityBoost
                * (distance - nearLimit)
                / (optimalDistance - nearLimit)
    else:
        distance -= optimalDistance
        if distance < farLimit:
            boost = maxQualityBoost * (1 - distance / farLimit)
    state.shotQuality *= 1 + boost
```

`targetIsObscured` casts from `state.correctedPosition()` toward the final
look-at point. The cast is shortened by `minimumDistanceFromTarget` so the
target itself is not counted as an obstacle:

```text
direction = referenceLookAt - correctedPosition
distance = length(direction)
if distance < max(minimumDistanceFromTarget, epsilon):
    return true
return physics.sphereCast(
    Ray{correctedPosition, normalize(direction)},
    cameraRadius,
    distance - minimumDistanceFromTarget,
    QueryFilter{occlusionLayers, ignoreEntity}).has_value()
```

`validate()` must match the reference clamps:

- `cameraRadius = max(0, cameraRadius)`
- `minimumDistanceFromTarget = max(0.01, minimumDistanceFromTarget)`
- `nearLimit = max(0.1, nearLimit)`
- `farLimit = max(nearLimit, farLimit)`
- `optimalDistance = clamp(optimalDistance, nearLimit, farLimit)`

The CineRig extension must not treat a null physics adapter or a zero layer
mask as a validation error. It simply performs no occlusion query.

## API Sketch

```cpp
namespace cinerig
{

class IShotQualityEvaluator
{
public:
    virtual ~IShotQualityEvaluator() = default;
};

class ShotQualityEvaluator : public CameraExtension, public IShotQualityEvaluator
{
public:
    struct DistanceEvaluationSettings
    {
        bool enabled = false;
        float optimalDistance = 10.0f;
        float nearLimit = 5.0f;
        float farLimit = 30.0f;
        float maxQualityBoost = 0.2f;
    };

    LayerMask occlusionLayers{1u};
    EntityId ignoreEntity = 0;
    float minimumDistanceFromTarget = 0.2f;
    float cameraRadius = 0.0f;
    DistanceEvaluationSettings distanceEvaluation{};

    void reset();
    void validate();

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    bool isTargetObscured(const CameraState& state, const IPhysicsWorld& physics) const;
};

} // namespace cinerig
```

The implementation may keep `isTargetObscured` private; the public API should
stay limited to configuration, validation, and the extension callback.

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineShotQualityEvaluator.cs`
  - Finalize stage, offscreen check, 0.2 occlusion penalty, distance boost,
    validation clamps, and default values.
- `com.unity.cinemachine/Runtime/Core/IShotQualityEvaluator.cs`
  - Marker interface contract.
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineDeoccluder.cs`
  - Existing quality evaluation producer that must also implement the marker.
- `CineRig/include/cinerig/core/camera_state.hpp`
  - `shotQuality`, `correctedPosition()`, `finalPosition()`, `hasLookAt()`, and
    `isTargetOffscreen()`.
- `CineRig/include/cinerig/engine/physics_world.hpp`
  - `sphereCast` and `QueryFilter`.
- `CineRig/src/managers/clear_shot.cpp`
  - Current consumer of `state.shotQuality`.

## Acceptance

- Defaults and validation match the reference values.
- A state without a look-at point is unchanged.
- An offscreen target multiplies shot quality by `0.2`.
- A spherecast hit before the shortened target distance multiplies shot quality
  by `0.2`; a clear cast does not.
- A target closer than `minimumDistanceFromTarget` is treated as obscured.
- Distance evaluation is disabled by default; when enabled, the near, optimal,
  and far branches match the reference formula.
- A null physics adapter or an empty layer mask does not invalidate the
  extension and does not query physics.
- ClearShot selects the quality-evaluated child correctly in a scenario test.
- The `32_clear_shot` example uses the evaluator without double quality
  penalties.
- Existing core, parity, example, install, and no-tests checks pass on Windows.

## Implementation Record

- Added `core::IShotQualityEvaluator` in `include/cinerig/core/shot_quality.hpp`.
- Added `components::ShotQualityEvaluator` in
  `include/cinerig/components/shot_quality_evaluator.hpp` and
  `src/components/shot_quality_evaluator.cpp`.
- Made `Deoccluder` implement the marker interface without changing its existing
  quality behavior.
- Exported both headers through `cinerig.hpp` and added the source to the
  `cinerig` target.
- Implemented the reference defaults, validation clamps, Finalize-stage
  offscreen check, spherecast occlusion penalty, and distance-quality branches.
  A zero-width near/optimal range resolves to the maximum boost instead of
  dividing by zero.
- Added `tests/unit/shot_quality_evaluator_test.cpp` and
  `tests/scenario/clear_shot_quality_test.cpp`.
- Updated `examples/32_clear_shot` to use `ShotQualityEvaluator` and keep
  Deoccluder quality disabled, avoiding a double penalty.

## Verification

- `cinerig_unit_tests "[shot-quality]"`: 8 test cases, 46 assertions passed.
- `cinerig_scenario_tests "[shot-quality]"`: 1 test case, 7 assertions passed.
- Full Debug `cinerig_unit_tests`: 332 test cases, 3029 assertions passed.
- Full Debug `cinerig_scenario_tests`: 141 test cases, 1154 assertions passed.
- Debug core CTest: 503/503 passed, including 6/6 performance cases.
- Release core CTest: 503/503 passed, including 6/6 performance cases.
- Debug examples CTest: 556/556 passed, including
  `example_clear_shot_smoke`.
- Release examples CTest: 556/556 passed, including
  `example_clear_shot_smoke`.
- Installed `find_package(CineRig)` consumer compiled, linked, and exited 0.
- `CINERIG_BUILD_TESTS=OFF` Release build passed.
- AddressSanitizer core CTest: 497/497 passed, performance excluded.
