# P5-06 Performance Benchmarks

Status: completed  
Depends on: P1-01, P2-03, P2-04, P3-03, P4-03, P4-04, P5-05  
Blocks: P5-07

## Goal

Add a repeatable performance suite for the engine-neutral runtime and remove
steady-state work that the reference implementation deliberately avoids:
per-frame registry sorting, temporary container allocation, and per-frame
update-table allocation. The suite reports useful timings and enforces
conservative budgets so
that a regression is visible without turning CI into a hardware benchmark.

## Scope

- Add a `tests/performance` Catch2 target with the `performance` CTest label.
- Benchmark representative steady-state workloads:
  - one Brain with 64 following/composing cameras in `Always` standby mode;
  - the same scene with `RoundRobin` standby, where only one non-live camera
    is refreshed per rendered frame;
  - `CameraState::lerp` over camera states with look-at and lens data;
  - `BezierSpline` position evaluation on a multi-knot arc-length spline.
- Add an allocation counter to the performance executable and assert that the
  warmed steady-state camera-frame loop performs zero heap allocations in
  optimized builds. Debug reports MSVC checked-iterator container proxies
  without enforcing zero.
- Remove hot-path costs found by the suite:
  - `UpdateManager::updateAll` must reuse a scratch level table instead of
    allocating a fresh nested-vector copy every frame. The scratch preserves
    the existing behavior when a manager enables a disabled child during
    update.
  - `BlendManager::syncLiveFlags` must reuse scratch storage instead of
    allocating a local live-camera vector every frame.
  - `VirtualCameraRegistry` must avoid `stable_sort` on every lookup, while
    still observing direct priority changes immediately.
- Add `tests/performance/README.md` and `docs/performance.md` with the workload
  definitions, measured host results, budgets, and adaptation notes.
- Update the task index, feature matrix, architecture/testing notes, and root
  README.

## Non-goals

- Reproducing the Unity Profiler, Burst, or editor performance windows.
- GPU, render-pipeline, SDL, or example-framework timing.
- Multi-threaded execution or a custom allocator for the runtime.
- Making Debug builds meet Release budgets. Debug and Release use separate,
  deliberately loose ceilings.
- Changing Cinemachine behavior or hiding work by skipping required updates.
  The benchmarks must use the public `CameraSystem`/`CameraBrain` path.

## Design

```text
tests/performance/
  CMakeLists.txt
  allocation_counter.hpp
  allocation_counter.cpp
  performance_test.cpp
  README.md

ctest -L performance
```

The performance tests use `FakeEntityWorld`, a no-op `ICameraOutput`, and a
fixture with real `VirtualCamera` pipelines:

```text
64 cameras: Follow (Body) + RotationComposer (Aim)
one Brain with Cut default blend
top-priority camera is live; other cameras use the tested standby mode
fixed 1/60 s steps through CameraSystem::lateUpdate
```

Timing is taken with `std::chrono::steady_clock` in the test executable only.
The core library remains free of wall-clock reads. Each benchmark uses a
warm-up, several samples, and the median per-operation time. Absolute budgets
are conservative regression guards, not claims about a particular CPU; the
README records the host and measured values.

The allocation guard overrides the executable's global allocation operators
and counts allocations only inside the measured steady-state loop. Setup,
warm-up, Catch2 reporting, and printing happen outside the counted region.

Reference registry behavior: Unity's `VirtualCameraRegistry` keeps an
`m_ActiveCamerasAreSorted` flag, sorts lazily, and exposes the active-camera
array "without generating garbage". CineRig keeps the same observable ordering
but cannot rely on a Unity priority setter, so it caches a priority/activation
signature and invalidates the cached order when that signature changes.

## Reference

- `com.unity.cinemachine/Runtime/Core/VirtualCameraRegistry.cs`
  - lazy sort flag, activation-order tie break, garbage-free active lookup.
- `com.unity.cinemachine/Runtime/Core/CameraUpdateManager.cs`
  - round-robin standby update once per rendered frame, per-camera update
    status, and the once-per-clock-frame guard.
- `com.unity.cinemachine/Runtime/Core/CinemachineVirtualCameraBase.cs`
  - cached target/name data and the `StandbyUpdate` performance contract.
- `com.unity.cinemachine/Runtime/Behaviours/CinemachineBrain.cs`
  - cached output state and one output application per render frame.
- `com.unity.cinemachine/Documentation~/CinemachineCamera.md`
  - standby update is the reference performance-tuning control.
- CineRig reference-focused tests:
  - `tests/scenario/camera_system_standby_test.cpp`
  - `tests/scenario/camera_system_update_clock_test.cpp`
  - `tests/scenario/brain_priority_test.cpp`
  - `tests/parity/priority_parity_test.cpp`

## Acceptance

- The performance target builds and runs on Windows Debug and Release.
- `ctest -L performance` passes.
- The Release steady-state camera-frame loop reports zero heap allocations
  after warm-up. Debug reports MSVC checked-iterator container proxies and does
  not enforce the zero-allocation assertion.
- `Always` updates all 64 cameras; `RoundRobin` updates the live camera plus
  one standby camera per rendered frame.
- Measured medians remain below the documented Debug and Release budgets.
- Direct priority writes still change the active camera on the next Brain
  update.
- The full unit, scenario, golden, parity, performance, and example suites pass.
- No core dependency on SDL, OpenGL, Unity, or editor facilities is added.

## Implementation Record

### Implemented

- `tests/performance` Catch2 target with the `performance` CTest label:
  - `allocation_counter.hpp/.cpp` with global allocation instrumentation and a
    probe that verifies the counter is active;
  - `performance_test.cpp` with 64-camera `Always`/`RoundRobin` frames,
    `CameraState::lerp`, `BezierSpline::evaluatePosition`, standby update-count
    checks, and Release zero-allocation enforcement;
  - `README.md` with workload, budget, host, and Debug proxy notes.
- Hot-path fixes:
  - `UpdateManager::updateAll` now copies the registry levels into reusable
    scratch storage before iterating. This removes the per-frame allocation
    while preserving safe iteration when a manager enables a disabled child
    during update.
  - `BlendManager::syncLiveFlags` reuses `currentLiveCameras_` instead of a local
    vector.
  - `VirtualCameraRegistry` caches its sorted active list behind a
    priority/activation signature; direct priority writes still invalidate the
    order on the next Brain update.
- `docs/performance.md` and updates to the architecture, feature matrix, root
  README, and task index.

### Verification

- `[performance]`: **6 cases**; Debug **13 assertions**, Release **14
  assertions**.
- Release steady-state camera frames: **0 heap allocations** over 30 frames
  after warm-up. Debug reports 2070 MSVC checked-iterator container-proxy
  allocations for the same loop and does not enforce zero.
- Measured medians, Windows x64, Intel Core i7-10700 (2.90 GHz):
  - 64-camera Always frame: Debug 1.09 ms / Release 67.9 us
    (budgets 10 ms / 500 us);
  - 64-camera RoundRobin frame: Debug 143 us / Release 6.35 us (budgets 1 ms /
    50 us);
  - `CameraState::lerp`: Debug 6.5 us / Release 0.544 us (budgets 30 us / 3 us);
  - `BezierSpline::evaluatePosition`: Debug 0.89 us / Release 0.029 us (budgets
    10 us / 0.5 us).
- Core Debug and Release: **465/465 passed** (319 unit + 140 scenario + 6
  golden; 4114 assertions). CTest includes parity and performance, so the full
  core run is **489/489**.
- Examples Debug and Release: **542/542 passed** (465 core + 18 parity + 6
  performance + 53 example/smoke tests, including 38 smoke tests).
- Core no-tests and examples-no-tests Debug builds pass.
- Install exports the updated runtime headers and library. The external
  `find_package(CineRig)` consumer rebuilds and exits 0.
- Windows Debug/Release verified; Linux remains untested.
