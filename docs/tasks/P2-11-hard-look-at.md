# P2-11 Hard Look At

Status: completed  
Depends on: P0-04, P1-02, P2-01  
Blocks: look-at rigs, simple aim cameras

## Goal

Implement the `components::HardLookAt` Aim component. It points the camera
directly at the LookAt target, with an optional offset in the target's local
space and stable handling of vertical aim.

## Scope

- Add `components::HardLookAt` in the Aim stage with:
  - `lookAtOffset` in target-local space
  - validity from an attached LookAt target
  - correct-position aiming
  - reference-up look rotation for ordinary directions
  - forward `fromToRotation` for directions parallel to reference up
  - no mutation without a valid target or usable look-at direction
- Add unit coverage for defaults, direct aiming, rotated offset, vertical aim,
  invalid targets, and zero-length directions.
- Add an `11_hard_look_at` SDL example with a moving/rotating target, offset
  toggle, pause/reset, and HUD.
- Update the task index, feature matrix, and example index.

## Non-goals

- Damping, noise, collision, lens effects, or target groups.
- Position placement or target tracking.
- Unity transform serialization or editor `Reset` hooks.

## Design

The component mirrors the original Cinemachine implementation:

```text
if valid look-at target and state has look-at:
    offset = lookAtTargetRotation * lookAtOffset
    direction = referenceLookAt + offset - correctedPosition
    if |direction| > epsilon:
        if |cross(normalize(direction), referenceUp)| < epsilon:
            rawOrientation = fromToRotation(forward, direction)
        else:
            rawOrientation = lookRotation(direction, referenceUp)
```

The target-local offset is rotated by the LookAt target rotation. When that
rotation is unavailable, the identity rotation is used, matching the original
component base behavior.

The vertical-aim branch handles look directions parallel to the reference up
vector, where a normal look rotation has no unique roll. `fromToRotation` gives
the same deterministic result as Unity's `Quaternion.FromToRotation`.

## API Sketch

```cpp
namespace cinerig
{

class HardLookAt : public CameraComponent
{
public:
    math::Vec3 lookAtOffset{};

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineHardLookAt.cs`
  - validity, target-local offset, and orientation branches
- `com.unity.cinemachine/Runtime/Core/CinemachineComponentBase.cs`
  - `LookAtTargetRotation` identity fallback
- `com.unity.cinemachine/Tests/Runtime/ClearShotTests.cs`
  - HardLookAt use in clear-shot camera setups

## Acceptance

- Direct target tracking points `forward` at the target from the corrected
  camera position.
- Local offsets rotate with the LookAt target.
- Vertical aim is deterministic and does not produce NaN.
- Missing targets and zero-length directions leave the orientation unchanged.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/hard_look_at.hpp` and
  `src/components/hard_look_at.cpp`
  - Aim-stage orientation from the corrected camera position to the LookAt
    target plus a target-local offset.
  - Reference-up look rotation with the vertical-direction
    `fromToRotation(forward, direction)` branch.
  - Target rotation fallback to identity when unavailable.
  - Invalid-target, missing look-at, and zero-length direction guards.
- `tests/unit/hard_look_at_test.cpp`
  - 4 cases and 13 assertions covering direct aim, rotated local offset,
    vertical/opposite aim, zero-length direction, invalid target, and defaults.
- `examples/11_hard_look_at`
  - Fixed camera, moving/rotating target, offset marker and aim line, offset
    toggle, pause/reset, aim-error HUD, and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the component and tests.
- Task index, feature matrix, and example index updated.

### Differences

- CineRig has no transform serialization, editor `Reset`, or
  `CameraLooksAtTarget` metadata; they do not affect runtime orientation.
- A missing target rotation uses identity, matching Cinemachine's component
  base fallback.
- Aiming uses `CameraState::correctedPosition()` exactly as the original.

### Verification

- Core Debug: **254/254 passed** (173 unit + 75 scenario + 6 golden).
- Core Release: **254/254 passed**.
- Examples Debug: **280/280 passed** (254 core + 26 example tests, including 11
  smoke tests).
- Examples Release: **280/280 passed**.
- `cinerig_11_hard_look_at --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 11 example exits 0.
- Visible window confirms the P2-11 title, zero aim error, target-local offset
  marker, aim line, controls, and frame count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_hard_look_at.png`.
- Linux was not verified on hardware.
