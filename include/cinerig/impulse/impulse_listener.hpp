#pragma once

#include <cstdint>
#include <optional>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/core/noise_profile.hpp>

namespace cinerig::impulse
{

class ImpulseManager;

class ImpulseListener : public CameraExtension
{
public:
    enum class SignalCombinationMode
    {
        Additive,
        UseLargest,
    };

    struct ImpulseReaction
    {
        std::optional<NoiseProfile> secondaryNoise;
        float amplitudeGain = 1.0f;
        float frequencyGain = 1.0f;
        float duration = 1.0f;
        std::uint32_t seed = 1;
        math::Vec3 noiseOffsets{};

        void reSeed();
        void reset();
        bool getReaction(
            float currentTime,
            float deltaTime,
            math::Vec3 impulsePos,
            math::Vec3& pos,
            math::Quat& rot);

    private:
        float currentAmount_ = 0.0f;
        float currentTime_ = 0.0f;
        float currentDamping_ = 0.0f;
        bool initialized_ = false;
    };

    explicit ImpulseListener(ImpulseManager* manager = nullptr);

    ImpulseManager* impulseManager = nullptr;
    Stage applyAfter = Stage::Noise;
    std::uint32_t channelMask = 1u;
    float gain = 1.0f;
    bool use2DDistance = false;
    bool useCameraSpace = true;
    SignalCombinationMode signalCombinationMode = SignalCombinationMode::Additive;
    ImpulseReaction reaction{};

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig::impulse
