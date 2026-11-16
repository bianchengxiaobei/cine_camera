# P2-14 Recomposer

Status: completed  
Depends on: P0-04, P1-02  
Blocks: Timeline-style final composition tweaks, hand-adjusted procedural aim

## Goal

Implement the `components::Recomposer` extension. It applies a final tilt, pan,
dutch, and zoom adjustment and can relax the camera's follow/look-at target
attachments before the pipeline runs.

## Scope

- Add `components::Recomposer` as a `CameraExtension` with:
  - `applyAfter` stage selection, defaulting to Finalize
  - `tilt`, `pan`, and `dutch` in degrees
  - `zoomScale` affecting both perspective FOV and orthographic size
  - `followAttachment` and `lookAtAttachment` applied in the pre-pipeline
    callback
  - validation that clamps zoom and attachments
- Add unit coverage for defaults, validation, attachment propagation, local
  tilt versus world-up pan, dutch/zoom lens changes, stage selection, and
  orientation correction.
- Add a `14_recomposer` SDL example with an Aim composer, interactive
  tilt/pan/dutch/zoom/attachment controls, scene references, and HUD.
- Update the task index, feature matrix, and example index.

## Non-goals

- Timeline integration or recorded-camera playback.
- Serializable animation curves or keyframes.
- Noise, collision, or post-processing.
- Editor-only `Reset` and transform serialization.

## Design

The component mirrors the original Cinemachine implementation:

```text
pre-pipeline:
    vcam.followTargetAttachment = followAttachment
    vcam.lookAtTargetAttachment = lookAtAttachment

if stage != applyAfter:
    return

lens = state.lens
tilted = rawOrientation * AngleAxis(tilt, right)
desired = AngleAxis(pan, referenceUp) * tilted
orientationCorrection = inverse(correctedOrientation) * desired

lens.dutch += dutch
if zoomScale != 1:
    lens.orthographicSize *= zoomScale
    lens.fieldOfView *= zoomScale
state.lens = lens
```

Tilt is local to the camera (around local right), pan is around the state's
reference up, and dutch is added to the lens after orientation correction. The
correction formula preserves the original Cinemachine behavior when another
extension has already contributed an orientation correction.

## API Sketch

```cpp
namespace cinerig
{

class Recomposer : public CameraExtension
{
public:
    Stage applyAfter = Stage::Finalize;
    float tilt = 0.0f;
    float pan = 0.0f;
    float dutch = 0.0f;
    float zoomScale = 1.0f;
    float followAttachment = 1.0f;
    float lookAtAttachment = 1.0f;

    void validate();

    void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam,
        CameraState& state,
        float deltaTime) override;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineRecomposer.cs`
  - attachment setters, stage check, orientation correction, dutch and zoom
- `com.unity.cinemachine/Runtime/Core/CinemachineExtension.cs`
  - pre-pipeline and post-stage callback model
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - raw/corrected orientation, reference up, and lens fields

## Acceptance

- Defaults match the original Reset values: apply after Finalize, neutral
  angles, zoom scale 1, attachments 1.
- The pre-pipeline callback sets both virtual-camera target attachments.
- Tilt rotates around local right; pan rotates around reference up.
- Dutch and zoom are reflected in the resulting lens.
- The extension only adjusts at `applyAfter`.
- Validation clamps zoom to at least 0.01 and attachments to 0..1.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/recomposer.hpp` and
  `src/components/recomposer.cpp`
  - Final-stage tilt, pan, dutch, and zoom recomposition.
  - Local-right tilt and reference-up pan orientation correction.
  - Follow/look-at attachment propagation in the pre-pipeline callback.
  - `validate()` zoom floor and attachment clamps.
- `tests/unit/recomposer_test.cpp`
  - 4 cases and 26 assertions covering defaults, validation, attachment
    propagation, stage selection, local tilt/world-up pan, dutch, and
    perspective/orthographic zoom.
- `examples/14_recomposer`
  - Aim composer with moving target, interactive tilt/pan/dutch/zoom, relaxed
    attachment toggle, scene references, HUD, and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the extension and tests.
- Task index, feature matrix, and example index updated.

### Differences

- The extension uses CineRig's existing pre-pipeline and post-stage callback
  interfaces.
- CineRig has no transform serialization or editor-only `Reset`; default values
  match the original Reset state and `validate()` mirrors OnValidate clamping.
- Angles remain degrees in the public API and are converted to radians for
  quaternion math.

### Verification

- Core Debug: **266/266 passed** (185 unit + 75 scenario + 6 golden).
- Core Release: **266/266 passed**.
- Examples Debug: **295/295 passed** (266 core + 29 example tests, including 14
  smoke tests).
- Examples Release: **295/295 passed**.
- `cinerig_14_recomposer --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 14 example exits 0.
- Visible window confirms the P2-14 title, tilt/pan/dutch values, zoom scale and
  resulting FOV, attachments, tilted composition, target, controls, and frame
  count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_recomposer.png`.
- Linux was not verified on hardware.
