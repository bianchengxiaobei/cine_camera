# P2-13 Camera Offset

Status: completed  
Depends on: P0-04, P1-02  
Blocks: runtime rig adjustments, composition-preserving offsets

## Goal

Implement the `components::CameraOffset` extension. It applies a camera-space
position offset after a selected pipeline stage and can re-aim the camera to
preserve the LookAt target's screen position after the offset.

## Scope

- Add `components::CameraOffset` as a `CameraExtension` with:
  - camera-space `offset`
  - `applyAfter` stage selection
  - `preserveComposition` mode
  - corrected-position accumulation
  - reference-look-at translation when composition is not preserved
  - re-aim from the corrected position when composition is preserved
- Add unit coverage for defaults, camera-space transformation, stage selection,
  reference-look-at translation, preserve-composition re-aim, and the
  preserve-mode Body-stage exception.
- Add a `13_camera_offset` SDL example with an Aim composer, offset toggle,
  preserve toggle, stage selection, target motion, and HUD.
- Update the task index, feature matrix, and example index.

## Non-goals

- Offset damping, collision, noise, or lens effects.
- Editor `Reset` and transform serialization.
- Multiple offsets per camera or extension ordering controls.

## Design

The component mirrors the original Cinemachine implementation:

```text
if stage != applyAfter:
    return

preserveAim = preserveComposition
    && state.hasLookAt()
    && stage > Body

if preserveAim:
    screenOffset = getCameraRotationToTarget(
        rawOrientation,
        referenceLookAt - correctedPosition,
        referenceUp)

worldOffset = rawOrientation * offset
positionCorrection += worldOffset

if not preserveAim:
    referenceLookAt += worldOffset
else:
    rawOrientation = lookRotation(
        referenceLookAt - correctedPosition,
        referenceUp)
    rawOrientation = applyCameraRotation(
        rawOrientation, -screenOffset, referenceUp)
```

The offset is rotated by the current raw camera orientation. Preserving
composition is only meaningful after the Body stage because it depends on a
resulting aim orientation and an existing LookAt point.

## API Sketch

```cpp
namespace cinerig
{

class CameraOffset : public CameraExtension
{
public:
    math::Vec3 offset{};
    Stage applyAfter = Stage::Aim;
    bool preserveComposition = false;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineCameraOffset.cs`
  - stage selection, camera-space offset, reference-look-at translation, and
    preserve-composition re-aim
- `com.unity.cinemachine/Runtime/Core/CinemachineExtension.cs`
  - extension callback model
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - corrected position, raw orientation, and reference look-at

## Acceptance

- Offset is transformed by raw camera orientation and accumulated into
  `positionCorrection`.
- Non-preserving mode translates `referenceLookAt` by the world offset.
- Preserving mode keeps the target's pre-offset screen rotation and re-aims
  from the corrected camera position.
- The offset only applies at `applyAfter`.
- Preserve mode after Body falls back to non-preserving behavior.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/camera_offset.hpp` and
  `src/components/camera_offset.cpp`
  - `CameraExtension` offset application at a selected pipeline stage.
  - Camera-space offset rotation and `positionCorrection` accumulation.
  - Reference-look-at translation when composition is not preserved.
  - Preserve-composition screen rotation capture and corrected-position re-aim.
  - Body-stage preserve-mode fallback to translation.
- `tests/unit/camera_offset_test.cpp`
  - 4 cases and 17 assertions covering defaults, camera-space transformation,
    stage selection, reference-look-at translation, Body-stage preserve
    fallback, and preserved target screen rotation.
- `examples/13_camera_offset`
  - Aim composer with moving target, camera-space offset, preserve toggle,
    Aim/Finalize stage selection, scene parallax references, aim-screen-error
    HUD, and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the extension and tests.
- Task index, feature matrix, and example index updated.

### Differences

- The extension is implemented against CineRig's existing `CameraExtension`
  callback and stage enum.
- CineRig has no transform serialization or editor `Reset`; runtime behavior
  matches the original component.
- Vec2 screen rotation uses the existing CineRig geometry helpers.

### Verification

- Core Debug: **262/262 passed** (181 unit + 75 scenario + 6 golden).
- Core Release: **262/262 passed**.
- Examples Debug: **290/290 passed** (262 core + 28 example tests, including 13
  smoke tests).
- Examples Release: **290/290 passed**.
- `cinerig_13_camera_offset --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 13 example exits 0.
- Visible window confirms the P2-13 title, camera-space offset, preserve mode,
  centered target, parallax columns, near-zero aim screen error, controls, and
  frame count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_camera_offset.png`.
- Linux was not verified on hardware.
