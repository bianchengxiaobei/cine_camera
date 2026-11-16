# Release Readiness

Current status: Windows verification complete; Linux validation and license
review remain release blockers.

## Verified

| Area | Result |
| --- | --- |
| Core Debug | 489/489 CTest passed |
| Core Release | 489/489 CTest passed |
| Examples Debug | 542/542 CTest passed |
| Examples Release | 542/542 CTest passed |
| Parity | 18 cases / 229 assertions passed |
| Performance | 6 cases passed; Release steady-state allocations zero |
| AddressSanitizer | 483/483 core tests passed (performance excluded) |
| Install/export | `find_package(CineRig)` consumer rebuilt and exited 0 |
| No-tests builds | Core and examples Debug configure/build passed |

Test composition: 319 unit, 140 scenario, 6 golden, 18 parity, 6 performance,
and 53 example/smoke tests. The examples total includes the core, parity, and
performance suites.

## Windows Commands

```powershell
cmake --build build\tests --config Debug
ctest --test-dir build\tests -C Debug --output-on-failure

cmake --build build\tests --config Release
ctest --test-dir build\tests -C Release --output-on-failure

ctest --test-dir build\tests -C Debug -L performance --output-on-failure
ctest --test-dir build\tests -C Release -L performance --output-on-failure
```

AddressSanitizer uses a separate build tree and excludes timing tests:

```powershell
cmake -S . -B build\asan-tests -G "Visual Studio 17 2022" -A x64 `
  -DCINERIG_BUILD_TESTS=ON -DCINERIG_BUILD_EXAMPLES=OFF `
  -DCMAKE_CXX_FLAGS="/fsanitize=address" `
  -DCMAKE_EXE_LINKER_FLAGS="/fsanitize=address"
cmake --build build\asan-tests --config Release -- /m
$env:PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64;$env:PATH"
ctest --test-dir build\asan-tests -C Release -LE performance -j 8 --output-on-failure
```

MSVC ASan does not support `detect_leaks` on Windows. The run covers memory
addressing errors only.

## Linux Commands

Run on a Linux host or CI runner with CMake, Ninja, and a C++20 compiler:

```bash
cmake --preset linux
cmake --build --preset linux
ctest --preset linux --output-on-failure
```

Also run the Release preset and the examples configuration if the release
artifacts include examples. Linux timing and allocation behavior must be
re-measured; Windows budgets are not a portable benchmark.

## Remaining Blockers

- **Linux:** not executed on a real host. WSL and Docker are unavailable on the
  current development machine. Clang 17 on Windows cannot build against MSVC STL
  14.44 because that STL requires Clang 19 or newer.
- **License:** CineRig does not yet have a distribution license or a reviewed
  third-party notice bundle. The reference project uses the Unity Companion
  License and must not be treated as a source-code license for CineRig. The
  dependency inventory is in [licenses.md](licenses.md).

## Decisions

- **C API:** no stable C ABI in 0.1. See [c-api.md](c-api.md).
- **Editor/tooling:** Unity Editor, prefabs, Timeline assets, and package
  integration remain excluded.
- **Performance budgets:** hardware-specific; the documented host is Intel
  Core i7-10700 on Windows 11 with VS2022 MSVC x64.

## Release Gate

- [x] Windows Debug/Release core and examples pass.
- [x] Performance budgets and allocation check pass.
- [x] AddressSanitizer core run passes.
- [x] Installed-package consumer passes.
- [x] C API decision recorded.
- [x] Third-party dependency inventory recorded.
- [ ] Linux Debug/Release builds and tests pass.
- [ ] CineRig license selected and third-party notices approved.
- [ ] Version, package contents, and release notes finalized.
