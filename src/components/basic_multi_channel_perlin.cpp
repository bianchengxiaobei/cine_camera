#include <cinerig/components/basic_multi_channel_perlin.hpp>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{
namespace
{

std::uint32_t splitMix32(std::uint32_t& state) noexcept
{
    state += 0x9e3779b9u;
    std::uint32_t value = state;
    value = (value ^ (value >> 16)) * 0x85ebca6bu;
    value = (value ^ (value >> 13)) * 0xc2b2ae35u;
    return value ^ (value >> 16);
}

float randomOffset(std::uint32_t& state) noexcept
{
    const float unit = static_cast<float>(splitMix32(state)) / 4294967296.0f;
    return unit * 2000.0f - 1000.0f;
}

} // namespace

bool BasicMultiChannelPerlin::isValid() const
{
    return noiseProfile.has_value();
}

void BasicMultiChannelPerlin::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached() || !isValid() || deltaTime < 0.0f)
    {
        initialized_ = false;
        return;
    }

    if (!initialized_)
        initialize();

    noiseTime_ += deltaTime * frequencyGain;

    const NoiseProfile& profile = *noiseProfile;
    const math::Vec3 positionNoise =
        profile.getCombinedPosition(noiseTime_, noiseOffsets) * amplitudeGain;
    const math::Vec3 cameraSpacePosition{
        positionNoise.x,
        positionNoise.y,
        -positionNoise.z,
    };
    state.positionCorrection +=
        math::rotate(state.correctedOrientation(), cameraSpacePosition);

    const math::Vec3 orientationNoise =
        profile.getCombinedOrientation(noiseTime_, noiseOffsets) * amplitudeGain;
    const math::Quat rotationNoise = math::quatFromEuler({
        -orientationNoise.x * math::DegToRad,
        -orientationNoise.y * math::DegToRad,
        orientationNoise.z * math::DegToRad,
    });

    if (!math::almostZero(pivotOffset))
    {
        const math::Vec3 pivotDisplacement =
            pivotOffset - math::rotate(rotationNoise, pivotOffset);
        state.positionCorrection +=
            math::rotate(state.correctedOrientation(), pivotDisplacement);
    }

    state.orientationCorrection = state.orientationCorrection * rotationNoise;
}

void BasicMultiChannelPerlin::reSeed()
{
    std::uint32_t state = seed;
    noiseOffsets = {
        randomOffset(state),
        randomOffset(state),
        randomOffset(state),
    };
    seed = state;
}

std::pair<float, float> BasicMultiChannelPerlin::getNoiseAmplitudeFrequency() const
{
    return {amplitudeGain, frequencyGain};
}

void BasicMultiChannelPerlin::setNoiseAmplitudeFrequency(std::pair<float, float> value)
{
    amplitudeGain = value.first;
    frequencyGain = value.second;
}

void BasicMultiChannelPerlin::initialize()
{
    initialized_ = true;
    noiseTime_ = virtualCamera().currentTime() * frequencyGain;
    if (math::almostZero(noiseOffsets))
        reSeed();
}

} // namespace cinerig
