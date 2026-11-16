# Runtime Parity Follow-Up

Status: complete (P6-01 through P6-06 complete)
Reference: Unity Cinemachine 3.1.x
Depends on: P0-P5 runtime feature work
Related: [release-readiness.md](release-readiness.md), [feature-matrix.md](feature-matrix.md)

## Purpose

P0-P5 delivered the engine-neutral runtime surface and the planned camera
features. A source-level comparison against the reference runtime found six
behavior gaps that are not Unity-editor integration work. P6-01 through P6-06
close all six:

1. shot-quality evaluation was only available inside Deoccluder (closed by
   P6-01);
2. Confiner2D/ConfinerOven did not implement skeleton padding or incremental
   baking (closed by P6-02);
3. GroupFraming did not enter the reference Confiner2D compatibility mode
   (closed by P6-03);
4. TargetGroup did not implement update-method, frame-cache, or owner-transform
   write-back semantics (closed by P6-04);
5. the fixed signal source and external impulse listener helpers were missing
   (closed by P6-05);
6. `BrainConfig::ignoreTimeScale` and the impulse time-scale option were not
   wired to an unscaled world clock (closed by P6-06).

This document is the roadmap. Each scheduled item has a separate implementation
contract under `docs/tasks/P6-*.md`. Items that should stay outside the core
runtime are recorded in the deferred table instead of being turned into empty
task documents.

The follow-up is not a release blocker for CineRig 0.1 unless the release scope
is expanded. Linux validation remains tracked in
[release-readiness.md](release-readiness.md).

## Gap Summary

| Priority | Gap | Reference | Current CineRig state | Task |
| --- | --- | --- | --- | --- |
| P1 | Shot-quality evaluator | `Runtime/Behaviours/CinemachineShotQualityEvaluator.cs`, `Runtime/Core/IShotQualityEvaluator.cs` | Standalone evaluator, marker, scenario coverage, and ClearShot example integration landed | [P6-01](tasks/P6-01-shot-quality-evaluator.md) (completed) |
| P1 | ConfinerOven skeleton and padding | `Runtime/Core/ConfinerOven.cs` | Incremental bake cache, skeleton extraction, normalized padding, oversize union, progress/timeout, and cache invalidation landed | [P6-02](tasks/P6-02-confiner2d-fidelity.md) (completed) |
| P1 | GroupFraming + Confiner2D compatibility | `Runtime/Behaviours/CinemachineGroupFraming.cs` | Body-stage selection, enabled-extension detection, and orthographic lens-cache invalidation landed | [P6-03](tasks/P6-03-group-framing-confiner2d.md) (completed) |
| P2 | TargetGroup update semantics | `Runtime/Behaviours/CinemachineTargetGroup.cs` | Fixed/Late update method, per-frame cache, lazy accessor refresh, and owner-transform write-back landed | [P6-04](tasks/P6-04-target-group-update.md) (completed) |
| P2 | Impulse helper coverage | `Runtime/Impulse/CinemachineFixedSignal.cs`, `Runtime/Impulse/CinemachinExternalImpulseListener.cs` | Raw curve evaluation, `FixedSignal`, and the host-driven `ExternalImpulseListener` with read-only offset exposure landed | [P6-05](tasks/P6-05-impulse-helpers.md) (completed) |
| P3 | Ignore time scale | `Runtime/Behaviours/CinemachineBrain.cs`, `Runtime/Impulse/CinemachineImpulseManager.cs` | World unscaled delta/time hooks, Brain effective-delta precedence and single application, fixed-clock selection, and impulse unscaled clock landed | [P6-06](tasks/P6-06-time-scale.md) (completed) |

## Planned Order

1. **P6-01 Shot Quality Evaluator** - completed; adds the standalone evaluator
   and ClearShot quality path.
2. **P6-02 Confiner2D Fidelity** - completed; adds incremental baking, skeleton
   padding, and oversize-window behavior.
3. **P6-03 GroupFraming + Confiner2D** - completed; depends on P6-02 for a
   stable lens-cache invalidation contract.
4. **P6-04 TargetGroup Update** - completed; adds clock-gated caching and
   owner-transform write-back.
5. **P6-05 Impulse Helpers** - completed; adds reusable host-facing impulse
   building blocks.
6. **P6-06 Time Scale** - completed; adds the world unscaled hooks, Brain
   effective-delta selection, and impulse unscaled time.

P6-01 was the first implementation because it is self-contained and directly
affects ClearShot selection quality. P6-02 provided the stable bake and
lens-cache behavior used by P6-03. P6-04 closed the independent TargetGroup
update/write-back gap. P6-05 closed the reusable impulse helper gap, and P6-06
closed the scaled/unscaled time gap.

## Documentation Deliverables

| Document | Purpose | Current state |
| --- | --- | --- |
| [runtime-parity.md](runtime-parity.md) | Roadmap, priorities, deferred items, verification baseline | Written |
| [P6-01 task](tasks/P6-01-shot-quality-evaluator.md) | Shot-quality evaluator contract | Implemented and verified |
| [P6-02 task](tasks/P6-02-confiner2d-fidelity.md) | Confiner2D/ConfinerOven contract | Implemented and verified |
| [P6-03 task](tasks/P6-03-group-framing-confiner2d.md) | GroupFraming/Confiner2D contract | Implemented and verified |
| [P6-04 task](tasks/P6-04-target-group-update.md) | TargetGroup update contract | Implemented and verified |
| [P6-05 task](tasks/P6-05-impulse-helpers.md) | Impulse helper contract | Implemented and verified |
| [P6-06 task](tasks/P6-06-time-scale.md) | Time-scale contract | Implemented and verified |
| [tasks/README.md](tasks/README.md) | Task index and phase status | Updated |
| [feature-matrix.md](feature-matrix.md) | Runtime/subfeature status and P6 links | Updated |
| [parity.md](parity.md) | Parity gap inventory and test mapping | Updated |
| [plan.md](../plan.md) | Roadmap and P6 phase entry | Updated |
| [README.md](../README.md) | Project status and roadmap link | Updated |
| [release-readiness.md](release-readiness.md) | Release-gate boundary for P6 | Updated |

Each task document must be updated again when implementation starts and
finishes: set `Status`, fill `Implementation Record`, add reference
differences, and record verification results.

## Deferred And Excluded

These items are intentionally not scheduled as core tasks. They remain visible
so the parity boundary is explicit.

| Item | Reference | Decision | Revisit condition |
| --- | --- | --- | --- |
| Target position cache | `Runtime/Core/TargetPositionCache.cs` | Deferred. It exists for editor Timeline scrubbing/playback and noise playback time. CineRig's runtime path uses current `IEntityWorld` transforms. | A Unity Timeline editor adapter or deterministic scrub/playback feature is in scope. |
| Solo camera | `Runtime/Core/CinemachineCore.cs` | Excluded. It is a global editor/debug override of the active camera. | A debug/editor adapter needs a solo-camera workflow. |
| Debug text, frustum display, `CinemachineDebug` | `Runtime/Debug/*`, `Runtime/Behaviours/CinemachineBrain.cs` | Excluded from the core. The example HUD is an adapter concern. | A debug visualization adapter is part of the deliverable. |
| Unity helper components | `Runtime/Helpers/CinemachineCameraEvents.cs`, `CinemachineCameraManagerEvents.cs`, `CinemachineTriggerAction.cs`, `GroupWeightManipulator.cs` | Adapter-layer work. CineRig exposes raw `BrainEvents`; UnityEvent wrappers are not core runtime. | A Unity integration package is built on top of CineRig. |
| Exact Clipper2 integer results | `Runtime/ThirdParty/clipper.cs` | Excluded from the core contract. The internal float polygon engine is the supported implementation. | Exact reference polygon bit-parity is required. |
| 2D collision callbacks | `CinemachineCollisionImpulseSource.cs`, Unity `Collider2D` integration | Adapter responsibility. The core consumes host-supplied `ImpactInfo`. | A Unity 2D physics adapter is shipped. |

## Definition Of Done

Each scheduled task is complete only when:

- the implementation and public headers are wired into CMake and
  `cinerig.hpp`;
- new unit/scenario tests cover defaults, validation, edge cases, and the
  reference behavior;
- existing core, parity, example, install, and no-tests checks remain green;
- the task document records differences from the reference and verification
  results;
- the feature matrix and task index reflect the new status;
- no new dependency is added to the core library.

## Verification Baseline

The current Windows baseline after P6-06 is:

- Core Debug/Release: **533/533** CTest;
- Examples Debug/Release: **586/586** CTest;
- unit: **354 cases / 3168 assertions**;
- scenario: **148 cases / 1188 assertions**;
- parity: **18 cases / 229 assertions**;
- performance: **7/7** cases, zero steady-state allocations;
- AddressSanitizer Release: **526/526** core tests, performance excluded;
- install/export consumer exits 0;
- `CINERIG_BUILD_TESTS=OFF` Release build passes.

P6 tasks must rerun the relevant suite and the full core suite. Performance
budgets should be rerun after any P6 task that touches the camera update path.
