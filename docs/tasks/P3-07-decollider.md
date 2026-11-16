# P3-07 Decollider

Status: completed  
Depends on: P1-02, P3-06  
Blocks: confiner integration, complete obstacle-response stack

## Goal

Port the 3D `CinemachineDecollider` camera extension. It pushes the camera out
of intersecting obstacles, lifts it above terrain, smooths/damps the correction,
restores the original look-at composition, and reports no separate shot state.

## Scope

- Add `components::Decollider` deriving from `CameraExtension`.
- Resolve at the `Body` stage.
- Support:
  - camera radius
  - terrain sphere-cast resolution along world up
  - decollision against obstacle layers
  - obstacle layers excluding terrain layers when terrain resolution is enabled
  - optional follow-target resolution point with local up offset
  - distance smoothing and damping while returning from collision
  - second terrain pass after decollision
- Preserve the raw look-at screen composition after moving the camera and
  update `rotationDampingBypass`.
- Add unit tests for defaults, terrain resolution/damping, layer exclusion,
  overlap decollision, smoothing, damping, follow target, and composition.
- Add a `22_decollider` SDL example with a moving target, raised terrain, and
  moving blocking volume plus controls/HUD/smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Capsule overlap and true collider penetration. `IPhysicsWorld` exposes sphere
  overlap and bounds, so the port uses camera-sphere overlap plus bounds entry
  distance; this is documented as an approximation.
- Unity tag strings, editor debug buffers, scratch colliders, or serialization.
- Replacing Deoccluder. Decollider only handles camera intersection and terrain;
  Deoccluder remains the line-of-sight solution.
- Confiner3D; it remains the next task.

## Design

```text
Body stage
  capture initial corrected position and look-at screen offset
  terrain pass: sphere cast down from camera + maxRaycast * up
  apply upward terrain displacement
  if decollision enabled:
    overlap camera sphere with obstacle layers minus terrain layers
    derive each obstacle's entry distance from expanded bounds
    move camera in front of the nearest valid overlap
    smooth/damp target distance
    resolve terrain again without damping
  restore screen composition and rotationDampingBypass
```

The extension stores state per `CameraId`. Time comes from
`VirtualCamera::currentTime()`. A missing physics world, zero obstacle mask, or
zero target-to-camera distance produces no correction.

## API Sketch

```cpp
class Decollider : public CameraExtension
{
public:
    struct FollowTargetSettings
    {
        bool enabled = false;
        float yOffset = 0.0f;
    };

    struct DecollisionSettings
    {
        bool enabled = false;
        LayerMask obstacleLayers{1u};
        FollowTargetSettings useFollowTarget{};
        float damping = 0.5f;
        float smoothingTime = 0.0f;
    };

    struct TerrainSettings
    {
        bool enabled = true;
        LayerMask terrainLayers{1u};
        float maximumRaycast = 10.0f;
        float damping = 0.5f;
    };

    float cameraRadius = 0.4f;
    DecollisionSettings decollision{};
    TerrainSettings terrainResolution{};

    void reset();
    void validate();
    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
    float maxDampTime() const override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineDecollider.cs`
  - camera radius, terrain resolution, capsule broadphase, overlap detection,
    entry-distance sorting, smoothing/damping, composition restoration, and
    defaults.

## Acceptance

- Defaults match the original reset values.
- Terrain sphere casts move the camera above terrain, with damping when the
  terrain drops away and no damping on the first frame.
- Decollision removes camera overlap with obstacles and leaves non-overlapping
  cameras untouched.
- Terrain layers are excluded from decollision when terrain resolution is on.
- Smoothing holds the nearest distance while active, then releases.
- Damping slows the return to the original distance.
- Follow-target resolution uses target-local up offset.
- Composition is preserved after camera movement.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/decollider.hpp` and
  `src/components/decollider.cpp`
  - `Decollider` `CameraExtension` with original camera-radius, terrain, and
    decollision defaults
  - per-`CameraId` extra state
  - terrain resolution via downward sphere cast, with terrain-drop damping
  - camera-sphere overlap against obstacle layers
  - terrain-layer subtraction when terrain resolution is enabled
  - expanded-bounds entry distances and nearest-overlap camera push-out
  - fallback to per-entity raycast when an obstacle has no bounds
  - optional follow-target resolution point with target-local up offset
  - nearest-distance smoothing and return damping
  - second terrain pass after decollision
  - look-at screen-composition restoration and rotation-damping bypass updates
  - validation for radius, damping, smoothing, and raycast length
- `CMakeLists.txt` and `include/cinerig/cinerig.hpp` export/build wiring.
- `tests/unit/decollider_test.cpp`
  - 9 cases and 50 assertions covering defaults/validation, terrain lift and
    damping, layer subtraction, overlap push-out, clear cameras, smoothing,
    return damping, follow-target offset, and screen composition.
- `examples/22_decollider`
  - ThirdPersonFollow camera over an oscillating terrain platform and a sweeping
    obstacle, terrain/decollision/damping/smoothing controls, raw/corrected
    sight lines, correction HUD, and a smoke test.
- `examples/README.md`, task index, feature matrix, and architecture updated.

### Differences

- Unity capsule overlap is approximated with a camera-sphere overlap query;
  only obstacles touching the camera are candidates.
- Collider penetration is replaced by expanded-AABB entry distance and
  nearest-overlap push-out. A per-entity raycast is used when bounds are absent.
- `Physics.ComputePenetration`, Unity tag strings, static scratch buffers, and
  editor debug data are not ported.
- Per-camera state is stored in a map keyed by `CameraId`.
- The previous target distance is updated even when the camera state is invalid,
  avoiding Unity's temporary `float.MaxValue` sentinel.

### Verification

- Core Debug: **330/330 passed** (249 unit + 75 scenario + 6 golden).
- Core Release: **330/330 passed**.
- Examples Debug: **367/367 passed** (330 core + 37 example tests, including 22
  smoke tests).
- Examples Release: **367/367 passed**.
- `cinerig_22_decollider --frames 180 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 22 example exits 0.
- Visible window shows terrain lift (`raw y 1.6 -> corrected y 2.277`) and,
  during obstacle overlap, decollision (`z 8 -> 6.8`) with combined correction.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_decollider.png`.
- Install includes `cinerig/components/decollider.hpp`.
- Linux was not verified on hardware.


