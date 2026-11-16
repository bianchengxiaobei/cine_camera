# P4-11 SequencerCamera

Status: completed  
Depends on: P1-04  
Blocks: P4-12 StateDriven

## Goal

Port `CinemachineSequencerCamera`. This manager plays an ordered instruction
list, holding each child camera for a configured time and blending to the next
camera with the next instruction's blend definition. The last camera is held
indefinitely unless looping is enabled.

## Scope

- Add `managers::SequencerCamera`:
  - ordered `Instruction` entries containing a child camera, blend-in
    definition, and hold time
  - loop flag and last-instruction hold semantics
  - activation-time/current-instruction state machine
  - hold plus blend duration scheduling
  - transition restart at the first instruction
  - negative-delta and invalid-state restart
  - instruction bounds, null/detached camera, empty-child, and empty-list
    handling
  - instruction blend lookup and base blend-manager integration
  - explicit `reset()` and `validate()`
  - current-instruction inspection for hosts and tests
- Add scenario tests for defaults/reset, instruction validation, hold timing,
  blend-in selection, last-shot hold, looping, malformed lists, disabled and
  detached children, transitions, negative deltas, list shrinking, and blend
  output.
- Add a `33_sequencer` SDL example with three shots, per-instruction blends and
  hold times, automatic cycling, loop/timing controls, sequence HUD, and a
  smoke test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity legacy look-at/follow upgrade fields and SaveDuringPlay.
- Editor inspectors, `ChildCameraProperty` enforcement, hierarchy discovery,
  and component enable callbacks.
- Timeline clips and playable assets.
- StateDriven camera policy.
- Multiple instruction advances in a single update. The original advances at
  most once per update; CineRig preserves that behavior.
- New blend curves or blend-manager behavior.

## Design

```text
managers::SequencerCamera : VirtualCameraManagerBase
  loop
  std::vector<Instruction> instructions
  activationTime / currentInstruction

  chooseCurrentCamera(worldUp, dt)
    invalidate current instruction when previous state is invalid
    reject no children or no instructions
    restart at instruction 0 on invalid state or negative delta
    clamp an out-of-range current instruction to the last entry
    holdTime = instruction.Hold + instruction.Blend.BlendTime
    minHold = 0 if another instruction or looping, else +infinity
    if now - activationTime > max(minHold, holdTime):
      advance exactly one instruction, wrapping when looping
    return instruction camera only if it is a direct child

  lookupBlend(outgoing, incoming)
    return instructions[currentInstruction].Blend when valid
    otherwise fall back to the manager default blend lookup
```

The blend stored on an instruction is its blend-in definition. The current
instruction is held for its own `Hold` plus its blend duration, then the next
instruction supplies the blend used for the transition. This matches the
original sequencing model and editor labeling. Comparisons are strict (`>`).

Instruction ordering is the source of truth. The manager does not silently
skip null, detached, or unregistered cameras: such an instruction produces no
live camera while still consuming its hold interval. Disabled direct children
are activated by the base manager, matching the original auto-activation.

`currentInstruction()` returns `-1` when no sequence is active; otherwise it
returns the index of the instruction currently being evaluated. A successful
transition or reset restarts the index at zero.

A sequencer needs an activation time before it can start. A brain activation
supplies it through `onTransitionFromCamera()`, as does a direct host call.
The original manager behaves the same way; a plain first update after reset
intentionally does not start the sequence.

## API Sketch

```cpp
namespace cinerig::managers
{
class SequencerCamera final : public VirtualCameraManagerBase
{
public:
    struct Instruction
    {
        VirtualCamera* camera = nullptr;
        BlendDefinition blend{BlendStyle::Cut, 0.0f};
        float hold = 0.0f;

        void validate();
    };

    bool loop = false;
    std::vector<Instruction> instructions;

    explicit SequencerCamera(std::string name = "SequencerCamera");

    void reset();
    void validate();
    int currentInstruction() const noexcept;

    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
    BlendDefinition lookupBlend(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming) const override;

private:
    void advanceCurrentInstruction(float deltaTime);
    void clearSequenceState();

    float activationTime_ = -1.0f;
    int currentInstruction_ = 0;
};
}
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineSequencerCamera.cs`
  - instruction list, loop flag, activation/current-instruction state machine,
    hold plus blend-time scheduling, current-instruction blend lookup, and
    transition/restart behavior
- `com.unity.cinemachine/Runtime/Core/CinemachineCameraManagerBase.cs`
  - child cache, previous-state invalidation, live child, blend manager, and
    auto-activation of disabled direct children
- `com.unity.cinemachine/Documentation~/CinemachineSequencerCamera.md`
  - user-facing sequencing, last-shot hold, and looping semantics
- `com.unity.cinemachine/Editor/Editors/CinemachineSequencerCameraEditor.cs`
  - blend-in and hold field presentation

## Acceptance

- Defaults are non-looping with an empty instruction list; instruction defaults
  are a null camera, Cut blend, and zero hold.
- `validate()` clamps negative and non-finite holds on every instruction.
- `reset()` clears instructions and loop, restores manager defaults, and
  invalidates the camera state.
- With a zero blend, a child is held until elapsed time is strictly greater
  than its hold; the next instruction then becomes live.
- Scheduling uses the current instruction's hold plus blend duration.
- The blend used when entering an instruction is that instruction's blend
  definition.
- The last instruction is held indefinitely when not looping, even after its
  hold expires.
- Looping wraps from the last instruction to the first and continues.
- Empty child lists, empty instruction lists, null instructions, and
  unregistered cameras produce no live child without entering invalid indices.
- Disabled direct-child cameras are auto-enabled when selected.
- `onTransitionFromCamera()` and negative delta time restart the sequence at
  instruction zero.
- A transition notification starts a fresh or reset sequence; brain activation
  supplies it automatically.
- Shrinking an instruction list clamps the current index to the last entry.
- At most one instruction advance occurs per update.
- The manager participates in the normal blend path and reports blending and
  live-child state.
- Unit/scenario tests, examples Debug/Release, smoke tests, no-tests build, and
  install pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/managers/sequencer_camera.hpp` and
  `src/managers/sequencer_camera.cpp`
  - instruction vector with camera, blend-in definition, and hold values
  - activation/current-instruction state machine and explicit reset/validate
  - strict hold plus blend-duration scheduling
  - last-instruction hold and loop wrap behavior
  - transition and negative-delta restart
  - instruction bounds, malformed-list, null, and detached-camera handling
  - instruction blend lookup with a safe manager-default fallback
  - disabled direct-child auto-activation through the manager base
  - `currentInstruction()` inspection
- `tests/scenario/sequencer_camera_test.cpp`
  - 9 cases and 88 assertions covering defaults/reset, instruction validation,
    transition start, strict hold timing, current-instruction blend-in,
    last-shot hold, looping, one-step advancement, transitions, negative
    deltas, malformed lists, disabled/orphan cameras, list shrinking, and
    brain-driven activation
- `examples/33_sequencer`
  - overview scene with three colored shots around a moving target
  - per-instruction holds and Cut/EaseInOut/Linear/EaseIn blends
  - loop, time-scale, pause, and restart controls
  - live instruction, shot, timing, and loop HUD
  - in-application framebuffer capture and smoke test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Children are explicitly registered through `VirtualCameraManagerBase`, and
  instructions reference those child pointers directly instead of Unity
  `Transform` hierarchies and `ChildCameraProperty` metadata.
- Unity's default instruction blend is `Cut`; CineRig's `BlendDefinition`
  default is `EaseInOut`, so `Instruction::blend` is explicitly initialized to
  `Cut` to preserve the original default.
- A null, detached, or unregistered camera yields no live camera for that
  instruction while still consuming its hold interval. Unity relies on editor
  restrictions and unguarded indices; CineRig validates bounds.
- Invalid instruction indices fall back to the manager's normal default/custom
  blend lookup instead of indexing out of range.
- Sequence activation still requires a transition notification, matching the
  original activation-time sentinel. CineRig brain activation calls it
  automatically; the overview-based example starts the manager directly.
- Unity editor callbacks, `SaveDuringPlay`, legacy target upgrade fields,
  Timeline, and StateDriven policy are not ported.
- At most one instruction advances per update, matching the original.

### Verification

- `[sequencer]`: **9 cases / 88 assertions passed**.
- Core Debug and Release: **424/424 passed**
  (319 unit + 99 scenario + 6 golden).
- Examples Debug and Release: **472/472 passed**
  (424 core + 48 example tests, including 33 smoke tests).
- `cinerig_33_sequencer --frames 2000 --hidden --no-vsync --capture ...`
  exits 0 in Debug; `--frames 180` exits 0 in Debug and Release. The no-tests
  Debug build exits 0 at 2 frames.
- Install includes `cinerig/managers/sequencer_camera.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, validated, and reset a
  `managers::SequencerCamera`.
- The captured view at `t=3.297` shows instruction 2/3, Shot B live with the
  thick cyan frustum, looping enabled, the moving target, and the other two
  standby shots. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_sequencer.png`.
- Windows was verified; Linux was not tested on hardware.
