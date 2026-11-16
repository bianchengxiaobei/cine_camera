# Release Readiness

Current status: Windows verification complete; MIT license, third-party
notices, release notes, and package contents finalized. Linux validation
remains the release blocker.

## Verified

| Area | Result |
| --- | --- |
| Core Debug | 533/533 CTest passed |
| Core Release | 533/533 CTest passed |
| Examples Debug | 586/586 CTest passed |
| Examples Release | 586/586 CTest passed |
| Parity | 18 cases / 229 assertions passed |
| Performance | 7 cases passed; Release steady-state allocations zero |
| AddressSanitizer | 526/526 core tests passed (performance excluded) |
| Install/export | `find_package(CineRig)` consumer rebuilt and exited 0 |
| No-tests build | Core Release `CINERIG_BUILD_TESTS=OFF` build passed |
| License | MIT selected; root [LICENSE](../LICENSE) added |
| Third-party notices | [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) covers bundled, fetched, and reference-project boundaries |
| Release notes | [CHANGELOG.md](../CHANGELOG.md) records the 0.1.0 feature set, verification, and known limitations |
| Package contents | Core install tree includes headers, library, CMake config, license, notices, changelog, and README |

Test composition: 354 unit, 148 scenario, 6 golden, 18 parity, 7 performance,
and 53 example/smoke tests. The examples total includes the core, parity, and
performance suites.

## 0.1.0 Package Contents

The core install tree contains:

- `include/cinerig/**` public headers;
- `lib/cinerig.lib` on MSVC or `libcinerig.a` on Unix-like toolchains;
- `lib/cmake/CineRig/` package config, version, and exported targets;
- `share/CineRig/LICENSE`, `THIRD_PARTY_NOTICES.md`, `CHANGELOG.md`, and
  `README.md`.

Tests, examples, fetched Catch2/SDL3 sources, and platform example binaries are
not part of the core install package. Examples are distributed separately and
carry the SDL3/glad notices listed in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md). The project version is
`0.1.0` in [CMakeLists.txt](../CMakeLists.txt); the release notes are in
[CHANGELOG.md](../CHANGELOG.md).

## Windows Commands

```powershell
cmake --build build\tests --config Debug
ctest --test-dir build\tests -C Debug --output-on-failure

cmake --build build\tests --config Release
ctest --test-dir build\tests -C Release --output-on-failure

ctest --test-dir build\tests -C Debug -L performance --output-on-failure
ctest --test-dir build\tests -C Release -L performance --output-on-failure

cmake --install build\tests --config Release --prefix build\install
```

The install step must place `LICENSE`, `THIRD_PARTY_NOTICES.md`,
`CHANGELOG.md`, and `README.md` under `share/CineRig` in addition to the
headers, library, and CMake package files.

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

## Decisions

- **License:** CineRig 0.1 uses the MIT License. The root [LICENSE](../LICENSE)
  and [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) are included in the
  install tree. The reference project's Unity Companion License remains a
  behavioral-reference boundary, not a source-code license for CineRig.
- **C API:** no stable C ABI in 0.1. See [c-api.md](c-api.md).
- **Runtime parity follow-up:** the P6 tasks in
  [runtime-parity.md](runtime-parity.md) are complete and are not release
  blockers for 0.1 unless the release scope is expanded. They remain separate
  from the Linux validation gate above.
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
- [x] CineRig license selected (MIT) and third-party notices bundled.
- [x] Version, package contents, and release notes finalized.
- [ ] Linux Debug/Release builds and tests pass.
