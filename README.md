# CineRig

Engine-agnostic camera rig runtime, written in C++20.

CineRig is a clean-room behavioral reimplementation of the Unity Cinemachine
3.1.x runtime. It provides virtual cameras, blending, procedural rigs, noise,
impulse, collision constraints, 2D confinement, splines, manager cameras, and
integration adapters without depending on Unity, SDL, OpenGL, or a game engine.

CineRig is not a Unity package replacement. It is a standalone C++ runtime for
hosts that implement a small set of adapter interfaces.

- Planning and roadmap: [plan.md](plan.md)
- Architecture: [docs/architecture.md](docs/architecture.md)
- Feature matrix: [docs/feature-matrix.md](docs/feature-matrix.md)
- Runtime parity: [docs/parity.md](docs/parity.md), [docs/runtime-parity.md](docs/runtime-parity.md)
- Release status and blockers: [docs/release-readiness.md](docs/release-readiness.md)
- C API decision: [docs/c-api.md](docs/c-api.md)
- Changelog and release notes: [CHANGELOG.md](CHANGELOG.md)
- License: [LICENSE](LICENSE)
- Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)

## Status

- Version: `0.1.0`
- Language: C++20
- License: MIT
- Windows: Debug, Release, AddressSanitizer, performance, install, and consumer
  verification complete.
- Linux: CMake presets and commands are provided, but validation on a Linux
  host or CI is still pending.
- P0-P5 feature work and the P6 runtime-parity follow-up are complete.
- The supported surface is the C++20 API. A stable C ABI is not provided in
  0.1.

Current Windows baseline: **533/533** core CTest, **586/586** example CTest,
**526/526** AddressSanitizer core tests, and **7/7** performance cases.
See [docs/release-readiness.md](docs/release-readiness.md) for the exact
commands and remaining release gate.

## Highlights

### Runtime and blending

- `CameraSystem` with stable `uint64_t` handles for cameras, brains, and
  target groups.
- Fixed, Late, Smart, and Manual update clocks.
- Virtual-camera pipeline with Body, Aim, Noise, and Finalize stages.
- Camera-state blending, nested blends, custom blend lookup, override stacks,
  lens inheritance, and brain events.
- Timeline-style override submission and mixer.
- Scaled and unscaled Brain blend time through `IEntityWorld` hooks.

### Cameras and procedural rigs

- Follow, OrbitalFollow, PositionComposer, RotationComposer.
- ThirdPersonFollow, ThirdPersonAim, PanTilt.
- HardLockToTarget, HardLookAt, RotateWithFollowTarget.
- CameraOffset, Recomposer, FollowZoom, FreeLookModifier.
- InputAxis and InputAxisController.
- TargetGroup, GroupFraming, and group update/write-back semantics.

### Noise, impulse, and constraints

- BasicMultiChannelPerlin and Gaussian filtering.
- Impulse manager, definitions, sources, listeners, and collision impulses.
- FixedSignal and ExternalImpulseListener.
- Deoccluder, Decollider, Confiner3D.
- ConfinerOven and Confiner2D with incremental baking, skeleton padding, and
  oversize handling.
- ShotQualityEvaluator used by ClearShot and Deoccluder paths.

### Splines and manager cameras

- Bezier splines, SplineDolly, SplineCart, SplineRoll,
  SplineDollyLookAtTargets, and SplineSmoother.
- MixingCamera, ClearShot, SequencerCamera, and StateDrivenCamera.

### Integration

- Post-processing profiles, weight blending, and auto-focus.
- Storyboard and PixelPerfect extensions with host-rendered adapters.
- Optional SDL3 + OpenGL 3.3 example layer under `examples/`.
- Installable CMake package with exported `cinerig::cinerig` target.

## What It Is and Is Not

CineRig aims for behavioral compatibility with the reference runtime where a
feature is implemented. Defaults, update order, blending rules, damping, and
edge cases are checked against source behavior with unit, scenario, golden, and
parity tests.

It is not:

- a Unity editor integration, prefab/scene package, or Timeline asset package;
- a drop-in replacement for the Unity Cinemachine package;
- a renderer, physics engine, input system, or windowing layer;
- a stable C ABI;
- bit-for-bit identical in floating-point results.

Editor-only and Unity-lifecycle features such as SoloCamera,
TargetPositionCache, Debug HUD, and UnityEvent helpers are excluded or deferred.
See [docs/runtime-parity.md](docs/runtime-parity.md) for the boundary.

## Requirements

- CMake 3.24+
- A C++20 compiler
- Visual Studio 2022 for the Windows presets, or Ninja for the Linux presets
- Network access on the first test configure (Catch2 v3 is fetched by default)
- For examples: SDL3 (fetched by default), an OpenGL 3.3 driver, and a desktop
  session

The core `cinerig` target links only the C++ standard library and platform
libraries. SDL3, OpenGL, glad, Catch2, and optional compatibility dependencies
do not enter the core library.

## Build

Windows Debug:

```powershell
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

Windows Release:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
```

Linux Debug:

```bash
cmake --preset linux
cmake --build --preset linux
ctest --preset linux
```

The host-default `tests` preset is also available:

```powershell
cmake --preset tests
cmake --build --preset tests
```

## Examples

Examples are disabled by default. `CINERIG_BUILD_EXAMPLES=ON` enables the
example layer and SDL3 support automatically.

```powershell
cmake -S . -B build/examples -G "Visual Studio 17 2022" -A x64 -DCINERIG_BUILD_EXAMPLES=ON
cmake --build build/examples --config Debug
ctest --test-dir build/examples -C Debug --output-on-failure
```

Run a windowed example from `build/examples/examples/`:

```powershell
build\examples\examples\01_follow\Debug\cinerig_01_follow.exe
build\examples\examples\19_impulse\Debug\cinerig_19_impulse.exe
build\examples\examples\24_confiner2d\Debug\cinerig_24_confiner2d.exe
build\examples\examples\32_clear_shot\Debug\cinerig_32_clear_shot.exe
```

Headless smoke run:

```powershell
build\examples\examples\01_follow\Debug\cinerig_01_follow.exe --frames 2 --hidden --no-vsync
```

The full example catalog, controls, and smoke-test commands are in
[examples/README.md](examples/README.md).

Useful examples by area:

| Area | Examples |
| --- | --- |
| Follow, composition, blending | `01_follow`, `03_blend`, `04_position_composer` |
| Orbit, input, third person | `05_orbital_follow`, `06_pan_tilt`, `07_third_person`, `08_third_person_aim` |
| Targets and framing | `09_target_group`, `17_group_framing` |
| Noise and impulse | `18_noise`, `19_impulse`, `20_collision_impulse` |
| Collision and confinement | `21_deoccluder`, `22_decollider`, `23_confiner3d`, `24_confiner2d` |
| Splines | `25_spline` through `30_spline_smoother` |
| Manager cameras | `31_mixing_camera` through `34_state_driven` |
| Integration | `35_timeline`, `36_post_processing`, `37_storyboard`, `38_pixel_perfect` |

## Testing

The test suite is split by purpose:

- `ctest -L unit`: math, core types, and component behavior.
- `ctest -L scenario`: `CameraSystem`, Brain, pipeline, and manager integration.
- `ctest -L golden`: deterministic JSON snapshots.
- `ctest -L parity`: representative expectations ported from the reference
  test suite.
- `ctest -L performance`: Release budgets and steady-state allocation checks.

Run all tests:

```powershell
cmake --build build\tests --config Debug
ctest --test-dir build\tests -C Debug --output-on-failure

cmake --build build\tests --config Release
ctest --test-dir build\tests -C Release --output-on-failure
```

Run a focused slice with Catch2 tags:

```powershell
build\tests\tests\scenario\Debug\cinerig_scenario_tests.exe "[time-scale]"
build\tests\tests\unit\Debug\cinerig_unit_tests.exe "[impulse]"
```

AddressSanitizer uses a separate build tree and excludes timing tests:

```powershell
cmake -S . -B build\asan-tests -G "Visual Studio 17 2022" -A x64 `
  -DCINERIG_BUILD_TESTS=ON -DCINERIG_BUILD_EXAMPLES=OFF `
  -DCMAKE_CXX_FLAGS="/fsanitize=address" `
  -DCMAKE_EXE_LINKER_FLAGS="/fsanitize=address"
cmake --build build\asan-tests --config Release -- /m
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;$env:PATH"
ctest --test-dir build\asan-tests -C Release -LE performance --output-on-failure
```

MSVC AddressSanitizer does not support leak detection on Windows. The run
covers memory addressing errors only.

## Use From CMake

Install the core package:

```powershell
cmake --install build\tests --config Release --prefix build\install
```

Consume it from another CMake project:

```cmake
find_package(CineRig CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE cinerig::cinerig)
```

The install tree contains:

```text
include/cinerig/...
lib/cinerig.lib                 # or libcinerig.a
lib/cmake/CineRig/...
share/CineRig/LICENSE
share/CineRig/THIRD_PARTY_NOTICES.md
share/CineRig/CHANGELOG.md
share/CineRig/README.md
```

## Integration Sketch

The host owns entities, transforms, time, physics, rendering, and input. CineRig
owns camera state, pipelines, blending, and manager logic.

```cpp
#include <cinerig/cinerig.hpp>

using namespace cinerig;

// Host implementations of IEntityWorld, ICameraOutput, and optional adapters.
World world;
Output output;

CameraSystem system(world);
BrainId brain = system.createBrain(output, {});

CameraId cameraId = system.createCamera<VirtualCamera>("Main Camera");
VirtualCamera* camera = system.camera(cameraId);
camera->targets.tracking = TargetRef::entity(playerEntity);
camera->lens.fieldOfView = 50.0f;

camera->setBody<Follow>().followOffset = {0.0f, 2.0f, -6.0f};
camera->setAim<RotationComposer>();

// Once per host frame, using the host's clock values.
system.fixedUpdate(world.fixedDeltaTime());
system.lateUpdate(world.deltaTime());
```

`IEntityWorld` defaults to scaled time. Override `unscaledDeltaTime(clock)` and
`unscaledTime()` if the host supports pause or slow-motion-independent camera
blends and impulses. See [docs/architecture.md](docs/architecture.md) for the
ownership and update model.

## Repository Layout

```text
include/cinerig/   Public headers
src/               Runtime implementation
tests/             Unit, scenario, golden, parity, and performance tests
examples/          Optional SDL3 + OpenGL example layer
docs/              Architecture, parity, release, and task documents
cmake/             CMake package configuration
third_party/       Vendored glad and stb headers
```

## Options

| Option | Default | Description |
| --- | --- | --- |
| `CINERIG_BUILD_TESTS` | `ON` | Build the Catch2/CTest suite |
| `CINERIG_BUILD_EXAMPLES` | `OFF` | Build the SDL3/OpenGL examples |
| `CINERIG_MATH_BACKEND` | `native` | `native` or `glm`; GLM is accepted but not implemented |
| `CINERIG_WITH_SDL3` | `OFF` | Enable SDL3 support; auto-enabled with examples |
| `CINERIG_WITH_CLIPPER2` | `OFF` | Compatibility option; ConfinerOven uses the internal polygon engine |
| `CINERIG_USE_SYSTEM_CATCH2` | `OFF` | Use an installed Catch2 instead of FetchContent |
| `CINERIG_USE_SYSTEM_SDL3` | `OFF` | Use an installed SDL3 instead of FetchContent |
| `CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS` | `OFF` | Enable windowed example smoke tests |

## Known Limitations

- Linux builds and tests are not yet verified on a Linux host or CI.
- `TargetPositionCache` is deferred because it serves editor Timeline
  scrubbing/playback rather than the current runtime path.
- SoloCamera, debug text/frustum display, Unity helper components, and Unity
  editor tooling are outside the core runtime.
- The optional GLM backend is accepted for compatibility but not implemented;
  Clipper2 is optional and not used by the internal ConfinerOven.
- Examples are distributed separately and require SDL3/OpenGL.
- CineRig does not provide Unity `MonoBehaviour` components, prefabs, scenes,
  Timeline assets, or a Unity Input System integration.

## License

CineRig is released under the [MIT License](LICENSE). Third-party components and
reference-project boundaries are documented in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and
[docs/licenses.md](docs/licenses.md).

Unity Cinemachine is used only as a behavioral reference. No Unity C# runtime
code, assets, prefabs, scenes, or editor tooling are included in CineRig.
