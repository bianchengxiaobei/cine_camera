# P2-07 Third Person Follow

Status: completed  
Depends on: P0-04, P1-02, P2-01, P2-04  
Blocks: ThirdPersonAim, third-person gameplay rigs

## Goal

Implement the full `components::ThirdPersonFollow` body component. It positions
the camera behind a target using the original origin/shoulder/hand rig, supports
camera-local damping and an optional obstacle-avoidance sphere cast, and keeps
the target visible in first-person and near-target configurations.

## Scope

- Add `components::ThirdPersonFollow` in the Body stage with:
  - `shoulderOffset`, `verticalArmLength`, `cameraSide`, `cameraDistance`
  - per-axis local `damping` and target attachment handling
  - `getHeading()` and raw origin/shoulder/hand rig placement
  - collision resolution through `engine::IPhysicsWorld::sphereCast`
  - `avoidObstacles` settings: enable, layer filter, ignored entity, radius,
    damping into/from collision
  - target-warp handling and `maxDampTime()`
  - first-person/near-target `ReferenceLookAt` correction and
    `IgnoreLookAtTarget` hint
- Expose `getRigPositions()` for examples and future gizmos.
- Add a `VirtualCamera` physics accessor so body components can perform
  optional collision queries without owning an engine service.
- Add `07_third_person` SDL example with a moving/rotating target, rig
  visualization, collision-obstacle toggle, pause/reset, and HUD.
- Add unit coverage for defaults, rig placement, heading, damping, collision,
  target warping, first-person look-at handling, and validation.
- Update the task index, feature matrix, architecture/coordinate notes, and
  example index.

## Non-goals

- ThirdPersonAim, noise, impulse, TargetGroup, or post-processing.
- A general-purpose decollider/occluder component.
- Unity tags; CineRig uses `EntityId` for ignore filtering.
- Serialization or editor gizmos.

## Design

### Rig

The component sets only `state.rawPosition` and `state.rawOrientation`; it does
not change the camera lens. The original rig is preserved:

```text
root      = target position
heading   = look rotation of the target forward projected onto the up plane
shoulder  = root + heading * (signed shoulder offset + damping correction)
hand      = shoulder + target rotation * (0, verticalArmLength, 0)
camera    = hand - target forward * (cameraDistance - correction.z)
```

`cameraSide` interpolates shoulder X from negative to positive just like the
original. The camera orientation is the target rotation; damping only affects
the effective shoulder offset and camera distance, not the target rotation.
CineRig's `math::Vec3::forward` is `-Z`, so the same formulas preserve the
original user-facing behind-the-target placement.

### Damping

The original `DetachedFollowTargetDamp` behavior is applied to a local-space
correction vector:

```text
correction += inverse(heading) * (previousTargetPosition - targetPosition)
correction -= damp(correction, damping, deltaTime, followTargetAttachment)
```

A negative `deltaTime` or an invalid previous state resets the correction and
collision correction. `maxDampTime()` includes the largest damping axis and the
collision damping values when obstacle avoidance is enabled.

### Collision

When `avoidObstacles.enabled` is set, the component sphere-casts from the root
to the hand (using a slightly enlarged radius), then from the collision-corrected
hand to the camera. Queries use `QueryFilter{layers, ignoreEntity}`. The hit
normal is used to place the target outside the obstacle, and the correction is
damped into/out of collision exactly like the original. A null physics adapter
or an empty layer mask disables the query without invalidating the component.

### First-person and look-at handling

If the computed camera position is within `0.15` units of the current reference
look-at point, the state receives `BlendHint::IgnoreLookAtTarget`. When there is
no look-at point, or the camera is exactly on it, the reference point is moved a
small distance along the target forward so downstream composers have a valid
aim point.

### API Sketch

```cpp
namespace cinerig
{

class ThirdPersonFollow : public CameraComponent
{
public:
    struct ObstacleSettings
    {
        bool enabled = false;
        LayerMask collisionFilter{1u};
        EntityId ignoreEntity = 0;
        float cameraRadius = 0.2f;
        float dampingIntoCollision = 0.0f;
        float dampingFromCollision = 0.5f;
    };

    math::Vec3 damping{0.1f, 0.5f, 0.3f};
    math::Vec3 shoulderOffset{0.5f, -0.4f, 0.0f};
    float verticalArmLength = 0.4f;
    float cameraSide = 1.0f;
    float cameraDistance = 2.0f;
    ObstacleSettings avoidObstacles{};

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    float maxDampTime() const override;

    void validate();
    static math::Quat getHeading(math::Quat targetRotation, math::Vec3 up);
    void getRigPositions(
        math::Vec3& root,
        math::Vec3& shoulder,
        math::Vec3& hand);
    EntityId currentObstacle() const noexcept;

private:
    math::Vec3 resolveCollisions(
        math::Vec3 root,
        math::Vec3 tip,
        float deltaTime,
        float cameraRadius,
        float& collisionCorrection);
    void getRawRigPositions(
        math::Vec3 root,
        math::Quat targetRotation,
        math::Quat heading,
        math::Vec3& shoulder,
        math::Vec3& hand) const;

    math::Vec3 previousFollowTargetPosition_{};
    math::Vec3 dampingCorrection_{};
    float collisionCorrection_ = 0.0f;
    EntityId currentObstacle_ = 0;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineThirdPersonFollow.cs`
  - rig math, damping correction, collision resolution, look-at correction,
    target warping, and rig position API
- `com.unity.cinemachine/Documentation~/CinemachineThirdPersonFollow.md`
  - origin/shoulder/hand/camera semantics and first-person notes
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - `DetachedFollowTargetDamp` target attachment behavior
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
  - ThirdPersonFollow baseline position test

## Acceptance

- Defaults match the original component reset values.
- Rig placement, heading projection, camera side interpolation, and camera
  distance are correct for arbitrary target positions and rotations.
- Damping uses the target attachment and resets correctly on invalid state.
- Optional sphere-cast collision pulls the hand/camera out of obstacles and
  respects layer/ignore filters.
- Target warping and max damping report are correct.
- First-person/near-target look-at correction works.
- Debug/Release builds and the existing test suite pass.

## Implementation Record

To be completed.

## Implementation Record

### Implemented

- `include/cinerig/components/third_person_follow.hpp` and
  `src/components/third_person_follow.cpp`
  - Body-stage rig placement using the original origin/shoulder/hand layout,
    heading projection, camera-side interpolation, and camera distance.
  - Local-space `DetachedFollowTargetDamp` correction with target attachment
    and invalid-state reset.
  - Optional obstacle avoidance using `IPhysicsWorld::sphereCast` for both the
    hand and camera, with layer filtering, ignored entity, radius, and damped
    into/from collision correction.
  - `getRigPositions()`, `getHeading()`, `validate()`, `maxDampTime()`,
    target-warp handling, and current-obstacle reporting.
  - First-person/near-target reference-look-at correction and
    `BlendHint::IgnoreLookAtTarget`.
- `include/cinerig/camera/virtual_camera.hpp` and
  `src/camera/virtual_camera.cpp`
  - Optional `physicsWorld()` accessor returning the system physics adapter or
    null when the virtual camera is detached.
- `tests/unit/third_person_follow_test.cpp`
  - 7 cases and 40 assertions covering defaults, rig placement, yaw/heading,
    damping and negative-time reset, collision filters and damping, first-person
    look-at handling, target warping, validation, and max damping.
- `examples/07_third_person`
  - Moving/rotating target, rig visualization, built-in wall collision, pause,
    collision toggle, reset, HUD, and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the new component and tests.
- Task index, feature matrix, architecture/coordinate notes, and example index
  updated.

### Differences

- CineRig has no Unity tags. Obstacle filtering uses `EntityId ignoreEntity`
  in `QueryFilter` instead of `IgnoreTag`.
- Collision queries are optional and route through the system
  `IPhysicsWorld`; a null adapter disables correction without invalidating the
  rig.
- `getRigPositions()` is public for examples/tests; there are no editor gizmos.
- `forceCameraPosition()` is not overridden, matching the original component's
  lack of force-position rig inference.
- `validate()` is explicit; it is not invoked automatically by a host editor.

### Verification

- Core Debug: **235/235 passed** (154 unit + 75 scenario + 6 golden).
- Core Release: **235/235 passed**.
- Examples Debug: **257/257 passed** (235 core + 22 example tests, including 7
  smoke tests).
- Examples Release: **257/257 passed**.
- `cinerig_07_third_person --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` build and 07 headless run are verified before
  completion.
- Visible window confirms the P2-07 title, target/camera positions, rig root
  and hand, collision status `on / obstacle: wall`, rig setup, target motion,
  controls, and frame count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_third_person.png`.
- Linux was not verified on hardware.
