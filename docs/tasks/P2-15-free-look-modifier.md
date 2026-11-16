# P2-15 Free Look Modifier

Status: completed  
Depends on: P0-04, P1-02, P2-04, P2-05  
Blocks: vertical-axis-driven top/center/bottom camera rigs

## Goal

Implement the `components::FreeLookModifier` extension. It reads a normalized
value from an OrbitalFollow, PanTilt, or ThirdPersonFollow component and drives
top/center/bottom camera settings through a list of built-in modifiers.

## Scope

- Add reusable modifier interfaces:
  - value source
  - position damping
  - screen composition
  - camera distance
  - noise amplitude/frequency
- Implement those interfaces on OrbitalFollow, PanTilt, ThirdPersonFollow,
  PositionComposer, and RotationComposer where applicable.
- Add `components::FreeLookModifier` with:
  - normalized value-source discovery and caching
  - configurable easing
  - before-pipeline/after-pipeline modifier lifecycle
  - built-in Tilt, Lens, PositionDamping, Composition, Distance, and Noise
    modifiers
  - validation and reset helpers
- Add `ScreenComposerSettings::lerp` for composition interpolation.
- Add unit coverage for value sources, easing, distance/damping/composition/lens
  interpolation, tilt correction, component restoration, and validation.
- Add a `15_free_look_modifier` SDL example with an OrbitalFollow vertical
  sweep, top/center/bottom settings, easing, HUD, and smoke test.
- Update the task index, feature matrix, and example index.

## Non-goals

- Editor inspectors, `[SerializeReference]` modifier lists, or concrete noise
  components.
- Timeline integration or animation-curve assets.
- Additional modifiers beyond the built-in set.
- Unity transform serialization.

## Design

CineRig exposes the original nested component interfaces as standalone
interfaces in `core/modifier_interfaces.hpp`. The extension locates a value
source among the virtual camera's pipeline components and extensions.

```text
pre-pipeline:
    refresh value source and component caches
    value = valueSource.normalizedModifierValue
    current = sign(value) * easingCurve(abs(value))
    each modifier.beforePipeline(current)

post-finalize:
    each modifier.afterPipeline(current)
```

Top/center/bottom blending uses:

```text
value >= 0: lerp(center, top, value)
value <  0: lerp(bottom, center, value + 1)
```

LensModifier interpolates from the pre-pipeline state lens toward the top or
bottom lens. Distance, damping, composition, and noise modifiers temporarily
change their target component before the pipeline and restore the center value
after Finalize. TiltModifier is a post-Finalize orientation correction.

The easing curve ports the original two-key cubic Hermite curve:

```text
y = 2t + 2t^3 - 3t^2 + easing * (t^3 - t)
```

## API Sketch

```cpp
class FreeLookModifier : public CameraExtension
{
public:
    template <typename T> struct TopBottomRigs { T top{}; T bottom{}; };

    class Modifier { /* validate/reset/cache/before/after */ };
    template <typename T> class ComponentModifier;
    class TiltModifier;
    class LensModifier;
    class PositionDampingModifier;
    class CompositionModifier;
    class DistanceModifier;
    class NoiseModifier;

    std::vector<std::unique_ptr<Modifier>> modifiers;
    float easing = 0.0f;

    void validate();
    void refreshCache(VirtualCamera& vcam);
    void reset(VirtualCamera& vcam);
};
```

## Reference

- `com.unity.cinemachine/Runtime/Behaviours/CinemachineFreeLookModifier.cs`
  - interfaces, built-in modifiers, easing, cache refresh, and callbacks
- `com.unity.cinemachine/Runtime/Components/CinemachineOrbitalFollow.cs`
  - normalized value, position damping, and distance implementations
- `com.unity.cinemachine/Runtime/Components/CinemachinePanTilt.cs`
  - normalized tilt value
- `com.unity.cinemachine/Runtime/Components/CinemachineThirdPersonFollow.cs`
  - normalized target tilt, damping, and distance
- `com.unity.cinemachine/Runtime/Core/ScreenComposerSettings.cs`
  - composition lerp semantics

## Acceptance

- Value sources report normalized values in -1..1 for the supported components.
- Easing changes the response without changing the endpoints.
- Top, center, and bottom values blend as specified.
- Distance, damping, composition, and noise settings are restored after
  Finalize.
- Tilt updates orientation correction only at Finalize.
- Lens interpolation preserves camera mode as in the original.
- Validation clamps tilt, damping, distance, composition, lens, and easing.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/core/modifier_interfaces.hpp`
  - value-source, position-damping, composition, distance, and noise interfaces.
- `include/cinerig/components/free_look_modifier.hpp` and
  `src/components/free_look_modifier.cpp`
  - normalized value-source discovery and caching
  - cubic-Hermite easing with configurable center smoothing
  - Tilt, Lens, PositionDamping, Composition, Distance, and Noise modifiers
  - before-pipeline mutation and after-Finalize restoration
  - validation and reset helpers
- Modifier interfaces implemented on:
  - `OrbitalFollow`: normalized vertical, position damping, distance
  - `PanTilt`: normalized tilt
  - `ThirdPersonFollow`: normalized target tilt, position damping, distance
  - `PositionComposer`: position damping, composition, distance
  - `RotationComposer`: composition
- `ScreenComposerSettings::lerp`
  - screen position, effective dead-zone/hard-limit sizes, enable flags, and
    hard-limit offset.
- `tests/unit/free_look_modifier_test.cpp`
  - 4 cases and 31 assertions covering value sources, easing, top/bottom
    blending, distance/damping/composition/lens/tilt behavior, restoration,
    noise settings, and validation.
- `examples/15_free_look_modifier`
  - OrbitalFollow vertical sweep, Distance/Damping/Composition/Lens/Tilt
    modifiers, easing toggle, pause/reset, HUD, and headless smoke test.
- `include/cinerig/cinerig.hpp`, `CMakeLists.txt`, and
  `tests/unit/CMakeLists.txt`
  - Export and build the extension and tests.
- Task index, feature matrix, and example index updated.

### Differences

- CineRig exposes the original nested interfaces as standalone `core`
  interfaces so camera components can implement them without including the
  extension.
- Modifier lists are `std::vector<std::unique_ptr<Modifier>>` rather than
  serialized editor references.
- The easing curve reproduces the original two-key cubic Hermite tangents
  without an editor AnimationCurve asset.
- A NoiseModifier is present and tested against a fake noise component; the
  concrete Perlin noise component remains P3 work.

### Verification

- Core Debug: **270/270 passed** (189 unit + 75 scenario + 6 golden).
- Core Release: **270/270 passed**.
- Examples Debug: **300/300 passed** (270 core + 30 example tests, including 15
  smoke tests).
- Examples Release: **300/300 passed**.
- `cinerig_15_free_look_modifier --frames 60 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 15 example exits 0.
- Visible window confirms the P2-15 title, vertical axis/normalized/eased
  values, top/base/bottom distance and damping, modified FOV, easing mode,
  orbit composition, controls, and frame count. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p2_free_look_modifier.png`.
- Linux was not verified on hardware.
