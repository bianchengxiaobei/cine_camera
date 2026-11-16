# P2-12 Rotate With Follow Target

Status: completed  
Depends on: P0-04, P1-02, P2-01  
Blocks: chase cameras, target-aligned mounts, no-look-at rigs

## Goal

Implement the `components::RotateWithFollowTarget` Aim component. It matches
the Tracking target's orientation with optional target-attachment-aware angular
damping and updates the state's reference up vector from the damped orientation.

## Scope

- Add `components::RotateWithFollowTarget` in the Aim stage with:
  - `damping` in seconds and `maxDampTime()`
  - previous reference orientation state
  - detached follow-target damping for the slerp weight
  - negative-time snap
  - `referenceUp = dampedOrientation * up`
  - invalid-target behavior without mutating the camera
- Add unit coverage for defaults, direct match, damped convergence, negative
  time, attachment damping, reference up, invalid targets, and max damping.
- Add a `12_rotate_with_follow` SDL example with a moving/rotating target,
  chase camera, damping toggle, pause/reset, and HUD.
- Update the task index, feature matrix, and example index.

## Non-goals

- LookAt targeting, position lock, collision, noise, or lens effects.
- Unity transform serialization or the deprecated `CinemachineSameAsFollowTarget`
  alias.
- Camera roll correction beyond copying the target orientation.

## Design

The component mirrors the original Cinemachine implementation:

```text
if not valid follow target:
    return

targetRotation = follow target rotation (identity if unavailable)
damped = targetRotation
if deltaTime >= 0:
    t = DetachedFollowTargetDamp(1, damping, deltaTime)
    damped = slerp(previousReferenceOrientation, targetRotation, t)
previousReferenceOrientation = damped
state.rawOrientation = damped
state.referenceUp = damped * up
```

`DetachedFollowTargetDamp` uses the virtual camera's follow-target attachment:
it blends the damping time toward `max(1, damping)` and the effective
`deltaTime` toward zero as attachment decreases. A negative `deltaTime` snaps
directly to the target and still records it as the previous orientation.

## API Sketch

```cpp
namespace cinerig
{

class RotateWithFollowTarget : public CameraComponent
{
public:
    float damping = 0.0f;

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    float maxDampTime() const override;

private:
    math::Quat previousReferenceOrientation_{};
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineRotateWithFollowTarget.cs`
  - validity, previous orientation, slerp weight, raw orientation, reference up
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - `DetachedFollowTargetDamp`
- `com.unity.cinemachine/Runtime/Deprecated/CinemachineSameAsFollowTarget.cs`
  - deprecated alias with identical runtime behavior

## Acceptance

- With zero damping the camera orientation matches the target immediately.
- Damping converges from the previous orientation to the target.
- Negative `deltaTime` snaps directly to the target.
- Low follow-target attachment reduces the effective damping response.
- `referenceUp` follows the damped camera orientation.
- Missing targets leave orientation unchanged.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/rotate_with_follow_target.hpp` and
  `src/components/rotate_with_follow_target.cpp`
  - Aim-stage target-orientation matching with previous reference orientation.
  - Detached follow-target damping for the slerp weight.
  - Negative-time snap and reference-up update from the damped orientation.
  - Invalid-target guard and `maxDampTime()`.
- `tests/unit/rotate_with_follow_target_test.cpp`
  - 4 cases and 18 assertions covering defaults, immediate match, damping
    convergence, negative-time snap, attachment damping, reference up, max
    damping, and invalid targets.
- `examples/12_rotate_with_follow`
  - Follow body chase rig, moving/banking target, target axes, damping toggle,
    pause/reset, orientation-error HUD, and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the component and tests.
- Task index, feature matrix, and example index updated.

### Differences

- The deprecated `CinemachineSameAsFollowTarget` alias is not recreated; its
  behavior is covered by `RotateWithFollowTarget`.
- CineRig has no transform serialization or editor `OnValidate`; runtime
  behavior matches the original component.
- Missing target rotations fall back to identity, matching the original
  component base fallback.

### Verification

- Core Debug: **258/258 passed** (177 unit + 75 scenario + 6 golden).
- Core Release: **258/258 passed**.
- Examples Debug: **285/285 passed** (258 core + 27 example tests, including 12
  smoke tests).
- Examples Release: **285/285 passed**.
- `cinerig_12_rotate_with_follow --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 12 example exits 0.
- Visible window confirms the P2-12 title, moving/banking target, tilted
  horizon from matched orientation, damping error HUD, controls, and frame
  count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_rotate_with_follow.png`.
- Linux was not verified on hardware.
