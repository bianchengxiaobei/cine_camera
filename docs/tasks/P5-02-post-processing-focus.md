# P5-02 Post-processing and Focus

Status: completed  
Depends on: P1-03, P2-01  
Blocks: P5-03

## Goal

Port the engine-neutral parts of `CinemachineVolumeSettings`,
`CinemachinePostProcessing`, and `CinemachineAutoFocus`. A virtual camera can
attach an opaque post-process profile that contributes a weighted custom
blendable to `CameraState`; `CameraState::lerp` blends those contributions
along with the camera. The Brain delivers the final state to an optional
`IPostProcessor` adapter, and the adapter applies the engine's native volumes.
Focus tracking and damped auto-focus update the physical-camera focus distance
without requiring an engine dependency in the core library.

## Scope

- Add `PostProcessProfile` as an opaque, shared, type-erased profile handle in
  `engine/post_processor.hpp`.
- Extend `IPostProcessor` with a default no-op `resetHistory()` hook for camera
  cuts.
- Add `components::PostProcessing`:
  - profile handle and weight
  - focus tracking modes `None`, `LookAtTarget`, `FollowTarget`,
    `CustomTarget`, and `Camera`
  - focus target (`TargetRef`), focus offset, and calculated focus distance
  - Finalize-stage focus update and custom-blendable submission
  - validation and per-camera reset/reporting
- Add `components::AutoFocus`:
  - target modes `None`, `LookAtTarget`, `FollowTarget`, `CustomTarget`, and
    `Camera`
  - focus offset, damping, and per-camera current focus distance
  - Finalize-stage physical focus update, damping, validation, and maximum
    damping time
- Add a non-owning `IPostProcessor` pointer to `CameraBrain`; apply it after
  output submission and call `resetHistory()` when an activation event is a
  cut.
- Make `BlendManager::processActiveCamera()` optionally report whether an
  activation was a cut, without changing existing callers.
- Add scenario tests for profile blendables, blend-weight scaling, focus modes,
  auto-focus damping, Brain cut notifications, validation, and missing
  profiles/targets.
- Add a `36_post_processing` SDL example with three profiled shots, blended
  profile weights, focus tracking, a lightweight adapter-driven tint, live HUD,
  and a smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity `UnityEngine.Rendering.VolumeProfile`, HDRP/URP volume components,
  PostProcessing v2 `PostProcessProfile`, `PostProcessLayer`, and
  `PostProcessVolume`.
- Copying/instantiating engine profiles or modifying their Depth of Field
  overrides in place. The host adapter applies the final focus distance and
  profile weights to its native profile/volume objects.
- HDRP screen-center depth-buffer auto focus and the `FocusDistance` custom
  pass.
- Editor preview scrubbing, SaveDuringPlay, profile invalidation callbacks, and
  domain-reload cleanup.
- Real render-pipeline effects in the SDL example. The example adapter reads
  profile handles/weights and drives a visible tint and HUD.

## Design

```text
engine::PostProcessProfile
  shared_ptr<const void> data
  type_index type
  isValid() / get<T>()

CameraState
  CustomBlendable { data, type, weight }
  CustomBlendableItems already merge identical handles and CameraState::lerp
  scales their weights by (1 - t) and t

components::PostProcessing : CameraExtension
  profile / weight
  focusTracking / focusTarget / focusOffset
  on Finalize:
    if profile invalid: return
    compute focus distance for the selected mode
    clamp to the physical-camera minimum
    write state.lens.physical.focusDistance
    add profile as a custom blendable with weight

components::AutoFocus : CameraExtension
  focusMode / customTarget / focusOffset / damping
  on Finalize:
    compute target distance
    clamp to the physical-camera minimum
    damp toward it using the previous per-camera value
    write state.lens.physical.focusDistance

CameraBrain
  optional IPostProcessor*
  processActiveCamera(..., &wasCut)
  if wasCut: postProcessor->resetHistory()
  applyToOutput():
    output.apply(applied)
    postProcessor->apply(applied)
```

The Brain does not interpret profiles. It only guarantees that the final
`AppliedCamera` contains the blended custom blendables and physical focus
distance. The host adapter owns profile objects, applies all active profiles
with their blended weights, and clears/fades volumes when `hasCamera` is false.

`PostProcessing` focus tracking is immediate, matching
`CinemachineVolumeSettings`. `AutoFocus` is damped, matching
`CinemachineAutoFocus`. `PostProcessing` clamps to the 0.01m lens minimum;
`AutoFocus` preserves the original 0.1m minimum.

## Differences

- Profiles are opaque shared handles rather than Unity assets. The host adapter
  owns the concrete profile/volume objects and interprets `CustomBlendable`
  entries by type.
- CineRig does not copy profiles or mutate a Depth of Field override. Focus is
  written to `CameraState::lens.physical.focusDistance`; the adapter applies it
  to its native profile/volume.
- The Brain calls an instance `IPostProcessor` directly after
  `ICameraOutput::apply()`. There is no global `CameraUpdatedEvent` listener or
  dynamic `__CMVolumes` hierarchy.
- Camera-cut history reset is an explicit adapter hook, not a global static
  HDRP/URP camera-data lookup.
- HDRP screen-center auto-focus and its compute/custom-pass pipeline are not
  ported. `AutoFocus` covers the CPU target-distance modes.
- Profile validity is represented by the opaque handle being non-null; hosts
  that need "non-empty profile" validation must perform it before assigning the
  handle.

## API Sketch

```cpp
namespace cinerig
{

struct PostProcessProfile
{
    std::shared_ptr<const void> data;
    std::type_index type = typeid(void);

    PostProcessProfile() = default;
    PostProcessProfile(std::shared_ptr<const void> value, std::type_index valueType);
    template <typename T> explicit PostProcessProfile(std::shared_ptr<T> value);

    bool isValid() const noexcept;
    template <typename T> const T* get() const noexcept;
};

class IPostProcessor
{
public:
    virtual ~IPostProcessor() = default;
    virtual void apply(const AppliedCamera& camera) = 0;
    virtual void resetHistory() {}
};

class PostProcessing : public CameraExtension
{
public:
    enum class FocusTrackingMode { None, LookAtTarget, FollowTarget, CustomTarget, Camera };

    PostProcessProfile profile;
    float weight = 1.0f;
    FocusTrackingMode focusTracking = FocusTrackingMode::None;
    TargetRef focusTarget;
    float focusOffset = 0.0f;

    void validate();
    void reset(VirtualCamera& vcam);
    float calculatedFocusDistance(const VirtualCamera& vcam) const noexcept;
    void postPipelineStageCallback(
        VirtualCamera& vcam, Stage stage, CameraState& state,
        float deltaTime) override;
};

class AutoFocus : public CameraExtension
{
public:
    enum class FocusMode { None, LookAtTarget, FollowTarget, CustomTarget, Camera };

    FocusMode focusMode = FocusMode::None;
    TargetRef customTarget;
    float focusOffset = 0.0f;
    float damping = 0.2f;

    void validate();
    void reset(VirtualCamera& vcam);
    float currentFocusDistance(const VirtualCamera& vcam) const noexcept;
    float maxDampTime() const override;
    void postPipelineStageCallback(
        VirtualCamera& vcam, Stage stage, CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/PostProcessing/CinemachineVolumeSettings.cs`
  - profile weight, focus tracking modes, Finalize-stage focus distance, and
    custom blendable submission
- `com.unity.cinemachine/Runtime/PostProcessing/CinemachinePostProcessing.cs`
  - PostProcessing v2 profile/volume behavior
- `com.unity.cinemachine/Runtime/PostProcessing/CinemachineAutoFocus.cs`
  - target focus modes, minimum focus distance, and damping
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - custom blendable merge/lerp and physical lens focus
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs`
  - camera-updated application and cut history reset
- `com.unity.cinemachine/Documentation~/CinemachineVolumeSettings.md`
  - profile weight and focus tracking behavior
- `com.unity.cinemachine/Documentation~/CinemachineAutoFocus.md`
  - physical focus-distance and depth-of-field expectations

## Acceptance

- A valid `PostProcessing` profile adds one custom blendable with the configured
  weight and profile handle; duplicate handles merge through the existing
  `CameraState` rules.
- During a camera blend, profile weights scale by the camera blend weights and
  remain readable by `IPostProcessor`.
- An invalid/null profile adds no blendable and does not change focus.
- `LookAtTarget`, `FollowTarget`, `CustomTarget`, and `Camera` focus modes
  produce the expected distances; missing targets fall back to the focus
  offset.
- `AutoFocus` computes the same target distances, applies damping from the
  previous per-camera value, clamps to its 0.1m minimum, and reports its
  damping time.
- `PostProcessing::validate()` clamps negative/non-finite weight and
  non-finite offsets; `AutoFocus::validate()` clamps negative/non-finite
  damping and offsets.
- Setting an `IPostProcessor` on a Brain causes `apply()` once per Brain output
  frame, including frames with no active camera.
- A camera cut activation calls `resetHistory()`; ordinary updates and
  non-cut blends do not.
- Null post-processors are safe, and existing Brain behavior is unchanged when
  no adapter is installed.
- Unit/scenario tests, examples Debug/Release, smoke tests, no-tests build,
  install, and an external `find_package(CineRig)` consumer pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/engine/post_processor.hpp`
  - `PostProcessProfile` opaque shared handle with type recovery.
  - `IPostProcessor::resetHistory()` default hook.
- `CameraBrain`
  - non-owning `setPostProcessor()` / `postProcessor()`.
  - post-processor application after `ICameraOutput::apply()`.
  - cut-history reset driven by a new optional cut report from
    `BlendManager::processActiveCamera()`.
- `components::PostProcessing`
  - profile weight/custom blendable submission.
  - `None`, `LookAtTarget`, `FollowTarget`, `CustomTarget`, and `Camera` focus
    modes.
  - physical focus distance, offset, minimum clamp, validation, reset, and
    per-camera reporting.
- `components::AutoFocus`
  - `None`, `LookAtTarget`, `FollowTarget`, `CustomTarget`, and `Camera` modes.
  - clamped focus distance, per-camera damping, validation, reset, and
    `maxDampTime()`.
- `tests/scenario/post_processing_test.cpp`
  - 9 cases / 48 assertions covering profile blendables, focus modes, blend
    weight scaling, AutoFocus damping, Brain apply/cut behavior, no-active
    cameras, validation, and missing profiles.
- `examples/36_post_processing`
  - three profiled sequencer shots, Warm/Cool/Verdant opaque profiles,
    overlapping profile weights, look-at/follow/custom focus, AutoFocus
    damping, adapter-driven tint, and live profile/focus HUD.
  - deterministic fixed-step simulation when `--frames` is used.
  - smoke test.
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Verification

- `[postprocess]`: **9 cases / 48 assertions passed**.
- Core Debug and Release: **452/452 passed**
  (319 unit + 127 scenario + 6 golden; 3928 assertions).
- Examples Debug and Release: **503/503 passed**
  (452 core + 51 example/smoke tests, including 35 and 36 smoke tests).
- `cinerig_36_post_processing --frames 180 --hidden --no-vsync` exits 0 in
  Debug and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `engine/post_processor.hpp`,
  `components/post_processing.hpp`, and `components/auto_focus.hpp`. An external
  `find_package(CineRig)` consumer used `PostProcessProfile`,
  `PostProcessing`, and `IPostProcessor`, and verified the custom blendable and
  physical focus distance.
- The captured view at frame 160 shows instruction 2/3, `Shot B` live,
  `Warm 44% + Cool 56%` profiles, focus `7.198m`, and one history reset.
  Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p5_post_processing.png`.

