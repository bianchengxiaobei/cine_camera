# CineRig

Engine-agnostic camera rig runtime, written in C++20.

CineRig is a clean-room behavioral reimplementation of the Cinemachine runtime.
It is designed to be integrated into any engine through a small set of adapter
interfaces.

- Planning and roadmap: [plan.md](plan.md)
- Task documents: [docs/tasks](docs/tasks)
- Runtime parity follow-up: [docs/runtime-parity.md](docs/runtime-parity.md)
- Release status and known blockers: [docs/release-readiness.md](docs/release-readiness.md)
- C API decision: [docs/c-api.md](docs/c-api.md)
- License: [LICENSE](LICENSE)
- Third-party notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
- Changelog and release notes: [CHANGELOG.md](CHANGELOG.md)
- License review notes: [docs/licenses.md](docs/licenses.md)

## Status

P0–P5 feature work is complete and verified on Windows in Debug and Release.
The P6 runtime parity follow-up is also complete for the identified non-editor
behavior gaps. P6-01 adds the standalone shot-quality evaluator; P6-02 adds
incremental Confiner2D/ConfinerOven baking, skeleton padding, and oversize
cache invalidation; P6-03 adds GroupFraming's Confiner2D Body-stage and
lens-cache compatibility path; P6-04 adds TargetGroup's clock-gated cache and
owner-transform write-back; P6-05 adds authored fixed impulse signals and the
host-driven external impulse listener; P6-06 adds unscaled world time hooks and
Brain/impulse ignore-time-scale selection. P6 is tracked separately from the
0.1 release gate.
Release readiness is in progress: Linux validation remains pending, while the
MIT license, third-party notices, package contents, and 0.1.0 release notes are
finalized. CineRig 0.1 exposes a C++20 API only; a stable C ABI is deferred.

The core library does not depend on SDL, OpenGL, or any game engine. P1 also
provides an optional SDL3 + OpenGL 3.3 example layer under `examples/`.

## Requirements

- CMake 3.24+
- A C++20 compiler
- Ninja for the Linux presets, or Visual Studio 2022 for the Windows presets
- Network access on the first test configure (Catch2 v3 is fetched by default)
- For examples: SDL3 (fetched by default), an OpenGL 3.3 driver and desktop session

## Build

Windows (Visual Studio 2022, Debug):

```powershell
cmake --preset windows
cmake --build --preset windows
ctest --preset windows
```

Linux (Ninja, Debug):

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

Examples (Windows shown; see `examples/README.md` for Linux and smoke tests):

```powershell
cmake -S . -B build/examples -G "Visual Studio 17 2022" -A x64 -DCINERIG_BUILD_EXAMPLES=ON
cmake --build build/examples --config Debug
ctest --test-dir build/examples -C Debug --output-on-failure
```

## Test suites

- `ctest -L unit`: pure math, core types, and component behavior.
- `ctest -L scenario`: CameraSystem, Brain, manager, and pipeline integration.
- `ctest -L golden`: deterministic JSON snapshots.
- `ctest -L parity`: representative Unity Cinemachine test expectations; see
  [docs/parity.md](docs/parity.md) and [tests/parity](tests/parity).
- `ctest -L performance`: steady-state camera, state-blend, and spline
  benchmarks with Release budgets and allocation checks; see
  [docs/performance.md](docs/performance.md).

## Options

- `CINERIG_BUILD_TESTS` (default `ON`)
- `CINERIG_BUILD_EXAMPLES` (default `OFF`)
- `CINERIG_MATH_BACKEND` (`native` or `glm`, default `native`; `glm` is accepted for compatibility but not implemented)
- `CINERIG_WITH_SDL3` (default `OFF`; auto-enabled with examples)
- `CINERIG_WITH_CLIPPER2` (accepted for compatibility; ConfinerOven uses the internal polygon engine)
- `CINERIG_USE_SYSTEM_CATCH2` (default `OFF`)
- `CINERIG_USE_SYSTEM_SDL3` (default `OFF`)
- `CINERIG_ENABLE_EXAMPLE_SMOKE_TESTS` (default `OFF`)
