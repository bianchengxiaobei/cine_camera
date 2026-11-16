# P5-01 Timeline Camera Track

Status: completed  
Depends on: P1-03  
Blocks: P5-02

## Goal

Port the camera-driving part of `CinemachineTimeline` into an engine-neutral
adapter. A timeline track owns one `brain::TimelineMixer`; every update it
selects up to two active shot clips and pushes their cameras, relative weight,
and delta time into the brain's existing override stack. While a clip is
active, timeline control replaces the Brain's normal priority selection. When
the track has no active clips, or the timeline is paused/stopped, control
returns to the Brain without destroying the track's override slot.

## Scope

- Add `ICameraOverrideStack` in the brain module as the host-facing contract
  for camera overrides. `CameraBrain` implements it directly.
- Add `TimelineShot` describing a clip's camera, local clip time,
  duration, and current input weight.
- Add `TimelineMixer`:
  - non-owning reference to an `ICameraOverrideStack`
  - fixed track priority
  - one persistent override ID per track
  - per-update shot selection and override submission
  - explicit, idempotent `release()`
  - release on destruction
  - inspection of the override ID
- Preserve the original playable mixer's active-clip and incoming/outgoing
  inference:
  - at most two valid clips with positive weight drive the override
  - the first two valid active clips in list order are used; later active
    clips are ignored
  - a clip's local time determines which overlapping clip is outgoing
  - equal local times use the shorter clip as the determining clip
  - a single clip fading out past half its duration is treated as outgoing
  - swapping the inferred incoming clip inverts `weightB`
- Feed cameras and weight straight into the override stack so timeline
  overlap controls the blend, independent of the Brain's default/custom blend
  lookup.
- Add scenario tests for single-shot cuts, overlap weights, fade-out,
  paused/empty tracks, invalid/zero-weight shots, priority between multiple
  tracks, and release/reuse.
- Add a `35_timeline` SDL example with a looping shot timeline, overlapping
  clips, a higher-priority gameplay camera, track enable/pause controls, and a
  smoke test.
- Update the task index, feature matrix, architecture notes, and example
  index.

## Non-goals

- Unity `PlayableGraph`, `PlayableDirector`, `TrackAsset`, `PlayableAsset`,
  exposed references, clip assets, and editor timeline integration.
- Timeline clip easing assets, animation tracks, activation tracks, signal
  tracks, and marker events. Hosts provide final clip weights and local times.
- Editor preview, scrubbing, `TargetPositionCache` record/playback, and the
  original `ScrubbingCacheHelper`.
- More than two simultaneous active clips. The original mixer deliberately
  processes at most two; the adapter preserves that limit.
- Managing shot cameras or virtual-camera lifetime. Cameras remain owned by
  `CameraSystem`; the adapter only holds non-owning pointers supplied for the
  current frame.
- New blend curves or changes to the core blend algorithm.

## Design

```text
brain::ICameraOverrideStack
  setCameraOverride(overrideId, priority, camA, camB, weightB, deltaTime)
  releaseCameraOverride(overrideId)
  defaultWorldUp()

brain::TimelineShot
  camera / clipTime / clipDuration / weight

brain::TimelineMixer
  stack: ICameraOverrideStack&
  priority: int
  overrideId: int = -1

  processFrame(shots, playing, deltaTime)
    indexA = -1, indexB = -1, activeInputs = 0
    weightB = 1
    for each shot in list order:
      reject null/invalid camera, non-playing state, weight <= 0
      indexA = indexB
      indexB = current index
      weightB = current weight
      if activeInputs == 2:
        incomingIsA = current.clipTime >= previous.clipTime
        if equal times:
          incomingIsA = current.clipDuration < previous.clipDuration
        break
    if exactly one active clip, weightB < 1, and clipTime > duration/2:
      incomingIsA = true
    if incomingIsA:
      swap indexA/indexB
      weightB = 1 - weightB
    overrideId = stack.setCameraOverride(
      overrideId, priority,
      camA, camB, weightB, deltaTime)
```

The adapter submits an override even when no clips are active, passing null
cameras. This makes the override frame inactive without removing it, exactly
like the original mixer: lower-priority timeline tracks or the gameplay Brain
can take over temporarily, and the same track resumes its slot on the next
active clip. `release()` is reserved for timeline stop/destruction.

`overrideId == -1` means the mixer has not submitted yet. A new negative ID is
allocated by the override stack on the first submission. Subsequent updates
reuse it. Priority is fixed for the lifetime of the mixer; a changed priority
requires releasing and creating a new track/mixer, matching the intended
per-track lifetime.

The adapter does not require the Brain's blend lookup. Overlapping clips
produce `weightB` directly in the override frame, so the BlendManager blends
between the two clip cameras with the timeline-supplied weight.

## Differences

- Unity Playables provide clip local time, duration, weight, and play state.
  CineRig hosts supply the same values in `TimelineShot`; the adapter does not
  evaluate a graph or clip easing.
- Clip times use `float`, consistent with the rest of the camera state and
  blend APIs. Hosts that need double-precision timeline time should convert at
  the bridge.
- Editor preview, scrubbing caches, `TargetPositionCache` record/playback, and
  `ScrubbingCacheHelper` are host/editor concerns and are not ported.
- `EnsureStarted()` is not ported, and the adapter never updates cameras.
  Timeline cameras should remain registered with `CameraSystem` (typically at
  a lower priority while the track overrides them) or be updated by the host.
  The Brain still updates the incoming camera when activation changes.
- Track priority is immutable for a mixer's lifetime. Recreating the track is
  the supported way to change priority, avoiding stale ordering in the core
  override stack.

## API Sketch

```cpp
namespace cinerig
{

class ICameraOverrideStack
{
public:
    virtual ~ICameraOverrideStack() = default;

    virtual int setCameraOverride(
        int overrideId,
        int priority,
        ICinemachineCamera* camA,
        ICinemachineCamera* camB,
        float weightB,
        float deltaTime) = 0;

    virtual void releaseCameraOverride(int overrideId) = 0;
    virtual math::Vec3 defaultWorldUp() const = 0;
};

struct TimelineShot
{
    ICinemachineCamera* camera = nullptr;
    float clipTime = 0.0f;
    float clipDuration = 0.0f;
    float weight = 0.0f;
};

class TimelineMixer
{
public:
    explicit TimelineMixer(ICameraOverrideStack& stack, int priority = 0);
    ~TimelineMixer();

    TimelineMixer(const TimelineMixer&) = delete;
    TimelineMixer& operator=(const TimelineMixer&) = delete;

    int processFrame(
        std::span<const TimelineShot> shots,
        bool playing,
        float deltaTime);
    void release();

    int overrideId() const noexcept;
    int priority() const noexcept;
    bool hasOverride() const noexcept;
};

} // namespace cinerig
```

## Reference

- `com.unity.cinemachine/Runtime/Timeline/CinemachinePlayableMixer.cs`
  - active clip collection, incoming/outgoing inference, weight inversion,
    override submission, and release on playable destruction
- `com.unity.cinemachine/Runtime/Timeline/CinemachineTrack.cs`
  - track priority and mixer creation
- `com.unity.cinemachine/Runtime/Timeline/CinemachineShot.cs`
  - clip-to-camera binding and validity
- `com.unity.cinemachine/Runtime/Timeline/CinemachineShotPlayable.cs`
  - shot camera holder
- `com.unity.cinemachine/Runtime/Core/CameraBlendStack.cs`
  - persistent override frames, ordering, and inactive-frame behavior
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs`
  - override-stack exposure through the Brain
- `com.unity.cinemachine/Documentation~/concept-timeline.md`
  - cuts, overlaps, multiple tracks, and return to gameplay control
- `com.unity.cinemachine/Documentation~/ControllingAndCustomizingBlends.md`
  - timeline overlaps bypass default/custom blend lookup

## Acceptance

- `CameraBrain` satisfies `ICameraOverrideStack`; its existing override
  behavior and tests are unchanged.
- A single active shot drives the Brain output to that camera.
- Two valid overlapping shots produce the original camera pair, incoming
  direction, and `weightB`; `weightB` remains independent of Brain blend rules.
- A single shot fading out after half its clip duration blends from that shot
  back to the lower override frame/gameplay camera using `1 - weight`.
- Paused or empty tracks submit an inactive override and return control to the
  Brain, without releasing the track's override slot.
- Null, invalid, and zero-weight shots are ignored.
- Multiple tracks honor override priority; equal priorities prefer the most
  recently created/applied frame, matching the core stack.
- `release()` is idempotent and removes only this track's override. A later
  `processFrame()` allocates a new override ID.
- Timeline adapter cameras are non-owning and are not updated, enabled, or
  destroyed by the adapter.
- Unit/scenario tests, examples Debug/Release, smoke tests, no-tests build,
  install, and an external `find_package(CineRig)` consumer pass on Windows.


## Implementation Record

### Implemented

- `include/cinerig/brain/camera_override_stack.hpp`
  - `ICameraOverrideStack` with override submission, release, and default
    world-up access.
- `CameraBrain` now implements `ICameraOverrideStack`; its existing override
  behavior is exposed through the engine-neutral interface.
- `include/cinerig/brain/timeline_mixer.hpp`,
  `src/brain/timeline_mixer.cpp`
  - `TimelineShot` and `TimelineMixer`.
  - active-shot filtering for null/invalid cameras, play state, and weight.
  - original two-clip limit and list-order selection.
  - original local-time incoming detection, equal-time duration rule,
    half-duration fade-out rule, and weight inversion.
  - persistent override ID and inactive null-camera submission when a track is
    empty or paused.
  - idempotent release and destructor cleanup.
- `tests/scenario/timeline_mixer_test.cpp`
  - 8 cases / 50 assertions covering single shots, overlap weights,
    independence from Brain blend rules, fade-out, paused/empty tracks,
    invalid/zero-weight shots, multiple-track priority and recency, equal-time
    incoming selection, two-clip limits, and release/destruction.
- `examples/35_timeline`
  - looping three-clip timeline with two 0.55-second overlaps.
  - gameplay camera above the timeline track in Brain priority.
  - live clip weights, override frame state, timeline clock, clip ranges, and
    gameplay fallback in the HUD.
  - `D` timeline drive, `Space` pause, `L` loop, `R` restart.
  - deterministic fixed-step simulation when `--frames` is used.
  - smoke test.
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Verification

- `[timeline]`: **8 cases / 50 assertions passed**.
- Core Debug and Release: **443/443 passed**
  (319 unit + 118 scenario + 6 golden).
- Examples Debug and Release: **493/493 passed**
  (443 core + 49 example tests + 1 timeline smoke test).
- `cinerig_35_timeline --frames 180 --hidden --no-vsync` exits 0 in Debug and
  Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/brain/camera_override_stack.hpp` and
  `cinerig/brain/timeline_mixer.hpp`. An external `find_package(CineRig)`
  consumer implemented the override interface, drove two timeline shots,
  checked the selected cameras/weight/delta time, and released the track.
- The captured view at frame 146 shows `t=2.433` and active clips
  `A 22% + B 78%`, track state `blending`, override `#1`, and `Clip B` live
  despite the gameplay camera having priority 100. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p5_timeline.png`.
