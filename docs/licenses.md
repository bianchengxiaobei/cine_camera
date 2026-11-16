# License Review Notes

Status: inventory complete; distribution approval and CineRig's own license
selection remain open.

This document records the dependency licenses found in the repository and build
configuration. It is an engineering inventory, not legal advice.

## CineRig

CineRig does not yet have a root `LICENSE` file. Selecting and adding the
project's distribution license is a release blocker and requires the project
owner's decision.

The reference project, Unity Cinemachine, is used only as a behavioral
specification. No C# runtime code, Unity assets, prefabs, scenes, or editor
tooling are vendored or copied. The reference project's Unity Companion License
does not grant a license to CineRig as an independent project; release approval
must review this boundary explicitly.

## Runtime

The core `cinerig` target links only the C++ standard library and platform
libraries. It does not link SDL3, OpenGL, glad, Catch2, Clipper2, or GLM.

## Bundled In The Repository

| Component | Location | Use | License |
| --- | --- | --- | --- |
| glad 2.0.8 generated loader | `third_party/glad/` | Example OpenGL loading | MIT for the generated loader; Khronos headers carry Apache-2.0 and MIT terms; see `third_party/glad/LICENSE` |
| stb_easy_font | `third_party/stb/stb_easy_font.h` | Example HUD text | MIT or public domain, at the user's choice; text is embedded in the header |

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

- [ ] Choose and add CineRig's root `LICENSE`.
- [ ] Generate a `THIRD_PARTY_NOTICES` bundle containing the full license texts
  for everything included in the release artifact.
- [ ] Include SDL3 and glad notices if examples are distributed.
- [ ] Include Catch2 notices if tests or test binaries are distributed.
- [ ] Confirm that no Unity Cinemachine code, assets, or package contents are
  present in the release artifact.
