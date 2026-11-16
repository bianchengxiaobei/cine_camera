# P4-12 StateDrivenCamera

Status: completed  
Depends on: P1-04, P0-05  
Blocks: P5

## Goal

Port `CinemachineStateDrivenCamera`. This manager maps opaque animation-state
hashes to child cameras and activates the highest-priority enabled camera whose
state instruction matches the animated target's current or incoming state.
Instruction-level activation delays and minimum durations filter short states
and prevent rapid camera churn.

## Scope

- Add `managers::StateDrivenCamera`:
  - animated target entity, layer index, and non-owning animation-state
    provider
  - state instructions containing a full hash, child camera, activation delay,
    and minimum duration
  - current-state selection and incoming-state selection during transitions
  - parent-hash fallback for nested or pseudo states
  - per-state duplicate instructions with priority ranking and earliest-list
    tie-breaking
  - first-child fallback when the target, provider, layer, or current state is
    unavailable or unmapped
  - active/pending instruction state machine with `activateAfter` and
    `minDuration`
  - pending-wait cancellation
  - active/pending instruction inspection for hosts and tests
  - explicit `reset()` and `validate()`
- Extend `IAnimationStateProvider` with a transition query whose default
  implementation treats a non-zero next-state hash as a transition. Existing
  providers remain source-compatible.
- Add scenario tests for defaults/reset, priority, enabled/direct-child
  filtering, transition-state selection, parent fallback, fallback behavior,
  timing, cancellation, malformed state lists, and brain participation.
- Add a `34_state_driven` SDL example with a simulated animation state machine,
  four mapped cameras, transition/wait HUD, manual state controls, and a smoke
  test.
- Update the task index, feature matrix, architecture notes, and example index.

## Non-goals

- Unity `Animator`, `RuntimeAnimatorController`, and `AnimatorClipInfo`
  integration. A host adapter implements `IAnimationStateProvider`.
- Synthesizing animation clip pseudo-hashes. Providers may return such hashes
  as opaque state identifiers and expose their parent relationships through
  `parentHashes`.
- Editor state scraping, `ChildCameraProperty` enforcement, SaveDuringPlay,
  and legacy look-at/follow upgrade fields.
- Timeline and new blend curves.
- Sub-frame state sampling. The provider is queried once per manager update.

## Design

```text
managers::StateDrivenCamera : VirtualCameraManagerBase
  animatedTarget / layerIndex / animationState
  instructions: { fullHash, camera, activateAfter, minDuration }
  parentHashes: { hash, parentHash }
  activationTime / activeInstruction
  pendingActivationTime / pendingInstruction

  chooseCurrentCamera(worldUp, dt)
    reset activation/pending timing when previous state is invalid
    reject no children
    fallback = first child
    reject missing target/provider, negative layer, or missing target entity
    clamp stale active/pending instruction indices
    hash = provider transition ? next state : current state
    while hash has no instruction:
      hash = parent hash, or 0 when absent
    best = highest-priority enabled direct child instruction for hash
    if no active camera:
      activate best immediately
    else if best differs from active and is not already pending:
      begin pending wait for best
    if pending waited > pending.activateAfter
       and active elapsed > active.minDuration:
      activate pending
    return active camera, or fallback when none is active
```

The active and pending instruction indices are independent. A pending switch is
not discarded merely because the animation state changes again; this preserves
the original behavior. `cancelWait()` promotes a pending instruction
immediately. Timing comparisons are strict (`>`).

`activationTime == 0` is the original "no active instruction" sentinel. Tests
and hosts that need immediate activation on the first update should run with a
non-zero world time.

Parent-hash traversal is cycle-guarded. If a traversal cannot reach a mapped
hash, the hash resolves to `0`, allowing a default instruction at hash `0` or
the first-child fallback.

## API Sketch

```cpp
namespace cinerig::managers
{
class StateDrivenCamera final : public VirtualCameraManagerBase
{
public:
    struct Instruction
    {
        int fullHash = 0;
        VirtualCamera* camera = nullptr;
        float activateAfter = 0.0f;
        float minDuration = 0.0f;

        void validate();
    };

    struct ParentHash
    {
        int hash = 0;
        int parentHash = 0;
    };

    EntityId animatedTarget = 0;
    int layerIndex = 0;
    IAnimationStateProvider* animationState = nullptr;
    std::vector<Instruction> instructions;
    std::vector<ParentHash> parentHashes;

    explicit StateDrivenCamera(std::string name = "StateDrivenCamera");

    void reset();
    void validate();
    void cancelWait();

    int activeInstruction() const noexcept;
    int pendingInstruction() const noexcept;
    bool isWaiting() const noexcept;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
};
}
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineStateDrivenCamera.cs`
  - instruction hashes, parent lookup, priority selection, active/pending
    state machine, transition-state selection, fallback camera, and
    `CancelWait`
- `com.unity.cinemachine/Runtime/Core/CinemachineCameraManagerBase.cs`
  - explicit child management, auto-activation, default/custom blends, and
    previous-state invalidation
- `com.unity.cinemachine/Tests/Runtime/StateDrivenCameraTests.cs`
  - state changes drive the live child and camera switches back on state
    reversal
- `com.unity.cinemachine/Documentation~/CinemachineStateDrivenCamera.md`
  - state mapping, default fallback, priority tie-breaking, wait, and minimum
    duration semantics
- `CineRig/include/cinerig/engine/animation_state.hpp`
  - host animation-state adapter

## Acceptance

- Defaults are an unset target/provider, layer `0`, and empty instruction and
  parent lists.
- `validate()` clamps negative and non-finite instruction delays and minimum
  durations.
- `reset()` clears target/provider/layer/instructions/parents, restores manager
  defaults, and invalidates camera state.
- With no children, the manager has no live child. With children but no valid
  target/provider, the first child is the fallback.
- A mapped current state activates its instruction immediately when no active
  instruction exists.
- During a provider transition, the incoming state hash is used instead of the
  current state hash.
- Duplicate instructions for a state select the highest-priority enabled direct
  child; equal priorities select the earliest instruction.
- Unmapped states traverse parent hashes up to a mapped ancestor or hash zero.
  Parent cycles terminate safely.
- An unmapped state with no active instruction returns the first child; it does
  not disrupt an already-active instruction.
- A pending instruction waits for its `activateAfter` and the active
  instruction's `minDuration`, using strict comparisons.
- `cancelWait()` promotes a valid pending instruction immediately.
- Instructions with null or detached cameras are ignored during ranking.
- Active/pending indices are clamped when the instruction list shrinks.
- The manager participates in the standard blend path and brain selection.
- Unit/scenario tests, examples Debug/Release, smoke tests, no-tests build, and
  install pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/managers/state_driven_camera.hpp` and
  `src/managers/state_driven_camera.cpp`
  - animated target entity, layer, and non-owning provider configuration
  - state instructions and parent-hash mappings
  - current/incoming state selection with default transition detection
  - cycle-safe parent fallback and hash-zero default instructions
  - enabled direct-child priority selection with earliest-instruction ties
  - first-child fallback for missing target/provider/state mappings
  - active/pending activation-delay and minimum-duration state machine
  - pending-wait cancellation and active/pending inspection
  - safe stale/destroyed instruction-camera handling
  - explicit reset and instruction validation
- `IAnimationStateProvider` gains a non-pure `isInTransition()` query whose
  default treats a non-zero next-state hash as a transition.
- `tests/scenario/state_driven_camera_test.cpp`
  - 11 cases and 99 assertions covering defaults/reset, fallback behavior,
    priority and ties, disabled/orphan/null cameras, incoming transition
    states, parent chains, cycles, hash-zero defaults, timing, cancellation,
    unmapped states, list shrinking, destroyed children, and brain updates
- `examples/34_state_driven`
  - simulated animation state machine with Idle/Walk/Run/Jump transitions
  - four mapped shots, auto cycling, manual state keys, speed/pause controls,
    pending-wait cancellation, state/rule HUD, and a smoke test
- Build/export wiring, task index, feature matrix, architecture, and example
  index updated.

### Differences

- Unity `Animator` and `AnimatorClipInfo` are replaced by
  `IAnimationStateProvider` and opaque hashes. State hashes may already
  represent pseudo clip sub-states; CineRig does not synthesize them.
- The animated target is an `EntityId`, and the provider is a non-owning host
  service configured on the manager.
- Parent hashes are an explicit public list rather than hidden editor
  serialization. Traversal is cycle-guarded, unlike the original loop.
- Controller binding, layer-count validation, and active-self checks are
  delegated to the host provider. A negative layer and a missing world entity
  use the first-child fallback.
- Instruction cameras are matched against the current direct-child list instead
  of dereferencing potentially stale serialized references after destruction.
- The original zero activation-time sentinel is preserved, so immediate
  activation requires a non-zero world time.
- Pending instructions are not cancelled merely because the state changes
  again, matching the original behavior. `cancelWait()` is the explicit bypass.
- Unity editor callbacks, `ChildCameraProperty`, SaveDuringPlay, legacy target
  upgrade fields, Timeline, and new blend curves are not ported.

### Verification

- `[statedriven]`: **11 cases / 99 assertions passed**.
- Core Debug and Release: **435/435 passed**
  (319 unit + 110 scenario + 6 golden).
- Examples Debug and Release: **484/484 passed**
  (435 core + 49 example tests, including 34 smoke tests).
- `cinerig_34_state_driven --frames 2000 --hidden --no-vsync --capture ...`
  exits 0 in Debug; `--frames 180` exits 0 in Debug and Release. The no-tests
  Debug build exits 0 at 2 frames.
- Install includes `cinerig/managers/state_driven_camera.hpp`. An external
  `find_package(CineRig)` consumer compiled, linked, validated, and reset a
  `managers::StateDrivenCamera`.
- The captured view at frame 2000 shows `Walk`, active instruction 2/4, Shot B
  live with the thick orange frustum, auto cycling enabled, and all four mapped
  shots. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p4_state_driven.png`.
- Windows was verified; Linux was not tested on hardware.
