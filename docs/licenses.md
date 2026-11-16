# License Review Notes

Status: inventory complete; MIT selected; notices bundled. Distribution
approval remains the project owner's responsibility.

This document records the dependency licenses found in the repository and build
configuration. It is an engineering inventory, not legal advice.

## CineRig

CineRig is distributed under the MIT License. The root [LICENSE](../LICENSE)
contains the project copyright notice and license text. Selected on 2026-11-16.
The bundled full license texts and dependency notices are in
[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

The reference project, Unity Cinemachine, is used only as a behavioral
specification. No C# runtime code, Unity assets, prefabs, scenes, or editor
tooling are vendored or copied. The reference project's Unity Companion License
does not grant a license to CineRig as an independent project.

## Runtime

The core `cinerig` target links only the C++ standard library and platform
libraries. It does not link SDL3, OpenGL, glad, Catch2, Clipper2, or GLM.

## Bundled In The Repository

| Component | Location | Use | License |
| --- | --- | --- | --- |
| glad 2.0.8 generated loader | `third_party/glad/` | Example OpenGL loading | `(WTFPL OR CC0-1.0) AND Apache-2.0`; the glad project license is MIT and Khronos headers carry Apache-2.0/MIT terms |
| stb_easy_font | `third_party/stb/stb_easy_font.h` | Example HUD text | MIT or public domain, at the user's choice; CineRig selects MIT |

## Fetched At Configure Time

| Component | Version | Use | License | Distribution action |
| --- | --- | --- | --- | --- |
| Catch2 | v3.8.1 | Tests only | Boost Software License 1.0 | Include its license if test binaries or sources are distributed |
| SDL3 | release-3.2.10 | Examples only | zlib | Include its license if example binaries or sources are distributed |
| GLM | not fetched | Optional math backend; currently accepted but not implemented | MIT | Not a current dependency |
| Clipper2 | not fetched | Optional compatibility option; ConfinerOven uses the internal polygon engine | BSL-1.0 | Not a current dependency |

CMake `FetchContent` places downloaded sources in the build tree; they are not
part of the installed `cinerig` package.

## Release Actions

- [x] Choose and add CineRig's root `LICENSE` (MIT).
- [x] Generate a `THIRD_PARTY_NOTICES.md` bundle containing the full license
  texts for bundled, fetched, and distributable components.
- [x] Include SDL3 and glad notices for example distribution.
- [x] Include Catch2 notices for test/source distribution.
- [x] Confirm that no Unity Cinemachine code, assets, or package contents are
  present in the release artifact.
- [ ] Project owner performs the final distribution/legal approval before
  shipping a release artifact outside the project.
