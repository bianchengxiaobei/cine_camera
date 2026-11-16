# P5-07 Release Readiness

Status: in_progress  
Depends on: P5-06  
Blocks: release

## Goal

Close the gaps that remain after the P0–P5 feature work: record the C API
decision, validate memory safety with a sanitizer, make the project status
accurate, and define the remaining release blockers. This task does not add new
camera features.

## Scope

- Evaluate a stable C ABI and record the decision.
- Run the core test suite under MSVC AddressSanitizer.
- Update `plan.md`, `README.md`, the task index, and the feature matrix so they
  reflect the completed Windows state.
- Add a durable release-readiness checklist with the exact verification
  commands, known gaps, and decisions.
- Select the distribution license, bundle third-party notices, and finalize
  the version, package contents, and release notes.
- Remove formatting defects introduced in the modified Markdown files.

## Non-goals

- Adding a C API implementation.
- Replacing the C++ API or changing public ownership semantics.
- Adding Linux CI or running Linux on a host that is not available.
- Selling or distributing the library before the license review is complete.
- Re-running the performance budgets under AddressSanitizer; instrumentation
  intentionally changes timing.

## Reference

- `com.unity.cinemachine/package.json` and the Unity Companion License for the
  reference project boundary.
- Existing CineRig performance, parity, and example documentation.
- No C# runtime code is copied by this task.

## Verification

Final Windows baseline after P6-06:

- Core Debug and Release: **533/533 passed**.
- Examples Debug and Release: **586/586 passed**.
- MSVC AddressSanitizer Release, excluding `performance`: **526/526 passed**
  (354 unit + 148 scenario + 18 parity + 6 golden).
- Performance label: **7/7 passed** in Debug and Release outside ASan.
- Unit: **354 cases / 3168 assertions**; scenario: **148 cases / 1188
  assertions**.
- Install/export consumer: rebuilt against `build\install` and exited 0.
- No-tests core Release build passes.
- Core install tree contains the `share/CineRig` license/notice/changelog/readme
  bundle.

## Decisions

- **C API:** not provided in 0.1. See [../c-api.md](../c-api.md).
- **Sanitizer scope:** AddressSanitizer runs on Windows for the core suites.
  MSVC ASan does not support `detect_leaks` on this platform, so this run
  validates memory addressing errors but not leak detection.
- **Linux:** still unverified on hardware. WSL is not installed and Docker is
  not available on the development host. Development-time Clang 17 is also
  incompatible with MSVC STL 14.44, which requires Clang 19 or newer.
- **License:** CineRig 0.1 uses the MIT License. The root `LICENSE` and
  `THIRD_PARTY_NOTICES.md` are included in the install tree. The reference
  project remains a behavioral-only boundary.

## Acceptance

- [x] C API decision documented.
- [x] ASan core suite passes.
- [x] Windows test, install, and consumer results are recorded.
- [x] Third-party dependency inventory recorded.
- [x] Project status documents reflect P0–P6 completion and the release state.
- [x] Distribution license selected (MIT) and third-party notices bundled.
- [x] Version, package contents, and release notes finalized.
- [ ] Linux Debug/Release builds and CTest pass on a Linux host or CI.

## Implementation Record

### Implemented

- Added `docs/c-api.md` with the 0.1 decision, ABI requirements, and revisit
  criteria.
- Added `docs/release-readiness.md` with verification commands, current
  results, and remaining blockers.
- Added `docs/licenses.md` with the runtime, vendored, and fetched dependency
  inventory plus release notice actions.
- Updated `plan.md` status, roadmap, C API decision, directory layout, and
  platform risk.
- Updated `README.md` with status and release/C API links.
- Added the task index entry and feature-matrix release notes.
- Added the root MIT `LICENSE` and `THIRD_PARTY_NOTICES.md` with the bundled
  glad/stb texts, fetched Catch2/SDL3 notices, and the Unity reference boundary.
- Added `CHANGELOG.md` with the 0.1.0 feature set, verification counts, and
  known limitations.
- Added install rules for `LICENSE`, `THIRD_PARTY_NOTICES.md`, `CHANGELOG.md`,
  and `README.md` under `share/CineRig`.
- Removed UTF-8 BOMs and trailing blank lines from the modified release-facing
  Markdown files where needed.

### Verification Results

- Windows Debug/Release core and examples remain green.
- ASan Release run command:
  `ctest --test-dir build\asan-tests -C Release -LE performance -j 8`.
  The MSVC ASan runtime directory must be on `PATH`.
- No source changes were required by the sanitizer run.
- The core install tree contains headers, library, CMake package files, and the
  `share/CineRig` license/notice/changelog/readme bundle.
- Linux remains the only functional validation gap; the MIT license and
  third-party notices are finalized.
