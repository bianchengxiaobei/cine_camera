# Feature Matrix

This matrix maps the Unity Cinemachine runtime (3.1.x) to CineRig. It is a
tracking document: update the C++ column as tasks are completed.

Status values: `planned`, `in-progress`, `done`, `excluded`.

## Core

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Camera state | `Runtime/Core/CameraState.cs` | `core::CameraState` | P0 | planned |
| Lens settings | `Runtime/Core/LensSettings.cs` | `core::LensSettings` | P0 | planned |
| Screen composition | `Runtime/Core/ScreenComposerSettings.cs` | `core::ScreenComposerSettings` | P0 | done |
| Lookahead settings | `Runtime/Core/LookaheadSettings.cs` | `core::LookaheadSettings` | P0 | done |
| Noise profile | `Runtime/Core/NoiseSettings.cs` | `core::NoiseProfile` | P0 | done |
| Priority | `Runtime/Core/PrioritySettings.cs` | `core::PrioritySettings` | P0 | planned |
| Output channels | `Runtime/Core/OutputChannel.cs` | `core::OutputChannels` | P0 | planned |
| State blending | `Runtime/Core/CameraState.cs` | `core::CameraState::lerp` | P0 | planned |
| Blend definition | `Runtime/Core/CinemachineBlend.cs` | `core::BlendDefinition`, `core::Blend` | P0 | planned |
| Camera interface | `Runtime/Core/ICinemachineCamera.cs` | `core::ICinemachineCamera` | P0 | planned |
| Damping | `Runtime/Core/Predictor.cs` | `math::Damper` | P0 | planned |
| Position prediction | `Runtime/Core/Predictor.cs` | `math::PositionPredictor` | P0 | planned |
| Target tracking | `Runtime/Core/TargetTracking.cs` | `core::Tracker`, `BindingMode` | P2 | done |

## Runtime

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Camera system | `Runtime/Core/CinemachineCore.cs` | `runtime::CameraSystem` | P1 | planned |
| Active camera registry | `Runtime/Core/VirtualCameraRegistry.cs` | `runtime::VirtualCameraRegistry` | P1 | planned |
| Update manager | `Runtime/Core/CameraUpdateManager.cs` | `runtime::UpdateManager` | P1 | planned |
| Update tracker | `Runtime/Core/UpdateTracker.cs` | `runtime::UpdateTracker` | P1 | planned |
| Target position cache | `Runtime/Core/TargetPositionCache.cs` | `runtime::TargetPositionCache`（运行时路径） | P1 | planned |
| Virtual camera base | `Runtime/Core/CinemachineVirtualCameraBase.cs` | `camera::VirtualCamera` | P1 | planned |
| Component base | `Runtime/Core/CinemachineComponentBase.cs` | `camera::CameraComponent` | P1 | planned |
| Extension base | `Runtime/Core/CinemachineExtension.cs` | `camera::CameraExtension` | P1 | planned |
| Pipeline | `Runtime/Behaviours/CinemachineCamera.cs` | `camera::Pipeline` | P1 | planned |

## Brain and blending

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Brain | `Runtime/Behaviours/CinemachineBrain.cs` | `brain::CameraBrain` | P1 | planned |
| Blend manager | `Runtime/Core/BlendManager.cs` | `brain::BlendManager` | P1 | planned |
| Override stack | `Runtime/Core/CameraBlendStack.cs` | `brain::CameraBlendStack` | P1 | planned |
| Custom blend rules | `Runtime/Core/CinemachineBlenderSettings.cs` | `brain::BlendLookup` | P1 | planned |
| Brain events | `Runtime/Core/CinemachineCore.cs` | `brain::BrainEvents` | P1 | planned |
| External camera | `Runtime/Behaviours/CinemachineExternalCamera.cs` | `camera::ExternalCamera` | P1 | planned |
| Manager base | `Runtime/Core/CinemachineCameraManagerBase.cs` | `camera::VirtualCameraManagerBase` | P1 | planned |

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
| Group framing | `Runtime/Behaviours/CinemachineGroupFraming.cs` | `targets::GroupFraming` | P3 | done |

## Noise, impulse, collision

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Perlin noise | `Runtime/Components/CinemachineBasicMultiChannelPerlin.cs` | `components::BasicMultiChannelPerlin` | P3 | done |
| Gaussian filter | `Runtime/Core/GaussianFilter.cs` | `math::GaussianWindow1D` | P0 | done |
| Impulse manager | `Runtime/Impulse/CinemachineImpulseManager.cs` | `impulse::ImpulseManager` | P3 | done |
| Impulse definition | `Runtime/Impulse/CinemachineImpulseDefinition.cs` | `impulse::ImpulseDefinition` | P3 | done |
| Impulse source | `Runtime/Impulse/CinemachineImpulseSource.cs` | `impulse::ImpulseSource` | P3 | done |
| Impulse listener | `Runtime/Impulse/CinemachineImpulseListener.cs` | `impulse::ImpulseListener` | P3 | done |
| Collision impulse source | `Runtime/Impulse/CinemachineCollisionImpulseSource.cs` | `impulse::CollisionImpulseSource` | P3 | done |
| Impulse filtering | `Runtime/Impulse/CinemachineImpulseListener.cs` | `impulse::ImpulseListener` combination modes | P3 | done |
| Deoccluder | `Runtime/Behaviours/CinemachineDeoccluder.cs` | `components::Deoccluder` | P3 | done |
| Decollider | `Runtime/Behaviours/CinemachineDecollider.cs` | `components::Decollider` | P3 | done |
| Confiner 3D | `Runtime/Behaviours/CinemachineConfiner3D.cs` | `components::Confiner3D` | P3 | done |
| Shot quality evaluator | `Runtime/Behaviours/CinemachineShotQualityEvaluator.cs` | `components::ShotQualityEvaluator` | P3 | planned |

## 2D, splines, managers

| Feature | Cinemachine source | CineRig target | Phase | Status |
| --- | --- | --- | --- | --- |
| Confiner 2D | `Runtime/Behaviours/CinemachineConfiner2D.cs` | `components::Confiner2D` | P4 | done |
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
  reporting, and target-warp handling. All planned P3 features are implemented.
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
  frame-time budgets; see [performance.md](performance.md). All planned camera
  feature work is complete; release readiness is tracked separately.
- A stable C ABI is not part of 0.1; the C++20 API and integration path are the
  supported release surface. See [c-api.md](c-api.md).
- Windows Debug/Release, performance, AddressSanitizer, and installed-package
  verification are complete. Linux validation and license review remain release
  blockers; see [release-readiness.md](release-readiness.md).
- Engine-specific integrations are adapters, not core features. The core library
  must remain buildable without SDL, OpenGL, Unity, or Clipper2.
