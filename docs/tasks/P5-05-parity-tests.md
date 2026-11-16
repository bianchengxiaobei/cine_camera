# P5-05 Parity Tests

Status: completed  
Depends on: P1-03, P2-01, P3-04, P5-01, P5-02, P5-03, P5-04  
Blocks: P5-06

## Goal

Add a dedicated parity suite that transcribes representative expected behavior
from the Unity Cinemachine tests into engine-neutral CineRig tests. The suite is
not a Unity test runner and does not require the Unity editor. It makes the
reference-to-CineRig behavioral mapping explicit and guards public API behavior
that was previously covered only indirectly by feature-specific tests.

## Scope

- Add a `tests/parity` Catch2 target with the `parity` CTest label.
- Port reference expectations for:
  - `CameraState::lerp` when either side asks to ignore the look-at target,
    including the reference look-at and raw-orientation result.
  - Standard damping monotonicity and vector damping over the reference
    frame-time grid.
  - `DefaultInputAxisDriver` exact response values and acceleration/deceleration
    timing from `InputAxisTests`.
  - Brain priority ordering, including integer limit values, activation order,
    disabling, and re-enabling.
  - Blend-manager event sequences for interrupted/nested blends, blend reversal,
    cancellation while the outgoing camera keeps updating, and snapshot reset
    after an interrupted freeze blend.
  - Final camera positions for the core body components that the reference
    `CameraPositionTests` covers, using the CineRig coordinate convention.
- Add `tests/parity/README.md` describing the mapping and intentional
  differences from the reference fixtures.
- Add `docs/parity.md` as the durable reference-test inventory and gap list.
- Wire the target into CMake and update the task index, feature matrix, and
  architecture/testing notes.

## Non-goals

- Running Unity, NUnit, the Unity Test Framework, or editor fixtures.
- Experimental `Cinemachine.EXPERIMENTAL_DAMPING` / `StableDamp` and its global
  average-frame-rate tracker; the default `Damp` path in the reference uses
  `StandardDamp`.
- Editor-only tests, inspector tests, prefab/scene tests, SaveDuringPlay, and
  Unity input/package integration.
- Performance benchmarks and frame-time budgets; those belong to P5-06.
- Replacing the existing unit, scenario, or golden suites. The parity suite uses
  public-facing integration paths where possible and complements them.

## Design

```text
tests/parity/
  camera_state_parity_test.cpp
  damping_parity_test.cpp
  input_axis_parity_test.cpp
  priority_parity_test.cpp
  blend_manager_parity_test.cpp
  camera_position_parity_test.cpp
  README.md

ctest -L parity
```

Reference tests are adapted in three ways:

1. Unity's `+Z` forward convention is translated to CineRig's `-Z` forward
   convention.
2. Unity coroutine frame stepping is replaced with explicit deterministic
   `BrainTestRig::step(deltaTime)` calls or direct `CameraSystem` updates.
3. Unity editor/runtime fixture state is replaced by the existing CineRig test
   support (`FakeEntityWorld`, `FakeCameraOutput`, `BrainTestRig`).

Tests should assert reference values, not merely re-implement the CineRig
algorithm in the expectation. Where the reference test depends on a Unity-only
facility, the gap is recorded in `docs/parity.md` instead of being faked.

## Reference

- `com.unity.cinemachine/Tests/Editor/CameraStateTests.cs`
  - `CameraState_IgnoreLookAtTarget_In_Lerp_Provides_ExpectedReferenceLookAt`
  - `CameraState_IgnoreLookAtTarget_In_Lerp_Provides_ExpectedRawOrientation`
- `com.unity.cinemachine/Tests/Editor/DampingTests.cs`
  - `StandardDampFloat`
  - `DampVector`
- `com.unity.cinemachine/Tests/Editor/InputAxisTests.cs`
  - `TestAxisDriverAccel`
  - `TestInputAxis`
- `com.unity.cinemachine/Tests/Editor/BlendManagerTests.cs`
  - `TestEventsNestedBlend`
  - `TestBlendReversal`
  - `TestBlendCancellationKeepsOutgoingCameraUpdating`
- `com.unity.cinemachine/Tests/Runtime/CamerasBlendingTests.cs`
  - `BlendClearsSnapshotWhenBlendingIsInterrupted`
- `com.unity.cinemachine/Tests/Runtime/PriorityTests.cs`
  - `CheckPriorityOrder`
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
  - `DoNothing`, `PositionComposer`, `HardLockToTarget`, `OrbitalFollow`,
    `Follow`
- `com.unity.cinemachine/Tests/Runtime/LookaheadTests.cs`
  - predictor motion/marker behavior is covered by the existing composer tests;
    Unity's public target-changed flags have no CineRig equivalent.
- `com.unity.cinemachine/Tests/Runtime/ManualUpdateTests.cs`
  - frame-gated manual updates are covered by
    `camera_system_update_clock_test.cpp`.
- `com.unity.cinemachine/Runtime/Core/Predictor.cs`
  - standard damping formulas and experimental stable damping.
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - look-at and orientation blend rules.

## Acceptance

- The parity target builds and runs on Windows Debug and Release.
- `ctest -L parity` passes.
- The full unit, scenario, golden, examples, and parity suites pass.
- Reference values for camera-state look-at blending, input-axis response,
  priority ordering, blend reversal/event counts, and camera final positions are
  asserted.
- Intentional differences and untested Unity-only behavior are documented in
  `docs/parity.md`.
- No core library dependency on SDL, OpenGL, Unity, or editor facilities is
  introduced.

## Implementation Record

### Implemented

- `tests/parity` Catch2 target with the `parity` CTest label:
  - `camera_state_parity_test.cpp`
  - `damping_parity_test.cpp`
  - `input_axis_parity_test.cpp`
  - `priority_parity_test.cpp`
  - `blend_manager_parity_test.cpp`
  - `camera_position_parity_test.cpp`
- `tests/parity/README.md` with the reference mapping, adaptations, and known
  gaps.
- `docs/parity.md` with the durable Unity-test inventory and parity status.
- Blend-stack parity fixes:
  - completed root blends clear `source` and the cached freeze snapshot;
  - outgoing live cameras are refreshed each frame while the active incoming
    camera is left to `CameraSystem`, transition callbacks, and the activation
    update path.
- CMake wiring for the new target and task index, feature matrix, architecture,
  and README testing notes.

### Verification

- `[parity]`: **18 cases / 229 assertions passed** in Debug and Release.
- Core Debug and Release: **465/465 passed** (319 unit + 140 scenario + 6
  golden; 4114 assertions). `ctest` includes the parity target, so the full
  core run is **483/483**.
- Examples Debug and Release: **536/536 passed** (465 core + 18 parity + 53
  example/smoke tests, including 38 smoke tests).
- Core no-tests and examples-no-tests Debug builds pass.
- Install exports the updated `brain/camera_blend_stack.hpp` and library. The
  external `find_package(CineRig)` consumer rebuilds and exits 0.
- The `[parity]` cases cover:
  - look-at hint matrix and raw-orientation slerp;
  - standard/vector damping on the reference frame-time grid;
  - exact input-axis response and accel/decel timing;
  - standard and integer-limit priority ordering;
  - nested blend event counts, reversal positions, cancelled-blend outgoing
    updates, and freeze-snapshot reset;
  - final positions for no-body, Follow, HardLockToTarget, OrbitalFollow,
    PositionComposer, and zero-rig ThirdPersonFollow.
- Windows Debug/Release verified; Linux remains untested.
