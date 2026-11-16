# CineRig Architecture

This document is a short companion to [plan.md](../plan.md). It records the
module boundaries and dependency rules that every task must preserve.

## Module layers

```text
examples / tests
      │
      ▼
cinerig::brain        camera selection, timeline overrides, blending, post-processing dispatch, output application
cinerig::managers     MixingCamera / ClearShot / Sequencer / StateDriven
cinerig::components   Follow / OrbitalFollow / PanTilt / ThirdPersonFollow / ThirdPersonAim / HardLockToTarget / HardLookAt / RotateWithFollowTarget / CameraOffset / Recomposer / FreeLookModifier / FollowZoom / Composers / BasicMultiChannelPerlin / Deoccluder / Decollider / Confiner3D / ConfinerOven / Confiner2D / SplineDolly / SplineCart / SplineRoll / SplineDollyLookAtTargets / SplineSmoother / PostProcessing / AutoFocus / StoryboardExtension / PixelPerfectExtension
cinerig::camera       VirtualCamera / Component / Extension / pipeline
cinerig::impulse      ImpulseManager / ImpulseDefinition / ImpulseSource / CollisionImpulseSource / ImpulseListener
cinerig::targets      TargetGroup (implementation) / GroupFraming
      │
      ▼
cinerig::runtime      CameraSystem / Registry / UpdateManager / UpdateTracker
cinerig::core         CameraState / Lens / Blend / Priority / Target / InputAxis / ModifierInterfaces / SplineSettings / SplineAutoDolly / BezierSpline
cinerig::math         vectors, quaternions, curves, damping
cinerig::engine       IEntityWorld / ICameraOutput / IPhysicsWorld / ISpline / IPostProcessor / ...
```

## Dependency rules

1. `math` depends on nothing except the C++ standard library.
2. `core` depends on `math` only.
3. `engine` depends on `core` and `math`; it must not depend on `camera`,
   `components`, `brain`, or any concrete engine.
4. `camera` depends on `core`, `math`, `engine`.
5. `components` and `targets` depend on `camera`; they must not depend on
   `brain` or `managers`.
6. `brain` depends on `camera`, `core`, `math`, `engine`.
7. `managers` depend on `camera`, `brain`, `core`.
8. `examples` and `tests` may depend on every public module.
9. The core library must never link SDL, OpenGL, GLAD, Clipper2, or GLM
   unless the optional math backend is explicitly selected.

## Runtime ownership

- `CameraSystem` owns cameras, brains, target groups, and the impulse manager.
- Callers interact through stable `uint64_t` handles (`CameraId`, `BrainId`,
  `GroupId`).
- `IEntityWorld` owns entity transforms; CineRig never owns engine objects.
- `ICameraOutput` owns the engine camera; CineRig writes the final state to it.
- `VirtualCamera` owns its pipeline components and extensions.
- `VirtualCameraManagerBase` owns references to its child cameras; it does not
  own their lifetime.
- `BlendManager` and `CameraBlendStack` own blend state and transient
  snapshot/nested-blend sources; they do not own cameras. Completed root blends
  clear their source state and snapshot so an interrupted freeze blend cannot
  reuse stale data.
- `TimelineMixer` non-owningly references an `ICameraOverrideStack` and shot
  cameras. It owns only its persistent override ID.
- `CameraBrain` non-owningly references an optional `IPostProcessor`. The host
  owns the adapter and any engine profiles referenced by custom blendables.
- `StoryboardExtension` owns its shared settings payload. The payload
  non-owningly references the host image through an opaque shared handle; the
  host overlay adapter uses the layout helper and its native image/texture.
- `PixelPerfectExtension` owns its settings and stores the last layout and
  correction for the host. The host supplies the output screen size each frame
  and owns offscreen render targets, crop bars, filtering, and camera-position
  snapping.

## Update pipeline

```text
CameraSystem::fixedUpdate / lateUpdate / manualUpdate
  → UpdateManager updates active virtual cameras (leaf-most first)
  → each VirtualCamera computes CameraState (Body → Aim → Noise → Finalize)
    and PixelPerfectExtension may correct its orthographic size at Body
  → timeline tracks submit up to two active shot cameras as override frames
  → each CameraBrain chooses a live camera and computes the blend
  → the blend stack advances, refreshes outgoing live cameras, and clears
    completed source/snapshot state
  → CameraBrain applies AppliedCamera to ICameraOutput
  → optional IPostProcessor applies blended profiles after output submission
  → optional host overlay adapter renders StoryboardSettings blendables
```

`VirtualCameraRegistry` caches its priority-sorted active list and re-sorts only
when the priority/activation signature changes or the active set is modified.
`UpdateManager` copies its level table into reusable scratch storage, and
`BlendManager` reuses its live-camera scratch buffers, so a warmed camera frame
does not allocate in optimized builds.

## Determinism

- All time comes from `IEntityWorld`; no `std::chrono` in the core library.
- `manualUpdate` must be deterministic for a fixed sequence of inputs.
- Noise and impulse use explicit seeds, not global random state.
- The library is single-threaded. An adapter may run multiple `CameraSystem`
  instances on different threads, but a single system is not thread-safe.

## Error handling

- Programmer errors use assertions in debug builds.
- Invalid targets/entities are treated as "not valid" and must not crash.
- Optional adapters (physics, splines, input, post-processing) have null
  implementations so callers do not need pervasive null checks.

## Testing

- `unit` covers pure math, core types, and component-level behavior.
- `scenario` covers CameraSystem, Brain, manager, pipeline, and integration
  behavior.
- `golden` compares deterministic JSON snapshots.
- `parity` transcribes representative Unity Cinemachine test expectations;
  see [parity.md](parity.md).
- `performance` measures steady-state camera frames, state blending, and spline
  evaluation, and enforces a zero-allocation contract in Release; see
  [performance.md](performance.md).
- AddressSanitizer runs the core suites in a separate build tree; timing tests
  are excluded because instrumentation distorts the budgets.
- Run everything with `ctest`, or a suite with `ctest -L <label>`. Release
  status and known platform gaps are tracked in
  [release-readiness.md](release-readiness.md).

## Serialization / editor

- CineRig does not implement Unity-style serialization, inspectors, handles,
  presets, or SaveDuringPlay.
- A host engine may serialize the public configuration structs directly.
- Debug visualization is an example/adapter concern.
