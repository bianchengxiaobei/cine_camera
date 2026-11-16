# Performance

CineRig keeps its steady-state camera updates free of heap allocation and avoids
work the Unity Cinemachine reference explicitly caches or defers. The
`performance` test suite measures representative public API paths and guards the
results with conservative budgets.

## Running

```powershell
cmake --build build\tests --config Release
ctest --test-dir build\tests -C Release -L performance --output-on-failure

& build\tests\tests\performance\Release\cinerig_performance_tests.exe --reporter console
```

See [tests/performance/README.md](../tests/performance/README.md) for the local
workload and instrumentation details.

## Reference behavior

The reference implementation treats standby updates, registry lookup, and
per-frame output as performance-sensitive:

- `Runtime/Core/VirtualCameraRegistry.cs` sorts active cameras lazily through an
  `m_ActiveCamerasAreSorted` flag and exposes the list without generating
  garbage.
- `Runtime/Core/CameraUpdateManager.cs` advances round-robin registration once
  per rendered frame, reuses per-camera update status, and guards against
  multiple updates on the same clock frame.
- `Runtime/Core/CinemachineVirtualCameraBase.cs` exposes `StandbyUpdate` as the
  performance-tuning control and caches target/name data.
- `Runtime/Behaviours/CinemachineBrain.cs` caches the applied camera state and
  submits one output update per render frame.

CineRig keeps the same observable behavior without Unity's global static
lifetime. Direct priority writes remain immediately visible because the
registry compares a priority/activation signature before reusing its cached
sort order.

## Steady-state changes

P5-06 removed three avoidable per-frame costs:

- `UpdateManager::updateAll` copies the registry level table into reusable
  scratch storage instead of allocating a fresh nested-vector copy every frame.
  The scratch keeps iteration safe when a manager enables a child during
  update.
- `BlendManager::syncLiveFlags` reuses scratch storage instead of constructing a
  local live-camera vector.
- `VirtualCameraRegistry` caches its priority-sorted active list and re-sorts
  only when the priority/activation signature changes or the registry is
  modified.

The result is zero heap allocations in the warmed Release camera-frame loop,
including the 64-camera `Always` path.

## Budgets

| Workload | Release budget | Debug budget | Release measured | Debug measured |
| --- | ---: | ---: | ---: | ---: |
| 64-camera Always frame | 500 us | 10 ms | 67.9 us | 1.09 ms |
| 64-camera RoundRobin frame | 50 us | 1 ms | 6.35 us | 143 us |
| CameraState::lerp | 3 us | 30 us | 0.544 us | 6.5 us |
| BezierSpline::evaluatePosition | 0.5 us | 10 us | 0.029 us | 0.89 us |

Measured on Intel Core i7-10700 (8C/16T, 2.90 GHz), Windows 11, Visual Studio
2022 MSVC x64. The budgets include deliberate headroom and are intended to catch
order-of-magnitude regressions, not to rank hardware. Re-measure after a
compiler, standard-library, or CPU change.

## Allocation policy

- The optimized camera update path must not allocate after warm-up.
- First-frame registry/status insertion, camera or extension creation, event
  registration, camera switching, and blend-source creation may allocate.
- Debug builds use MSVC checked iterators, which allocate `_Container_proxy`
  objects for `CameraState` and other containers. The Debug suite reports those
  allocations but enforces the zero-allocation contract only in Release.
- The core library never reads wall-clock time. `std::chrono` is confined to the
  performance test executable.

## Gaps

- No GPU, SDL, OpenGL, render pipeline, or example-framework timing.
- No Unity Profiler, Burst, ECS, or editor instrumentation comparison.
- No multi-threaded scaling benchmark.
- Linux timing and allocation behavior remain unverified on a real host.
