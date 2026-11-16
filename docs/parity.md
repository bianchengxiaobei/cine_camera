# Cinemachine Parity Inventory

This document tracks how the Unity Cinemachine 3.1.x test suite maps to
CineRig. The parity suite is intentionally separate from the feature unit and
scenario suites: it ports representative expected values and event sequences
from the reference tests so that behavioral drift is visible.

Run the suite with:

```powershell
ctest --test-dir build\tests -C Debug -L parity --output-on-failure
```

## Parity Suite

The `tests/parity` target currently has 18 test cases and 229 assertions.

| Reference test | Parity coverage | Status |
| --- | --- | --- |
| `Tests/Editor/CameraStateTests.cs` | `CameraState` ignore-look-at combination matrix, reference look-at, raw-orientation slerp | Ported |
| `Tests/Editor/DampingTests.cs` | `StandardDamp` monotonicity, vector damping on the reference FPS grid | Ported for the default standard path |
| `Tests/Editor/InputAxisTests.cs` | Exact `DefaultInputAxisDriver` response values and accel/decel timing | Ported |
| `Tests/Editor/BlendManagerTests.cs` | Nested event counts, blend reversal positions, cancelled-blend outgoing updates | Ported |
| `Tests/Editor/BlendManagerTests.cs` | Active-blend setter tests | Not applicable; CineRig does not expose a writable active blend |
| `Tests/Runtime/CamerasBlendingTests.cs` | Interrupted freeze snapshot reset before restarting a blend | Ported |
| `Tests/Runtime/CamerasBlendingTests.cs` | Basic blend duration/progress and nested blend interruption | Covered by scenario and parity tests |
| `Tests/Runtime/PriorityTests.cs` | Standard and integer-limit ordering, disable/re-enable | Ported |
| `Tests/Runtime/CameraPositionTests.cs` | No body, Follow, HardLock, OrbitalFollow, PositionComposer, ThirdPersonFollow final positions | Ported with the coordinate and rig adaptations below |
| `Tests/Runtime/LookaheadTests.cs` | Predictor movement and lookahead behavior | Covered by `position_composer_test.cpp` and `rotation_composer_test.cpp` |
| `Tests/Runtime/LookaheadTests.cs` | `FollowTargetChanged` / `LookAtTargetChanged` flags | No public CineRig equivalent |
| `Tests/Runtime/ManualUpdateTests.cs` | Frame-gated manual updates | Covered by `camera_system_update_clock_test.cpp` |
| `Tests/Runtime/BrainTargetOverrideTests.cs` | Multiple Unity brain objects driving camera/empty GameObjects | Adapter concern; CineRig has one `ICameraOutput` per brain |
| `Tests/Runtime/CinemachineEventRegistrationTests.cs` | Unity module-load event registration and static reset | Not applicable; no domain reload or Unity module lifecycle |
| `Tests/Runtime/ClearShotTests.cs` | Shot quality, activation delay, minimum duration | Covered by `clear_shot_test.cpp` |
| `Tests/Runtime/CmDeoccluderTests.cs` | Line-of-sight resolution and camera correction | Covered by `deoccluder_test.cpp` |
| `Tests/Runtime/Confiner2DUnitTests.cs` | Shape cache, shrinking, damping, invalidation | Covered by `confiner2d_test.cpp` and `confiner_oven_test.cpp` |
| `Tests/Runtime/SplineCartTests.cs`, `SplineDollyCameraTest.cs`, `SplineDollyLookatTarget.cs` | Spline evaluation, cart motion, roll, look-at profiles | Covered by the spline unit suite |
| `Tests/Runtime/StateDrivenCameraTests.cs` | State mapping, transition timing, minimum duration | Covered by `state_driven_camera_test.cpp` |
| `Tests/Runtime/CheckPhysicalCameraProperties.cs` | Unity physical camera synchronization | Covered at the lens/adapter boundary |
| `Tests/Runtime/RuntimeUtilityTests.cs` | Unity raycast and collision helpers | Engine adapter behavior; covered by fake physics tests |
| `Tests/Runtime/CameraPositionTests.cs` default ThirdPerson rig assertion | Zero-rig adaptation described below | Adapted |
| `Tests/Editor/SamplesTests.cs`, `UpgradeCm2ToCm3Tests.cs`, editor utility tests | Unity Editor, assets, and prefabs | Excluded |
| `CmCameraProceduralBehaviourCacheTests.cs`, editor cache tests | Editor pipeline cache behavior | Not applicable; CineRig rebuilds state explicitly |

## Behavioral Fixes From Parity Work

The parity cases exposed two blend-stack differences from the reference
`CameraBlendStack`/`CinemachineBlend` behavior:

- Completed root blends now clear the source blend and cached
  `FreezeWhenBlendingOut` snapshot. Without this, restarting an interrupted
  freeze blend reused the stale pre-interruption snapshot.
- The root blend now refreshes outgoing live camera states each frame while
  excluding the active incoming camera. The active camera is already updated by
  `CameraSystem`, its transition callback, and the activation update path, so
  excluding it preserves the reference once-per-frame update guarantee without
  reintroducing double damping.

## Adaptations and Intentional Differences

- CineRig uses right-handed `+Y` up and `-Z` forward. Unity uses `+Z` forward.
  Reference vector expectations are translated where required.
- Unity coroutine frames are replaced by deterministic test-rig steps.
- `VirtualCamera` caches its state instead of reading a transform every time.
  Tests that manually move a camera refresh its state explicitly when the
  reference fixture relies on live position reads.
- The default `CinemachineThirdPersonFollow` shoulder rig is retained; the
  reference position assertion is mapped through an explicitly zeroed rig.
- `CINEMACHINE_EXPERIMENTAL_DAMPING`/`StableDamp`, the global average frame
  rate tracker, and `SetDampTimeScale` are not part of CineRig's default
  damping path.
- CineRig has no Unity editor, prefab, scene, `ControlledObject`, domain-reload,
  or static-reset lifecycle.
- Unity target-change flags are replaced by direct predictor reset and cached
  target comparisons inside the composers.
