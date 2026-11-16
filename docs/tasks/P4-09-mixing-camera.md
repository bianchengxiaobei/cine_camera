# P4-09 MixingCamera

Status: completed  
Depends on: P1-04  
Blocks: P4-10 ClearShot

## Goal

Port `CinemachineMixingCamera`. This manager camera exposes its children in
eight fixed weight slots and produces the weighted average of the enabled
children's camera states, without an internal transition blend.

## Scope

- Add `managers::MixingCamera`:
  - eight fixed weights matching Timeline-friendly slots
  - getters/setters by slot index and by child camera
  - reset, validation, and non-negative weight handling
  - weighted sequential `CameraState::lerp` over enabled children
  - highest-weight live child and live-child percentage
  - child live flags for the update manager's standby policy
  - `isLiveChild` semantics for all positively weighted children
  - manager Finalize extension callback and owner transform write
  - child transition notification and inherited force/warp behavior
- Add a `31_mixing_camera` SDL example with three cameras, weight presets,
  child frustums, the mixed frustum, weight HUD, and a smoke test.
- Add scenario tests for defaults/reset, slot and child weight access,
  weighted state mixing, zero/disabled weights, live-child selection and
  percentages, live flags, Finalize callbacks, and eight-slot limits.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Internal blends between children. Mixing is immediate per-frame averaging,
  matching the original manager.
- Timeline integration or animating array elements. The fixed slots are
  retained, but CineRig uses a `std::array<float, 8>`.
- Unity child discovery through Transform hierarchy. Children remain explicit
  through `VirtualCameraManagerBase`.
- Editor Reset/OnValidate callbacks. `reset()` and `validate()` are explicit.
- ClearShot, Sequencer, and StateDriven camera policies.
- Child camera pipelines are not updated from inside the mixer; the CineRig
  update manager continues to own update scheduling. Weighted children are
  marked live so subsequent standby updates keep their states fresh.

## Design

```text
managers::MixingCamera : VirtualCameraManagerBase
  std::array<float, MaxCameras> weights
  reset() / validate()
  getWeight(index/child) / setWeight(index/child)
  liveChild() / liveChildPercent()
  isLiveChild(child, dominantOnly)

  internalUpdateCameraState(worldUp, dt)
    update caches
    clear live state when previous state is invalid
    for each enabled child in the first 8 slots with weight > epsilon:
      ensure a new child has a valid state
      accumulate weight
      first child: state = child.state
      otherwise:   state = CameraState::lerp(state, child.state, weight / total)
      track highest weight
    mark weighted children live, clear unweighted ones
    state_ = mixed state
    invoke Finalize extensions and write the owner transform
    previousStateIsValid = true
```

The live child is the first child with the highest positive weight. Its
percentage is `highest / total * 100`, or zero when the total is negligible.
The manager's own internal blend manager is not used for mixing, so
`isBlending()` remains false and `activeBlend()` remains null.

## API Sketch

```cpp
namespace cinerig::managers
{
class MixingCamera final : public VirtualCameraManagerBase
{
public:
    static constexpr std::size_t MaxCameras = 8;

    std::array<float, MaxCameras> weights{...};

    MixingCamera(std::string name = "MixingCamera");

    void reset();
    void validate();

    bool setWeight(int index, float weight) noexcept;
    float getWeight(int index) const noexcept;
    bool setWeight(const VirtualCamera& child, float weight) noexcept;
    float getWeight(const VirtualCamera& child) const noexcept;

    float liveChildPercent() const noexcept;

    VirtualCamera* liveChild() const override;
    bool isLiveChild(const ICinemachineCamera* child, bool dominantChildOnly) const override;
    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override;
    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
    void invalidateCameraCache() override;
};
}
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineMixingCamera.cs`
  - eight fixed weight slots, reset defaults, getters/setters, live child
    percentage, weighted state accumulation, live-child selection, child
    transition forwarding, and disabled/negative weight handling
- `com.unity.cinemachine/Runtime/Core/CinemachineCameraManagerBase.cs`
  - child cache, default targets, force/warp propagation, Finalize callback,
    and `PreviousStateIsValid`
- `com.unity.cinemachine/Runtime/Core/CameraState.cs`
  - weighted interpolation semantics

## Acceptance

- Defaults expose eight slots initialized to `0.5`; `reset()` selects the first
  slot with weight `1` and clears the rest.
- `validate()` clamps negative and non-finite weights; mixing also ignores
  negative weights.
- Weight getters/setters address the first eight direct children only; invalid
  indices and non-child cameras return `0`/`false`.
- Two enabled children with positions `(0,0,0)` and `(10,4,0)` and weights
  `1`/`3` mix to `(7.5,3,0)`; the second child is live at `75%`.
- Zero weights and disabled children do not contribute. With no contributors,
  the previous mixed state is retained and the live child is null.
- Every positively weighted enabled child reports `isLiveChild`, while only the
  highest-weight child is dominant.
- Weighted children are marked live for the update manager and unweighted
  children are cleared.
- The manager Finalize extension receives the mixed state, and the mixed state
  is pushed to the manager's owner entity.
- At most eight children contribute, even if more are registered.
- Unit/scenario tests, examples Debug/Release, smoke tests, no-tests build, and
  install pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/managers/mixing_camera.hpp` and
  `src/managers/mixing_camera.cpp`
  - eight fixed weight slots and explicit reset/validation
  - weight access by slot and by direct child
  - enabled/positive weight filtering and sequential `CameraState::lerp`
  - highest-weight live child and live percentage
  - weighted child live flags for standby updates
  - dominant and non-dominant `isLiveChild` semantics
  - Finalize extension invocation and owner transform write
  - child transition forwarding and inherited force/warp behavior
  - manager participation through the standard brain update path
- `tests/scenario/mixing_camera_test.cpp`
  - 8 cases and 106 assertions covering defaults/reset, slot and child weight
    access, validation, weighted position/lens mixing, live child and
    percentage, zero/disabled/negative weights, state retention, eight-slot
    limits, live-flag changes, Finalize/owner output, transition forwarding,
    and brain participation
- `examples/31_mixing_camera`
  - three orbiting child cameras, child/mixed frustums, weight presets,
    live-child HUD, pause/reset controls, and smoke test
- `VirtualCameraManagerBase` gains virtual `liveChild()` and
  `invalidateCameraCache()` hooks for derived manager policies.
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Children are explicitly registered through `VirtualCameraManagerBase`
  instead of discovered from a Unity Transform hierarchy.
- The manager uses a `std::array<float, 8>` instead of eight serialized named
  fields. Defaults remain `0.5` for all slots; `reset()` selects the first
  slot.
- Unity component enable/disable and editor Reset/OnValidate callbacks are not
  ported. `validate()` is explicit and mixing itself ignores invalid weights.
- The original relies on recursive live-child checks for child updates.
  CineRig marks every positively weighted child live so the update manager's
  standby policy keeps all contributors fresh; unweighted children are
  cleared.
- Weighted mixing is immediate and does not use the manager blend manager, so
  `isBlending()` remains false and `activeBlend()` remains null.
- With no contributors, the previous mixed state is retained, matching the
  original persistent manager state.
- No Timeline integration, editor serialization, ClearShot, Sequencer, or
  StateDriven policy is included.

### Verification

- `[mixing]`: **8 cases / 106 assertions passed**.
- Core Debug and Release: **408/408 passed**
  (319 unit + 83 scenario + 6 golden).
- Examples Debug and Release: **454/454 passed**
  (408 core + 46 example tests, including 31 smoke tests).
- `cinerig_31_mixing_camera --frames 180 --hidden --no-vsync` exits 0 in
  Debug and Release. The no-tests Debug build exits 0 at 2 frames.
- Install includes `cinerig/managers/mixing_camera.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, reset, and configured a
  `managers::MixingCamera`.
- Visible overview shows orange/cyan/green child frustums, the magenta mixed
  frustum, green target, weight HUD, live child, and percentage. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_mixing_camera.png`.
- Windows was verified; Linux was not tested on hardware.
