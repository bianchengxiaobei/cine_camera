# Parity Tests

The parity target transcribes representative expectations from the Unity
Cinemachine test suite into deterministic CineRig tests. It runs without Unity,
NUnit, or editor fixtures.

```powershell
ctest --test-dir build\tests -C Debug -L parity --output-on-failure
```

The target currently contains 18 Catch2 test cases and 229 assertions.

## Reference Mapping

| Reference test | CineRig case | Reference behavior |
| --- | --- | --- |
| `Tests/Editor/CameraStateTests.cs` | `camera_state_parity_test.cpp` | Ignore-look-at hint combinations, reference look-at, and slerped raw orientation |
| `Tests/Editor/DampingTests.cs` | `damping_parity_test.cpp` | Standard damp monotonicity and vector damping over the 10-144 FPS grid |
| `Tests/Editor/InputAxisTests.cs` | `input_axis_parity_test.cpp` | Exact axis response values for 0.1/0.5/1/100 input and accel/decel timing |
| `Tests/Editor/BlendManagerTests.cs` | `blend_manager_parity_test.cpp` | Nested blend event counts, reversal positions, and cancelled-blend updates |
| `Tests/Runtime/CamerasBlendingTests.cs` | `blend_manager_parity_test.cpp` | Freeze snapshot reset after an interrupted blend |
| `Tests/Runtime/PriorityTests.cs` | `priority_parity_test.cpp` | Standard and integer-limit priority ordering, disable, and re-enable |
| `Tests/Runtime/CameraPositionTests.cs` | `camera_position_parity_test.cpp` | Body-component final positions: none, Follow, HardLock, Orbital, PositionComposer, ThirdPerson |

## Adaptations

- Unity's `+Z` forward convention is translated to CineRig's `-Z` forward
  convention.
- Coroutine frame stepping is replaced by deterministic
  `BrainTestRig::step(deltaTime)` calls.
- Unity `FakeCamera.State` reads its live position field directly. CineRig
  `VirtualCamera` caches state, so parity tests refresh any manually moved
  camera state before the next deterministic frame when the reference fixture
  depends on that behavior.
- The ThirdPersonFollow reference assertion uses the default shoulder rig. The
  CineRig parity case zeroes the shoulder offset, arm length, and camera
  distance so the reference assertion maps to CineRig's rig parameters without
  changing the component's defaults.
- The blend-stack fix updates outgoing live cameras once per frame while leaving
  the active incoming camera to `CameraSystem`, transition callbacks, and the
  activation update path. This avoids the reference
  `CinemachineVirtualCameraBase.UpdateCameraState` double-update behavior
  without relying on a Unity global frame guard.

## Gaps

- `CINEMACHINE_EXPERIMENTAL_DAMPING` and `AverageFrameRateTracker.StableDamp`
  are intentionally not ported. The reference default `Damper.Damp` path uses
  `StandardDamp`.
- Unity's public `FollowTargetChanged` / `LookAtTargetChanged` flags have no
  CineRig equivalent. Predictor motion and clear-on-target-change behavior are
  covered through the composer tests.
- Unity editor-only tests, static reset/event registration, prefab/scene tests,
  and `ControlledObject` multi-brain synchronization are not applicable.
- `CameraPositionTests.ThirdPersonFollow` is represented with a zero rig as
  described above rather than asserting the reference default-rig position.
