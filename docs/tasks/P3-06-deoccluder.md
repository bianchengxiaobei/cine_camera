# P3-06 Deoccluder

Status: completed  
Depends on: P1-02, P3-04, P3-05  
Blocks: decollider/confiner integration, shot-quality-driven camera selection

## Goal

Port the 3D `CinemachineDeoccluder` camera extension. It preserves the line of
sight to the look-at or follow target by pulling the camera in front of the
nearest obstacle, optionally pushing it along the obstacle to restore height or
distance, then damping/smoothing the correction. It also reports occlusion and
displacement state and can evaluate shot quality.

## Scope

- Add `components::Deoccluder` deriving from `CameraExtension`.
- Resolve at the `Body` stage and restore the original screen-space look-at
  composition after moving the camera.
- Support:
  - collision-layer filtering and transparent layers
  - CineRig-native ignored entity filtering
  - minimum distance from target
  - camera radius and distance limit
  - `PullCameraForward`, `PreserveCameraHeight`, and
    `PreserveCameraDistance` strategies
  - maximum effort and bounds-clamped wall walking
  - occlusion start delay and distance smoothing
  - damping when entering and leaving occlusion
  - optional follow-target resolution point with local up offset
- Report target-obscured and camera-displacement state per virtual camera.
- Evaluate shot quality at the `Finalize` stage:
  - target offscreen or obscured penalty
  - displacement penalty
  - near/far/optimal-distance boost
- Add unit tests for defaults, filters, pull-forward, strategies, follow target,
  distance limit, minimum distance, state reporting, and shot quality.
- Add a `21_deoccluder` SDL example with moving occluders, strategy/filter
  controls, correction HUD, and a smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity tag strings. CineRig physics queries expose stable entity ids, so
  `ignoreEntity` replaces `IgnoreTag`; hosts can ignore target colliders by id.
- Editor debug paths, collider-type-specific corner normals, and
  `Physics.ComputePenetration` parity. Wall walking uses physics bounds and
  overlap data available through `IPhysicsWorld`.
- Shared MonoBehaviour extension state, serialization, or SaveDuringPlay.
- Decollider and Confiner3D; they remain separate tasks.

## Design

The extension keeps one `ExtraState` per `CameraId` because a parent camera's
extension can be invoked while evaluating child cameras. All time comes from
`VirtualCamera::currentTime()`, and the extension uses the existing
`IPhysicsWorld::sphereCast`, `overlapSphere`, and `bounds` queries.

```text
Body stage
  capture initial corrected position and look-at composition
  rotate previous displacement by rotationDampingBypass
  resolve line of sight
    sphere cast from target toward camera
    pull in front of nearest hit
    optionally walk along obstacle / restore distance or height
    respect camera radius and minimum target distance
  apply occlusion delay and distance smoothing
  damp displacement
  apply positionCorrection
  restore raw orientation for original look-at screen offset
  update rotationDampingBypass and cached state

Finalize stage
  evaluate target offscreen/obscured and displaced penalties
  apply near/far distance quality boost
```

Layer masks are intersected for line-of-sight tests. Transparent layers are
removed from the blocking mask, while full collision layers are used for the
camera-radius overlap step, matching the original. A null physics world, zero
mask, missing target, or missing look-at point produces no correction.

## API Sketch

```cpp
class Deoccluder : public CameraExtension
{
public:
    struct FollowTargetSettings
    {
        bool enabled = false;
        float yOffset = 0.0f;
    };

    enum class ResolutionStrategy
    {
        PullCameraForward,
        PreserveCameraHeight,
        PreserveCameraDistance,
    };

    struct ObstacleAvoidance
    {
        bool enabled = true;
        float distanceLimit = 0.0f;
        float minimumOcclusionTime = 0.0f;
        float cameraRadius = 0.4f;
        FollowTargetSettings useFollowTarget{};
        ResolutionStrategy strategy = ResolutionStrategy::PullCameraForward;
        int maximumEffort = 4;
        float smoothingTime = 0.0f;
        float damping = 0.4f;
        float dampingWhenOccluded = 0.2f;
    };

    struct QualityEvaluation
    {
        bool enabled = false;
        float optimalDistance = 10.0f;
        float nearLimit = 5.0f;
        float farLimit = 30.0f;
        float maxQualityBoost = 0.2f;
    };

    LayerMask collideAgainst{1u};
    EntityId ignoreEntity = 0;
    LayerMask transparentLayers{0u};
    float minimumDistanceFromTarget = 0.3f;
    ObstacleAvoidance avoidObstacles{};
    QualityEvaluation qualityEvaluation{};

    void reset();
    void validate();

    bool isTargetObscured(CameraId cameraId) const;
    bool cameraWasDisplaced(CameraId cameraId) const;
    float cameraDisplacementDistance(CameraId cameraId) const;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
    float maxDampTime() const override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineDeoccluder.cs`
  - obstacle avoidance settings, line-of-sight resolution, wall walking,
    camera-radius handling, occlusion timing/smoothing, damping, composition
    restoration, displacement state, and shot-quality evaluation.

## Acceptance

- Defaults match the original reset values.
- Disabled avoidance, zero collision mask, no physics, or no target produce no
  correction.
- Transparent layers are excluded from blocking casts.
- Pull-forward moves the camera in front of the nearest obstacle while keeping
  the minimum distance from the target.
- Non-pull strategies walk along obstacle bounds with clamped effort.
- Follow-target resolution uses the follow target's local up offset.
- Corrections are damped, occlusion delay and distance smoothing behave, and
  the look-at screen composition is restored.
- Obscured/displaced state queries and shot-quality penalties/boosts behave.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/deoccluder.hpp` and
  `src/components/deoccluder.cpp`
  - `Deoccluder` `CameraExtension` with original avoidance/quality defaults
  - per-`CameraId` extra state for parent/child camera evaluation
  - collision mask and transparent-layer subtractive filtering
  - CineRig-native ignored entity filtering
  - minimum target distance, camera radius, distance limit, and validation
  - pull-forward sphere-cast resolution
  - preserve-height and preserve-distance wall walking with bounds clamping,
    nearby-obstacle corner normals, maximum effort, and line-of-sight checks
  - camera-radius overlap resolution with bounds-based push-out fallback
  - occlusion start delay, nearest-distance smoothing, and enter/leave damping
  - look-at screen-composition restoration and rotation-damping bypass updates
  - follow-target resolution point with target-local up offset
  - obscured/displaced state queries and per-camera displacement distance
  - Finalize-stage shot quality: obscured/offscreen and displaced penalties plus
    near/optimal/far distance boost
- `CMakeLists.txt` and `include/cinerig/cinerig.hpp` export/build wiring.
- `tests/unit/deoccluder_test.cpp`
  - 10 cases and 74 assertions covering defaults/validation, pull-forward,
    transparent layers, ignored entity, distance limit, minimum distance,
    disabled/missing data, follow-target offset, push-around resolution, damping,
    occlusion delay, distance smoothing, camera-radius overlap, obscuration state,
    and shot-quality penalties.
- `examples/21_deoccluder`
  - ThirdPersonFollow camera, sliding blocker, transparent glass panel,
    strategy/filter/damping/quality controls, raw/corrected sight lines,
    correction/occlusion/shot-quality HUD, and a smoke test.
- `examples/README.md`, task index, feature matrix, and architecture updated.

### Differences

- Unity `IgnoreTag` is represented by `ignoreEntity`, because CineRig physics
  queries expose stable entity ids rather than engine tag strings.
- Collider-type-specific corner detection is replaced by `overlapSphere` plus
  physics bounds. This keeps the component engine-neutral.
- `Physics.ComputePenetration` is approximated by pushing the camera away from
  overlap points (or bounds centers when the camera is fully inside).
- The original transparent-obstacle special case for detecting a camera fully
  inside an obstacle when the overlap query is empty is not ported.
- Editor-only debug paths and occluding-collider lists are not retained.
- Per-camera state is stored in a map keyed by `CameraId`; no shared MonoBehaviour
  extension cache exists.

### Verification

- Core Debug: **321/321 passed** (240 unit + 75 scenario + 6 golden).
- Core Release: **321/321 passed**.
- Examples Debug: **357/357 passed** (321 core + 36 example tests, including 21
  smoke tests).
- Examples Release: **357/357 passed**.
- `cinerig_21_deoccluder --frames 180 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 21 example exits 0.
- Visible window shows pull-forward correction (`camera 8 -> 3.2`), displaced
  state, clear line of sight after resolution, and shot quality near 0.806.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_deoccluder.png`.
- Install includes `cinerig/components/deoccluder.hpp`.
- Linux was not verified on hardware.

