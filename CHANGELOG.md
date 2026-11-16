# Changelog

All notable changes to CineRig are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-11-16

### Added

- Engine-neutral C++20 runtime with `CameraSystem`, virtual-camera registry,
  update manager/tracker, external cameras, camera managers, and output brains.
- Virtual-camera pipeline with body, aim, noise, finalize, and extension stages,
  all driven through `IEntityWorld` and related adapter interfaces.
- Camera-state blending, custom blend lookup, override stacks, camera events,
  lens inheritance, and fixed/late/smart/manual update clocks.
- 3D procedural cameras: Follow, OrbitalFollow, PositionComposer,
  RotationComposer, ThirdPersonFollow, ThirdPersonAim, HardLockToTarget,
  HardLookAt, RotateWithFollowTarget, CameraOffset, Recomposer, FollowZoom,
  and FreeLookModifier.
- Input and targeting: `InputAxis`, `InputAxisController`, `TargetGroup`,
  `GroupFraming`, and target tracking/binding modes.
- Noise and impulse: Perlin noise, Gaussian filtering, impulse definitions,
  sources, listeners, collision impulses, Deoccluder, Decollider, and
  Confiner3D.
- 2D and spline support: ConfinerOven, Confiner2D, Bezier splines,
  SplineDolly, SplineCart, SplineRoll, SplineDollyLookAtTargets, and
  SplineSmoother.
- Manager cameras: MixingCamera, ClearShot, SequencerCamera, and
  StateDrivenCamera.
- Integration helpers: timeline-style camera overrides and mixer, post-process
  profiles and auto-focus, Storyboard, and PixelPerfect.
- P6 runtime-parity follow-up: standalone ShotQualityEvaluator, incremental
  Confiner2D/ConfinerOven baking with skeleton padding, GroupFraming
  Confiner2D compatibility, TargetGroup update/cache/write-back semantics,
  `FixedSignal`, `ExternalImpulseListener`, and scaled/unscaled time selection
  for Brain blends and impulse events.
- SDL3/OpenGL example layer and examples covering follow, blend, procedural
  cameras, noise/impulse, collision, framing, splines, 2D confinement,
  manager cameras, timeline, storyboard, and pixel-perfect layouts.
- Unit, scenario, golden, parity, performance, and example smoke tests.
- Install/export package with CMake config files, license, notices, changelog,
  and README.

### Changed

- Aligned Brain time-scale behavior with the reference precedence:
  blend-manager override, then unscaled world delta when enabled, then the
  caller-supplied scaled delta. The effective delta is applied once per frame.
- Aligned impulse event time selection with the manager's `ignoreTimeScale`
  option while keeping multiple worlds independent and deterministic.
- Aligned TargetGroup caching and owner-transform write-back with fixed/late
  update semantics.
- Aligned ConfinerOven with incremental baking, normalized skeleton padding,
  oversize windows, and cache invalidation.
- Aligned GroupFraming with the reference Confiner2D Body-stage and
  orthographic lens-cache behavior.
- Removed steady-state allocations from the camera update path and added
  Release performance budgets.

### Verified

- Windows core Debug/Release: **533/533** CTest passed.
- Windows examples Debug/Release: **586/586** CTest passed.
- AddressSanitizer Release, performance excluded: **526/526** core tests passed.
- Unit: **354 cases / 3168 assertions**; scenario: **148 cases / 1188
  assertions**; parity: **18 cases / 229 assertions**.
- Performance: **7/7** cases with zero steady-state allocations in Release.
- Installed-package consumer rebuilt and exited 0.
- `CINERIG_BUILD_TESTS=OFF` Release build passed.

### Known limitations

- Linux Debug/Release builds and tests are not yet verified on a Linux host or
  CI; Windows budgets are not portable.
- `TargetPositionCache` is deferred because it serves editor Timeline
  scrubbing/playback rather than the current runtime path.
- Solo camera, debug text/frustum display, Unity helper components, and Unity
  editor tooling are intentionally outside the core runtime.
- No stable C ABI is provided in 0.1; the supported surface is the C++20 API.
- The optional GLM math backend is accepted for compatibility but not
  implemented. Clipper2 is optional and not used by the internal ConfinerOven.
- Examples are distributed separately from the core install tree and require
  SDL3/OpenGL.

## [Unreleased]

- Linux validation and final release approval remain pending.
