# P3-03 Basic Multi Channel Perlin

Status: completed  
Depends on: P0-02, P0-04, P1-02, P2-15  
Blocks: impulse secondary noise, free-look noise modulation

## Goal

Implement the `components::BasicMultiChannelPerlin` Noise-stage component. It
samples a `NoiseProfile` over time and writes camera-space position and
orientation corrections, with amplitude/frequency gain, pivot compensation,
deterministic re-seeding, and FreeLook noise-modifier integration.

## Scope

- Add `components::BasicMultiChannelPerlin`:
  - optional `NoiseProfile`
  - `amplitudeGain`, `frequencyGain`, and camera-space `pivotOffset`
  - deterministic `reSeed()`, seed, and per-channel time offsets
  - Noise-stage position and orientation corrections
  - negative-delta and missing-profile initialization reset
  - `IModifiableNoise` amplitude/frequency accessors
- Convert Unity `+Z` camera-space sampled noise to CineRig `-Z` camera space;
  keep the public pivot offset in CineRig camera space.
- Add unit coverage for defaults, validity, time accumulation, amplitude and
  frequency gain, invalid deltas, position and orientation mapping, pivot
  compensation, re-seeding, and FreeLook modulation.
- Add an `18_noise` SDL example with a static framed scene, adjustable
  amplitude/frequency, pivot toggle, freeze, re-seed/reset, HUD, and smoke test.
- Mark the existing `GaussianWindow1D` implementation as done in the feature
  matrix.
- Update the task index, feature matrix, architecture, and example index.

## Non-goals

- Target-position cache playback or editor-only re-seed controls.
- Impulse listeners or secondary impulse noise.
- Additional Perlin implementations or new filter variants.
- Serialization of profiles, seeds, or noise offsets.

## Design

The original component samples both position and orientation noise in Unity
camera space. CineRig uses the same right/up axes but `-Z` forward, so sampled
position noise negates `z`. Unity-positive pitch and yaw map to CineRig
negative pitch and yaw, so orientation noise negates `x` and `y` before
`quatFromEuler`. Orientation noise amplitudes remain in degrees. The public
`pivotOffset` uses CineRig camera space, like other CineRig camera-space
offset settings.

```text
if no profile or deltaTime < 0:
    initialized = false
    return

if not initialized:
    initialized = true
    noiseTime = currentTime * frequencyGain
    if noiseOffsets == 0: reSeed()

noiseTime += deltaTime * frequencyGain
positionCorrection += correctedOrientation * cameraSpace(positionNoise)
if pivotOffset != 0:
    positionCorrection += correctedOrientation
        * (pivot - rotate(rotNoise, pivot))
orientationCorrection *= rotNoise
```

Re-seeding uses a deterministic 32-bit mixer rather than Unity's global random
generator. `reSeed()` fills three offsets in `[-1000, 1000]` and advances the
stored seed so repeated calls produce different sequences.

## API Sketch

```cpp
class BasicMultiChannelPerlin
    : public CameraComponent
    , public IModifiableNoise
{
public:
    std::optional<NoiseProfile> noiseProfile;
    math::Vec3 pivotOffset{};
    float amplitudeGain = 1.0f;
    float frequencyGain = 1.0f;
    std::uint32_t seed = 1;
    math::Vec3 noiseOffsets{};

    Stage stage() const override { return Stage::Noise; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;

    float noiseTime() const noexcept { return noiseTime_; }
    void reSeed();

    std::pair<float, float> getNoiseAmplitudeFrequency() const override;
    void setNoiseAmplitudeFrequency(std::pair<float, float> value) override;

private:
    bool initialized_ = false;
    float noiseTime_ = 0.0f;
};
```

## Reference

- `com.unity.cinemachine/Runtime/Components/CinemachineBasicMultiChannelPerlin.cs`
  - defaults, initialization, time accumulation, position/orientation
    corrections, pivot compensation, re-seeding, and modulation interface
- `com.unity.cinemachine/Runtime/Core/NoiseSettings.cs`
  - noise profile channels and combined filtering
- `CineRig/include/cinerig/core/noise_profile.hpp`
  - existing profile sampling implementation

## Acceptance

- Defaults match the original: amplitude 1, frequency 1, zero pivot offset.
- A missing profile makes the component invalid and applies no correction.
- First update seeds noise time from the virtual camera time; later updates
  accumulate scaled delta time.
- Negative delta resets initialization and applies no correction.
- Amplitude and frequency gains scale the sampled correction.
- Unity camera-space position noise `+Z` maps to CineRig forward `-Z`.
- `pivotOffset` is interpreted in CineRig camera space.
- Unity-positive orientation pitch and yaw map to CineRig pitch down and yaw
  right; orientation noise is converted from degrees.
- Pivot compensation rotates the camera around the camera-space pivot.
- `reSeed()` is deterministic for a seed and changes subsequent offsets.
- The component exposes the `IModifiableNoise` accessors used by
  FreeLookModifier.
- Unit tests, examples Debug/Release, smoke tests, no-tests build, and install
  pass on Windows.

## Implementation Record

### Implemented

- `include/cinerig/components/basic_multi_channel_perlin.hpp` and
  `src/components/basic_multi_channel_perlin.cpp`
  - optional `NoiseProfile`
  - amplitude gain, frequency gain, and CineRig camera-space pivot offset
  - deterministic 32-bit re-seeding with three time offsets in `[-1000, 1000]`
  - first-update initialization from virtual-camera time and scaled delta-time
    accumulation
  - missing-profile and negative-delta initialization reset
  - position correction in corrected camera orientation
  - orientation correction from degree-valued profile channels
  - pivot compensation and post-multiplied orientation correction
  - `IModifiableNoise` amplitude/frequency accessors
- Unity-to-CineRig conversion
  - sampled position `z` negates because Unity is `+Z` forward and CineRig is
    `-Z` forward
  - orientation `x` and `y` negate to preserve Unity pitch-down and yaw-right
  - public pivot offset uses CineRig camera space
- `tests/unit/basic_multi_channel_perlin_test.cpp`
  - 8 cases and 37 assertions covering defaults, validity, time accumulation,
    gains, invalid deltas, axis conversion, orientation conversion, pivot
    compensation, deterministic re-seeding, and modulation accessors.
- `examples/18_noise`
  - static framed target, two position and two orientation channels, amplitude
    and frequency controls, pivot toggle, freeze, re-seed/reset, correction HUD,
    and headless smoke test.
- Feature matrix marks the existing `GaussianWindow1D`, noise profile, and
  Perlin component as done.

### Differences

- Unity's global random source is replaced by a deterministic 32-bit mixer.
  `reSeed()` advances the stored seed so repeated calls produce new offsets.
- The noise profile is owned by value through `std::optional`; there is no
  Unity asset reference or TargetPositionCache playback time.
- CineRig camera-space pivot offsets use the same convention as other CineRig
  camera-space settings; only sampled profile data is converted from Unity.
- CineRig has no per-component enabled flag, so validity is profile presence.

### Verification

- Core Debug: **293/293 passed** (212 unit + 75 scenario + 6 golden).
- Core Release: **293/293 passed**.
- Examples Debug: **326/326 passed** (293 core + 33 example tests, including 18
  smoke tests).
- Examples Release: **326/326 passed**.
- `cinerig_18_noise --frames 120 --hidden --no-vsync` exits 0.
- `build/examples-no-tests` Debug builds and the 18 example exits 0.
- Visible window shows the static target with live position/orientation shake,
  correction HUD, and all controls. Screenshot:
  `C:\Users\CHENFU~1\AppData\Local\Temp\cinerig_p3_noise.png`.
- Linux was not verified on hardware.

