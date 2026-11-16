# P3-08 Confiner3D

Status: completed  
Depends on: P1-02, P3-06, P3-07  
Blocks: P3 completion, host volume integration

## Goal

Port `CinemachineConfiner3D`. The Confiner3D camera extension snaps the camera
inside a host-supplied bounding volume and optionally slows camera motion near
the volume edge. It exposes whether the camera was displaced and by how much.

## Scope

- Add `IPhysicsWorld::closestPoint` with a bounds-based default implementation
  and an exact box/sphere implementation in `SdlPhysics`.
- Add `components::Confiner3D` deriving from `CameraExtension`:
  - bounding volume entity
  - validity check
  - closest-point confinement at the `Body` stage
  - slowing distance with the original cubic easing
  - per-camera previous position/displacement state
  - displacement queries
  - follow-target warp state adjustment
- Add unit tests for defaults/validation, inside/outside points, missing volume,
  slowing, previous-state behavior, displacement reporting, and warp handling.
- Add a `23_confiner3d` SDL example with a visible bounding volume, moving
  target, slowing controls, displacement HUD, and smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity collider enabled/active hierarchy checks. `boundingVolume != 0` plus a
  successful physics closest-point query defines validity.
- Non-convex mesh-collider pass-through behavior. A host adapter can return the
  query point unchanged to emulate it.
- Confiner2D, editor gizmos, serialization, or SaveDuringPlay.

## Design

```text
Body stage
  if no bounding entity or no closest-point result: do nothing
  newPosition = physics.closestPoint(volume, correctedPosition)
  if slowing enabled and previous state valid:
    measure distance to edge along previous->new direction
    lerp from previous position with clamp(t^3 + 0.05)
  apply position correction
  cache corrected position and displacement per camera
```

`IPhysicsWorld::closestPoint` is optional for adapters; the default clamps to
the entity bounds. `SdlPhysics` provides exact box and sphere closest-point
behavior. This keeps the Confiner3D independent of a concrete collider type.

## API Sketch

```cpp
class Confiner3D : public CameraExtension
{
public:
    EntityId boundingVolume = 0;
    float slowingDistance = 0.0f;

    void reset();
    void validate();
    bool isValid() const noexcept;
    bool cameraWasDisplaced(CameraId cameraId) const;
    float cameraDisplacementDistance(CameraId cameraId) const;

    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
    float maxDampTime() const override;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineConfiner3D.cs`
  - bounding-volume closest point, slowing distance/easing, displacement state,
    validity, warp handling, and defaults.

## Acceptance

- Defaults match the original reset values.
- Points inside the volume are unchanged; points outside move to the closest
  volume point.
- Missing/invalid volume produces no correction.
- Slowing distance reduces motion near the edge using `t^3 + 0.05` with clamped
  interpolation; first frames without a valid previous state snap directly.
- Camera displacement queries reflect the last correction.
- Target warps adjust cached camera position for the followed target.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `IPhysicsWorld::closestPoint` with a bounds-clamping default and exact box and
  sphere behavior in `SdlPhysics`.
- `include/cinerig/components/confiner3d.hpp` and
  `src/components/confiner3d.cpp`
  - bounding volume entity and validity check
  - closest-point confinement at the `Body` stage
  - slowing distance with the original `t^3 + 0.05` clamped interpolation
  - per-`CameraId` previous position and displacement state
  - displacement queries and maximum damping time
  - followed-target warp adjustment for cached camera positions
  - reset and validation
- `CMakeLists.txt` and `include/cinerig/cinerig.hpp` export/build wiring.
- `tests/unit/confiner3d_test.cpp`
  - 7 cases and 23 assertions covering defaults/validation, containment,
    clamping, missing volumes, slowing, invalid previous state, and warp handling.
- `examples/23_confiner3d`
  - moving target, selectable box/sphere/no volume, slowing control, raw and
    confined sight lines, displacement HUD, and smoke test.
- `examples/tests/input_physics_test.cpp` now covers exact box/sphere
  closest-point results.
- `examples/README.md`, task index, feature matrix, and architecture updated.

### Differences

- Volume validity is `boundingVolume != 0` plus a successful closest-point query;
  host entity active/enabled state is the adapter's responsibility.
- Non-convex mesh-collider pass-through is not modeled; a host adapter can
  return the input point to reproduce it.
- The default closest-point implementation clamps to entity bounds; adapters
  with exact shapes override it.
- `onTargetObjectWarped` adjusts cached states when the extension owner's follow
  target matches, because the CineRig extension callback does not receive the
  evaluating virtual camera.
- State is stored per `CameraId`; editor gizmos, serialization, and SaveDuringPlay
  are not ported.

### Verification

- Core Debug: **337/337 passed** (256 unit + 75 scenario + 6 golden).
- Core Release: **337/337 passed**.
- Examples Debug: **375/375 passed** (337 core + 38 example tests, including 23
  smoke tests).
- Examples Release: **375/375 passed**.
- `cinerig_23_confiner3d --frames 180 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 23 example exits 0.
- Visible window shows box confinement (`raw x 5.994 -> corrected x 4.992`),
  slowing, displacement `0.941`, and the selected volume wireframe. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_confiner3d.png`.
- Install includes `cinerig/components/confiner3d.hpp`.
- Linux was not verified on hardware.


