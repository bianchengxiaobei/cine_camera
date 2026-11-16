# Performance Benchmarks

The performance suite exercises the public `CameraSystem`/`CameraBrain` update
path and enforces conservative regression budgets. It is not a portable CPU
benchmark and does not replace the unit, scenario, golden, or parity suites.

## Run

```powershell
cmake --build build\tests --config Release
ctest --test-dir build\tests -C Release -L performance --output-on-failure
```

Show the measured medians:

```powershell
& build\tests\tests\performance\Release\cinerig_performance_tests.exe --reporter console
```

The target is `cinerig_performance_tests`; all cases carry the Catch2/CTest
`performance` label.

## Workloads

- **64-camera Always frame**: one Brain, 64 `VirtualCamera` instances with a
  `Follow` body and `RotationComposer` aim, static targets, a top-priority live
  camera, and `StandbyUpdateMode::Always`. Every enabled camera is refreshed.
- **64-camera RoundRobin frame**: the same scene with
  `StandbyUpdateMode::RoundRobin`. The live camera and one standby camera are
  refreshed per rendered frame.
- **CameraState::lerp**: state blending with look-at, position, orientation,
  lens, and shot-quality data.
- **BezierSpline::evaluatePosition**: normalized position evaluation on a
  33-point smooth arc-length spline.

Each timing case uses a warm-up, five samples, and the median per-operation
time. The camera workloads advance `CameraSystem::lateUpdate` with a fixed
1/60 s step.

## Allocation Guard

The executable replaces its global allocation operators and counts requests
inside a steady-state loop. Setup, warm-up, reporting, and Catch2 assertions are
outside the counted region. A probe allocation verifies that the counter is
active.

Release builds enforce zero allocations. Debug builds on MSVC use checked
iterators, which allocate `_Container_proxy` objects when `CameraState` and other
containers are constructed, so Debug reports the count without enforcing zero.
The optimized build is the allocation contract.

## Budgets

Budgets are regression guards with deliberate headroom, not tuning targets.
Measured values are from the host below in the recorded Debug/Release runs.

| Workload | Release budget | Debug budget | Release measured | Debug measured |
| --- | ---: | ---: | ---: | ---: |
| 64-camera Always frame | 500 us | 10 ms | 67.9 us | 1.09 ms |
| 64-camera RoundRobin frame | 50 us | 1 ms | 6.35 us | 143 us |
| CameraState::lerp | 3 us | 30 us | 0.544 us | 6.5 us |
| BezierSpline::evaluatePosition | 0.5 us | 10 us | 0.029 us | 0.89 us |

Host: Intel Core i7-10700 (8C/16T, 2.90 GHz), Windows 11, Visual Studio 2022
MSVC x64. Re-measure before tightening budgets after a compiler or hardware
change.

## Reference Mapping

- `Runtime/Core/VirtualCameraRegistry.cs` keeps an explicit sorted flag and
  exposes active cameras without generating garbage. CineRig caches a
  priority/activation signature so direct priority writes still invalidate the
  order without sorting every lookup.
- `Runtime/Core/CameraUpdateManager.cs` advances round-robin once per rendered
  frame and keeps per-camera update status.
- `Runtime/Core/CinemachineVirtualCameraBase.cs` documents `StandbyUpdate` as a
  performance control and caches target/name data.
- `Runtime/Behaviours/CinemachineBrain.cs` applies output once per render frame.

## Gaps

- No Unity Profiler, Burst, editor, render-pipeline, or GPU measurement.
- The runtime is single-threaded; no thread-scaling benchmark is provided.
- Absolute budgets are host/compiler specific.
- Linux timing and allocation behavior are not yet verified on a real host.
