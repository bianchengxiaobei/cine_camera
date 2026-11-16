# P2-10 Hard Lock To Target

Status: completed  
Depends on: P0-04, P1-02, P2-01  
Blocks: tool/attachment cameras, simple tracking rigs

## Goal

Implement the `components::HardLockToTarget` Body component. It places the
camera on the Tracking target position with optional detached target damping
and target-warp handling.

## Scope

- Add `components::HardLockToTarget` in the Body stage with:
  - `damping` (seconds) and validation
  - previous-target position state
  - target attachment-aware damping
  - target-warp handling and `maxDampTime()`
  - invalid-target behavior without mutating the camera
- Add unit coverage for direct lock, damping, negative-time reset, target
  warping, invalid targets, and max damping.
- Add `10_hard_lock` SDL example with a moving target, camera position lock,
  damping toggle, pause/reset, and HUD.
- Update the task index, feature matrix, and example index.

## Non-goals

- HardLookAt, RotateWithFollowTarget, collision, noise, or lens effects.
- Force-position inheritance.
- Unity transform serialization.

## Design

The component mirrors the original Cinemachine implementation:

```text
target = follow target position
if previous state valid and deltaTime >= 0:
    position = previousPosition
        + DetachedFollowTargetDamp(target - previousPosition, damping, deltaTime)
else:
    position = target
previousPosition = position
state.rawPosition = position
```

`DetachedFollowTargetDamp` uses the virtual camera's follow-target attachment:
it blends the damping time toward `max(1, damping)` and the effective
`deltaTime` toward zero as attachment decreases. A negative `deltaTime` snaps to
the target.

`onTargetObjectWarped()` advances the cached previous position when the warped
entity is the effective Tracking target.

## API Sketch

```cpp
namespace cinerig
{

class HardLockToTarget : public CameraComponent
{
public:
    float damping = 0.0f;

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    float maxDampTime() const override;
    void validate();

private:
    math::Vec3 previousTargetPosition_{};
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineHardLockToTarget.cs`
  - damping, previous position, and warp behavior
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - `DetachedFollowTargetDamp`
- `com.unity.cinemachine/Tests/Runtime/CameraPositionTests.cs`
  - HardLockToTarget baseline position test

## Acceptance

- Zero damping places the camera exactly on the Tracking target.
- Positive damping follows target motion with the expected lag and converges.
- Negative time snaps to the target.
- Target warps preserve continuity.
- Debug/Release builds and the existing test suite pass.

## Implementation Record

To be completed.

## Implementation Record

### Implemented

- `include/cinerig/components/hard_lock_to_target.hpp` and
  `src/components/hard_lock_to_target.cpp`
  - Body-stage target-position lock with optional detached damping.
  - Previous-position state, target-attachment-aware damping, negative-time
    snap, target-warp handling, validation, and `maxDampTime()`.
- `tests/unit/hard_lock_to_target_test.cpp`
  - 4 cases and 15 assertions covering direct lock, damping/convergence,
    negative-time snap, target warp, target attachment, missing targets, and
    validation.
- `examples/10_hard_lock`
  - Moving tool target, camera lock error HUD, damping toggle, pause/reset, and
    headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the component and tests.
- Task index, feature matrix, and example index updated.

### Differences

- CineRig has no transform serialization or editor OnValidate; `validate()` is
  explicit.
- Target warps use the effective `TargetRef`/`EntityId` rather than Unity
  `Transform` identity.
- The example composes HardLockToTarget with RotationComposer to keep a separate
  look-at point visible from the locked camera position.

### Verification

- Core Debug: **250/250 passed** (169 unit + 75 scenario + 6 golden).
- Core Release: **250/250 passed**.
- Examples Debug: **275/275 passed** (250 core + 25 example tests, including 10
  smoke tests).
- Examples Release: **275/275 passed**.
- `cinerig_10_hard_lock --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 10 example exits 0.
- Visible window confirms the P2-10 title, lock error 0, damping mode, moving
  target state, controls, and frame count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_hard_lock.png`.
- Linux was not verified on hardware.
