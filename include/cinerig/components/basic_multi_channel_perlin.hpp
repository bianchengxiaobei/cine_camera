#pragma once

#include <cstdint>
#include <optional>
#include <utility>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/modifier_interfaces.hpp>
#include <cinerig/core/noise_profile.hpp>

namespace cinerig
{

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
    void initialize();

    bool initialized_ = false;
    float noiseTime_ = 0.0f;
};

} // namespace cinerig
