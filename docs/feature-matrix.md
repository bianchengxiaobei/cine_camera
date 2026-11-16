# Feature Matrix

This matrix maps the Unity Cinemachine runtime (3.1.x) to CineRig. It is a
tracking document: update the C++ column as tasks are completed.

Status values: `planned`, `in-progress`, `done`, `excluded`.

## Core

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Camera state | `Runtime/Core/CameraState.cs` | `core::CameraState` | P0 | done |
| Lens settings | `Runtime/Core/LensSettings.cs` | `core::LensSettings` | P0 | done |
| Screen composition | `Runtime/Core/ScreenComposerSettings.cs` | `core::ScreenComposerSettings` | P0 | done |
| Lookahead settings | `Runtime/Core/LookaheadSettings.cs` | `core::LookaheadSettings` | P0 | done |
| Noise profile | `Runtime/Core/NoiseSettings.cs` | `core::NoiseProfile` | P0 | done |
| Priority | `Runtime/Core/PrioritySettings.cs` | `core::PrioritySettings` | P0 | done |
| Output channels | `Runtime/Core/OutputChannel.cs` | `core::OutputChannels` | P0 | done |
| State blending | `Runtime/Core/CameraState.cs` | `core::CameraState::lerp` | P0 | done |
| Blend definition | `Runtime/Core/CinemachineBlend.cs` | `core::BlendDefinition`, `core::Blend` | P0 | done |
| Camera interface | `Runtime/Core/ICinemachineCamera.cs` | `core::ICinemachineCamera` | P0 | done |
| Damping | `Runtime/Core/Predictor.cs` | `math::Damper` | P0 | done |
| Position prediction | `Runtime/Core/Predictor.cs` | `math::PositionPredictor` | P0 | done |
| Target tracking | `Runtime/Core/TargetTracking.cs` | `core::Tracker`, `BindingMode` | P2 | done |

## Runtime

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Camera system | `Runtime/Core/CinemachineCore.cs` | `runtime::CameraSystem` | P1 | done |
| Active camera registry | `Runtime/Core/VirtualCameraRegistry.cs` | `runtime::VirtualCameraRegistry` | P1 | done |
| Update manager | `Runtime/Core/CameraUpdateManager.cs` | `runtime::UpdateManager` | P1 | done |
| Update tracker | `Runtime/Core/UpdateTracker.cs` | `runtime::UpdateTracker` | P1 | done |
| Target position cache | `Runtime/Core/TargetPositionCache.cs` | `runtime::TargetPositionCache`（运行时路径） | P1 | planned |
| Virtual camera base | `Runtime/Core/CinemachineVirtualCameraBase.cs` | `camera::VirtualCamera` | P1 | done |
| Component base | `Runtime/Core/CinemachineComponentBase.cs` | `camera::CameraComponent` | P1 | done |
| Extension base | `Runtime/Core/CinemachineExtension.cs` | `camera::CameraExtension` | P1 | done |
| Pipeline | `Runtime/Behaviours/CinemachineCamera.cs` | `camera::Pipeline` | P1 | done |

## Brain and blending

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Brain | `Runtime/Behaviours/CinemachineBrain.cs` | `brain::CameraBrain` | P1 | done |
| Blend manager | `Runtime/Core/BlendManager.cs` | `brain::BlendManager` | P1 | done |
| Override stack | `Runtime/Core/CameraBlendStack.cs` | `brain::CameraBlendStack` | P1 | done |
| Custom blend rules | `Runtime/Core/CinemachineBlenderSettings.cs` | `brain::BlendLookup` | P1 | done |
| Brain events | `Runtime/Core/CinemachineCore.cs` | `brain::BrainEvents` | P1 | done |
| External camera | `Runtime/Behaviours/CinemachineExternalCamera.cs` | `camera::ExternalCamera` | P1 | done |
| Manager base | `Runtime/Core/CinemachineCameraManagerBase.cs` | `camera::VirtualCameraManagerBase` | P1 | done |
| Brain ignore time scale | `Runtime/Behaviours/CinemachineBrain.cs` | `BrainConfig::ignoreTimeScale` + `IEntityWorld` unscaled hooks | P6 | done |

## 3D procedural cameras

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Follow | `Runtime/Components/CinemachineFollow.cs` | `components::Follow` | P1 minimal / P2 full | done |
| Orbital follow | `Runtime/Components/CinemachineOrbitalFollow.cs` | `components::OrbitalFollow` | P2 | done |
| Position composer | `Runtime/Components/CinemachinePositionComposer.cs` | `components::PositionComposer` | P2 | done |
| Rotation composer | `Runtime/Components/CinemachineRotationComposer.cs` | `components::RotationComposer` | P1 minimal / P2 full | done |
| Pan tilt | `Runtime/Components/CinemachinePanTilt.cs` | `components::PanTilt` | P2 | done |
| Third person follow | `Runtime/Components/CinemachineThirdPersonFollow.cs` | `components::ThirdPersonFollow` | P2 | done |
| Third person aim | `Runtime/Behaviours/CinemachineThirdPersonAim.cs` | `components::ThirdPersonAim` | P2 | done |
| Hard lock to target | `Runtime/Components/CinemachineHardLockToTarget.cs` | `components::HardLockToTarget` | P2 | done |
| Hard look at | `Runtime/Components/CinemachineHardLookAt.cs` | `components::HardLookAt` | P2 | done |
| Rotate with follow target | `Runtime/Components/CinemachineRotateWithFollowTarget.cs` | `components::RotateWithFollowTarget` | P2 | done |
| Camera offset | `Runtime/Behaviours/CinemachineCameraOffset.cs` | `components::CameraOffset` | P2 | done |
| Recomposer | `Runtime/Behaviours/CinemachineRecomposer.cs` | `components::Recomposer` | P2 | done |
| Follow zoom | `Runtime/Behaviours/CinemachineFollowZoom.cs` | `components::FollowZoom` | P3 | done |
| FreeLook modifier | `Runtime/Behaviours/CinemachineFreeLookModifier.cs` | `components::FreeLookModifier` | P2 | done |

## Input and targets

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Input axis | `Runtime/Core/InputAxis.cs` | `core::InputAxis` | P2 | done |
| Input axis controller | `Runtime/Core/InputAxisControllerBase.cs` | `core::InputAxisController` | P2 | done |
| Target group | `Runtime/Behaviours/CinemachineTargetGroup.cs` | `targets::TargetGroup` | P2 | done |
| Target group update/write-back | `Runtime/Behaviours/CinemachineTargetGroup.cs` | `targets::TargetGroup::update()` | P6 | done |
| Group framing | `Runtime/Behaviours/CinemachineGroupFraming.cs` | `targets::GroupFraming` | P3 | done |
| Group framing + Confiner2D | `Runtime/Behaviours/CinemachineGroupFraming.cs` | `ICameraLensCacheInvalidator` integration | P6 | done |

## Noise, impulse, collision

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Perlin noise | `Runtime/Components/CinemachineBasicMultiChannelPerlin.cs` | `components::BasicMultiChannelPerlin` | P3 | done |
| Gaussian filter | `Runtime/Core/GaussianFilter.cs` | `math::GaussianWindow1D` | P0 | done |
| Impulse manager | `Runtime/Impulse/CinemachineImpulseManager.cs` | `impulse::ImpulseManager` | P3 | done |
| Impulse ignore time scale | `Runtime/Impulse/CinemachineImpulseManager.cs` | `impulse::ImpulseManager::ignoreTimeScale` | P6 | done |
| Impulse definition | `Runtime/Impulse/CinemachineImpulseDefinition.cs` | `impulse::ImpulseDefinition` | P3 | done |
| Impulse source | `Runtime/Impulse/CinemachineImpulseSource.cs` | `impulse::ImpulseSource` | P3 | done |
| Fixed signal source | `Runtime/Impulse/CinemachineFixedSignal.cs` | `impulse::FixedSignal` | P6 | done |
| Impulse listener | `Runtime/Impulse/CinemachineImpulseListener.cs` | `impulse::ImpulseListener` | P3 | done |
| External impulse listener | `Runtime/Impulse/CinemachinExternalImpulseListener.cs` | `impulse::ExternalImpulseListener` | P6 | done |
| Collision impulse source | `Runtime/Impulse/CinemachineCollisionImpulseSource.cs` | `impulse::CollisionImpulseSource` | P3 | done |
| Impulse filtering | `Runtime/Impulse/CinemachineImpulseListener.cs` | `impulse::ImpulseListener` combination modes | P3 | done |
| Deoccluder | `Runtime/Behaviours/CinemachineDeoccluder.cs` | `components::Deoccluder` | P3 | done |
| Decollider | `Runtime/Behaviours/CinemachineDecollider.cs` | `components::Decollider` | P3 | done |
| Confiner 3D | `Runtime/Behaviours/CinemachineConfiner3D.cs` | `components::Confiner3D` | P3 | done |
| Shot quality marker | `Runtime/Core/IShotQualityEvaluator.cs` | `core::IShotQualityEvaluator` | P6 | done |
| Shot quality evaluator | `Runtime/Behaviours/CinemachineShotQualityEvaluator.cs` | `components::ShotQualityEvaluator` | P6 | done |

## 2D, splines, managers

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Confiner 2D | `Runtime/Behaviours/CinemachineConfiner2D.cs` | `components::Confiner2D` | P4 | done |
| Confiner 2D skeleton/padding | `Runtime/Core/ConfinerOven.cs` | `ConfinerOven` incremental skeleton/padding | P6 | done |
| Confiner oven | `Runtime/Core/ConfinerOven.cs` | `components::ConfinerOven` | P4 | done |
| Spline abstractions | `Runtime/Core/SplineSettings.cs`, `SplineHelpers.cs` | `engine::ISpline`, `core::BezierSpline`, `core::SplineHelpers`, `core::SplineSettings` | P4 | done |
| Spline dolly | `Runtime/Components/CinemachineSplineDolly.cs`, `Runtime/Core/SplineAutoDolly.cs` | `components::SplineDolly`, `core::SplineAutoDolly` | P4 | done |
| Spline cart | `Runtime/Behaviours/CinemachineSplineCart.cs` | `components::SplineCart` | P4 | done |
| Spline roll | `Runtime/Behaviours/CinemachineSplineRoll.cs` | `components::SplineRoll` | P4 | done |
| Spline dolly look-at | `Runtime/Components/CinemachineSplineDollyLookAtTargets.cs` | `components::SplineDollyLookAtTargets` | P4 | done |
| Spline smoother | `Runtime/Helpers/CinemachineSplineSmoother.cs` | `components::SplineSmoother` | P4 | done |
| Mixing camera | `Runtime/Behaviours/CinemachineMixingCamera.cs` | `managers::MixingCamera` | P4 | done |
| Clear shot | `Runtime/Behaviours/CinemachineClearShot.cs` | `managers::ClearShot` | P4 | done |
| Sequencer camera | `Runtime/Behaviours/CinemachineSequencerCamera.cs` | `managers::SequencerCamera` | P4 | done |
| State-driven camera | `Runtime/Behaviours/CinemachineStateDrivenCamera.cs` | `managers::StateDrivenCamera` | P4 | done |

## Integration and excluded

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Storyboard | `Runtime/Behaviours/CinemachineStoryboard.cs` | `components::StoryboardExtension` + adapter | P5 | done |
| Pixel perfect | `Runtime/Behaviours/CinemachinePixelPerfect.cs` | `components::PixelPerfectExtension` | P5 | done |
| Post-processing | `Runtime/PostProcessing/*` | `PostProcessProfile` + `PostProcessing` / `AutoFocus` + `IPostProcessor` | P5 | done |
| Timeline | `Runtime/Timeline/*` | `ICameraOverrideStack` + `TimelineMixer` adapter | P5 | done |
| Unity Editor tools | `Editor/*` | none | - | excluded |
| SaveDuringPlay | `Runtime/SaveDuringPlay/*` | none | - | excluded |
| CM2 deprecated | `Runtime/Deprecated/*` | none | - | excluded |
| Unity Input System | package integration | `engine::IInputSource` | P5 | adapter only |
| Unity Splines asset | package integration | `engine::ISpline` | P4 | adapter only |
| Solo camera / debug HUD | `Runtime/Core/CinemachineCore.cs`, `Runtime/Debug/*` | none | - | excluded |
| Unity helper components | `Runtime/Helpers/*` | host adapters | - | adapter only |

## P6 Runtime Parity Follow-Up

| Task | Scope | Status |
| --- | --- | --- |
| [P6-01](tasks/P6-01-shot-quality-evaluator.md) | Shot quality evaluator | done |
| [P6-02](tasks/P6-02-confiner2d-fidelity.md) | Confiner2D/ConfinerOven skeleton, padding, bake budget | done |
| [P6-03](tasks/P6-03-group-framing-confiner2d.md) | GroupFraming + Confiner2D lens-cache compatibility | done |
| [P6-04](tasks/P6-04-target-group-update.md) | TargetGroup update method, cache, transform write-back | done |
| [P6-05](tasks/P6-05-impulse-helpers.md) | Fixed signal source and external impulse listener | done |
| [P6-06](tasks/P6-06-time-scale.md) | Brain and impulse ignore-time-scale semantics | done |

`TargetPositionCache` is deferred: it is only needed for editor Timeline
scrub/playback, not for the current runtime camera path. See
[runtime-parity.md](runtime-parity.md) for deferred items and the recommended
order.

## Notes

- P1 implements only a minimal WorldSpace Follow and a basic RotationComposer
  for the vertical slice. P2-01 completed the Tracker/BindingMode foundation and
  the full Follow; P2-02 completed the full screen-space RotationComposer.
  P2-03 completed the full post-Aim PositionComposer. P2-04 completed
  OrbitalFollow and InputAxis. P2-05 completed InputAxisController and
  OrbitalFollow axis discovery. P2-06 completed PanTilt. P2-07 completed
  ThirdPersonFollow. P2-08 completed ThirdPersonAim. P2-09 completed
  TargetGroup. P2-10 through P2-15 completed HardLockToTarget, HardLookAt,
  RotateWithFollowTarget, CameraOffset, Recomposer, and FreeLookModifier.
  All planned P2 features are implemented. P3-01 completed FollowZoom and the
  target-attachment damping helpers shared with framing work. P3-02 completed
  GroupFraming, including perspective/orthographic framing and view-space group
  bounds. P3-03 completed BasicMultiChannelPerlin with deterministic re-seeding,
  Unity-to-CineRig noise-axis conversion, and FreeLook noise modulation.
  P3-04 completed impulse envelopes, manager events, modern/legacy definitions,
  source helpers, listener filtering, camera-space application, and secondary noise.
  P3-05 completed host-fed 3D collision/trigger impulse filtering, impact
  direction, mass/speed scaling, and body-velocity fallback. P3-06 completed
  Deoccluder line-of-sight resolution, collision strategies, smoothing/damping,
  displacement reporting, and shot quality. P3-07 completed Decollider terrain
  resolution, camera-overlap push-out, distance smoothing, and damping.
  P3-08 completed Confiner3D closest-point containment, slowing, displacement
  reporting, and target-warp handling.   P3 feature work is complete; P6-01
  through P6-06 add the shot-quality, 2D-confiner, framing-compatibility,
  target-group update/write-back, impulse-helper, and time-scale follow-ups.
  P4-01 completed ConfinerOven's aspect-aware baking, winding-independent
  erosion, holes, maximum windows, and midpoint oversize fallback. P4-02
  completed Confiner2D shape caches, frustum shrinking, slowing, damping,
  invalidation, and transform deltas. The 2D confiner does not require Clipper2.
  P4-03 completed the shared spline contract, `SplineSettings`, cubic Bezier
  arc-length evaluation, unit conversion, closest points, and smooth control
  points. P4-04 completed SplineDolly body placement, spline-local offsets,
  rotation modes, offset/angular damping, and fixed-speed/nearest-point
  automatic dolly. P4-05 completed the entity-driven SplineCart helper with
  update-method dispatch, unit evaluation, preserved scale, and the same
  automatic-dolly methods. P4-06 completed normalized/eased spline roll
  profiles and roll application in SplineDolly and SplineCart. P4-07 completed
  normalized look-at profiles with per-item easing and Aim-stage targeting for
  SplineDolly. P4-08 completed explicit and host-triggered smoothing of
  BezierSpline control points, knot orientation, and open/looped seams. P4-09
  completed eight-slot weighted child-state mixing with live-child selection
  and update-manager liveness tracking. P4-10 completed quality/priority
  shot selection with activation timing, pending switches, and deterministic
  equal-shot randomization. P4-11 completed instruction-list sequencing with
  hold/blend timing, last-shot hold, looping, and brain-driven activation.
  P4-12 completed hash-based animation-state mapping with priority selection,
  parent fallback, transition states, activation delays, and minimum dwell.
  P5-01 completed engine-neutral timeline shot selection, active-clip weight
  submission through camera overrides, track priority, and gameplay fallback.
  P5-02 completed opaque post-process profiles, custom-blendable weight
  blending, physical focus tracking, damped auto-focus, and the Brain
  post-processor/cut-history hook. P5-03 completed opaque storyboard image
  handles, owner-only custom-blendable submission, fill/split layout math,
  camera mute hints, and a host-rendered screen-space overlay adapter. P5-04
  completed pixel-perfect settings, reference/crop/upscale layout math,
  live-camera orthographic correction, final-blit geometry, and a pixel-art
  adapter example. P5-05 completed a reference-focused parity suite covering
  camera-state look-at blending, standard/vector damping, input-axis response,
  priority limits, blend events/reversal/cancellation, interrupted freeze
  snapshots, and body-component final positions; see
  [parity.md](parity.md). P5-06 completed the performance suite, removed
  steady-state allocations from the camera update path, and added release
  frame-time budgets; see [performance.md](performance.md). All planned P0-P5
  camera feature work is complete; the P6 runtime parity follow-up is tracked
  separately in [runtime-parity.md](runtime-parity.md), and release readiness
  remains in [release-readiness.md](release-readiness.md).
- P6-01 completed the standalone `ShotQualityEvaluator`, the
  `IShotQualityEvaluator` marker, ClearShot scenario coverage, and the
  `32_clear_shot` integration. P6-02 completed incremental ConfinerOven baking,
  normalized skeleton padding, oversize-window padding, and bake progress/status.
  P6-03 completed GroupFraming's Confiner2D Body-stage compatibility and
  orthographic lens-cache invalidation. P6-04 completed TargetGroup's
  clock-gated cache and owner-transform write-back. P6-05 completed raw curve
  evaluation, the authored `FixedSignal`, and the host-driven
  `ExternalImpulseListener`. P6-06 completed the world unscaled hooks, Brain
  effective-delta selection, fixed-clock unscaled delta, and impulse unscaled
  time.
- A stable C ABI is not part of 0.1; the C++20 API and integration path are the
  supported release surface. See [c-api.md](c-api.md).
- Windows Debug/Release, performance, AddressSanitizer, and installed-package
  verification are complete. The MIT license, third-party notices, release
  notes, and package contents are finalized. Linux validation remains the
  release blocker; see [release-readiness.md](release-readiness.md).
- Engine-specific integrations are adapters, not core features. The core library
  must remain buildable without SDL, OpenGL, Unity, or Clipper2.
