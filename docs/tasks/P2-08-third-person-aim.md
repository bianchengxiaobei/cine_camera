# P2-08 Third Person Aim

Status: completed  
Depends on: P0-04, P1-02, P2-01, P2-07  
Blocks: third-person shooter rigs with stable aim

## Goal

Implement the `components::ThirdPersonAim` camera extension. It raycasts along
the camera/player forward direction, stores the hit point in
`CameraState::referenceLookAt`, and optionally cancels rotational noise by
re-orienting the camera to keep the aim point centered.

## Scope

- Add `components::ThirdPersonAim` as a `CameraExtension` with:
  - aim collision layer filter and ignored `EntityId`
  - `aimDistance` with a minimum of 1
  - `noiseCancellation`
  - `AimTarget` accessor for the player-space aim point
  - Body-stage aim point detection and Finalize-stage stabilization
  - forward-origin adjustment when the camera is behind the followed player
- Add unit coverage for default settings, hit/fallback look-at points, filter
  and ignore behavior, aim-target computation, and noise-cancellation
  orientation.
- Add an `08_third_person_aim` SDL example or integrate the extension into a
  third-person scene with an aim marker and HUD.
- Update the task index, feature matrix, architecture/coordinate notes, and
  example index.

## Non-goals

- Noise generation; this extension only stabilizes an existing camera state.
- General-purpose decolliders/occluders.
- Unity tags; use `EntityId` for ignored entities.
- Serialization or editor inspectors.

## Design

The extension mirrors the original `CinemachineThirdPersonAim`:

- At the Body stage, when `noiseCancellation` is enabled and a follow target is
  available, compute the look-at point from the camera position and the
  player's forward direction. Store it in `state.referenceLookAt`.
- At the Finalize stage, when `noiseCancellation` is enabled, re-orient the
  camera toward `state.referenceLookAt` and clear orientation correction.
- When `noiseCancellation` is disabled, perform the same detection at Finalize
  using the corrected camera orientation, without changing camera orientation.
- `AimTarget` is the point the player would hit from the player origin to the
  detected camera aim point; it can differ from `ReferenceLookAt` because of the
  camera/player parallax.

For CineRig's right-handed `-Z` forward convention, the original local-space
"camera behind player" check uses the component of
`playerPosition - cameraPosition` along the player's forward direction:

```cpp
const Vec3 local = rotate(inverse(playerRotation), playerPosition - cameraPosition);
const float distanceAlongForward = -local.z;
if (distanceAlongForward > 0.0f)
{
    cameraPosition += forward * distanceAlongForward;
    aimDistance -= distanceAlongForward;
}
aimDistance = max(1.0f, aimDistance);
```

Ray queries use `IPhysicsWorld::raycast` with
`QueryFilter{aimCollisionFilter, ignoreEntity}`. A null physics adapter or
empty layer mask produces no hit and falls back to the configured aim distance.

## API Sketch

```cpp
namespace cinerig
{

class ThirdPersonAim : public CameraExtension
{
public:
    LayerMask aimCollisionFilter{1u};
    EntityId ignoreEntity = 0;
    float aimDistance = 200.0f;
    bool noiseCancellation = true;

    void validate();
    const math::Vec3& aimTarget() const noexcept;
    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    math::Vec3 computeLookAtPoint(
        math::Vec3 cameraPosition,
        math::Vec3 playerPosition,
        math::Quat playerRotation,
        math::Vec3 forward) const;
    math::Vec3 computeAimTarget(
        math::Vec3 cameraLookAt,
        math::Vec3 playerPosition) const;

    math::Vec3 aimTarget_{};
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineThirdPersonAim.cs`
  - Body/Finalize stages, camera/player origin correction, hit fallback, noise
    cancellation, and `AimTarget`
- `com.unity.cinemachine/Documentation~/CinemachineThirdPersonAim.md`
  - aim detection and parallax semantics
- `com.unity.cinemachine/Runtime/Components/CinemachineThirdPersonFollow.cs`
  - first-person look-at correction that this extension complements

## Acceptance

- Defaults match the original aim settings.
- Hits and misses produce the correct reference look-at point and aim target.
- Layer filtering and ignored entities are respected.
- Noise cancellation re-orients the camera toward the reference point.
- Disabled noise cancellation detects the aim point without changing camera
  orientation.
- Debug/Release builds and the existing test suite pass.

## Implementation Record

To be completed.

## Implementation Record

### Implemented

- `include/cinerig/components/third_person_aim.hpp` and
  `src/components/third_person_aim.cpp`
  - `CameraExtension` with aim collision layers, ignored `EntityId`,
    configurable `aimDistance`, and noise-cancellation mode.
  - Body-stage camera/player aim point detection and Finalize-stage orientation
    stabilization.
  - Player-origin correction for cameras behind the player, adjusted for
    CineRig's right-handed `-Z` forward convention.
  - Player-space `AimTarget` computation and public accessor.
  - Fallback to the configured aim distance when no hit or no physics adapter is
    available.
- `tests/unit/third_person_aim_test.cpp`
  - 5 cases and 23 assertions covering defaults/validation, hits,
    fallbacks, filters/ignored entities, and disabled noise cancellation.
- `examples/08_third_person_aim`
  - ThirdPersonFollow plus ThirdPersonAim, visible aim sphere and marker,
    collision toggle, noise-cancellation toggle, pause/reset, HUD, and headless
    smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the extension and tests.
- Task index, feature matrix, architecture/coordinate notes, and example index
  updated.

### Differences

- CineRig has no Unity tags. Aim queries use `EntityId ignoreEntity` in
  `QueryFilter` instead of `IgnoreTag`.
- The extension is attached explicitly with `VirtualCamera::addExtension()` and
  exposes a const `aimTarget()` accessor instead of a public inspector field.
- A null physics adapter or empty layer mask returns the distance fallback
  without querying; it is not treated as a fatal error.
- `validate()` is explicit; it is not invoked automatically by a host editor.
- Noise cancellation only stabilizes the existing camera state; noise
  generation remains a P3 feature.

### Verification

- Core Debug: **241/241 passed** (160 unit + 75 scenario + 6 golden).
- Core Release: **241/241 passed**.
- Examples Debug: **264/264 passed** (241 core + 23 example tests, including 8
  smoke tests).
- Examples Release: **264/264 passed**.
- `cinerig_08_third_person_aim --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 08 example exits 0.
- Visible window confirms the P2-08 title, target/camera rig, collision wall,
  aim mode, aim target, aim marker, controls, and frame count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_third_person_aim.png`.
- Linux was not verified on hardware.

