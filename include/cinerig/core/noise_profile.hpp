#pragma once

#include <cmath>
#include <cstdint>
#include <vector>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

namespace noise_detail
{

constexpr std::uint32_t hash1D(std::int32_t value) noexcept
{
    std::uint32_t hash = static_cast<std::uint32_t>(value);
    hash ^= hash >> 16;
    hash *= 0x7feb352du;
    hash ^= hash >> 15;
    hash *= 0x846ca68bu;
    hash ^= hash >> 16;
    return hash;
}

constexpr float fade(float t) noexcept
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float gradient1D(std::int32_t lattice, float delta) noexcept
{
    const float gradient = (hash1D(lattice) & 1u) != 0u ? 1.0f : -1.0f;
    return gradient * delta;
}

}

inline float perlinNoise1D(float value) noexcept
{
    const float floored = std::floor(value);
    const std::int32_t lattice = static_cast<std::int32_t>(floored);
    const float fraction = value - floored;
    const float start = noise_detail::gradient1D(lattice, fraction);
    const float end = noise_detail::gradient1D(lattice + 1, fraction - 1.0f);
    const float blended = start + noise_detail::fade(fraction) * (end - start);
    return math::clamp(blended + 0.5f, 0.0f, 1.0f);
}

struct NoiseProfile
{
    struct Params
    {
        float frequency = 0.0f;
        float amplitude = 0.0f;
        bool constant = false;

        float getValueAt(float time, float timeOffset) const;
    };

    struct TransformParams
    {
        Params x;
        Params y;
        Params z;

        math::Vec3 getValueAt(float time, math::Vec3 timeOffsets) const;
    };

    std::vector<TransformParams> positionNoise;
    std::vector<TransformParams> orientationNoise;

    static math::Vec3 getCombinedFilterResults(
        const std::vector<TransformParams>& noise, float time, math::Vec3 timeOffsets);

    math::Vec3 getCombinedPosition(float time, math::Vec3 timeOffsets) const;
    math::Vec3 getCombinedOrientation(float time, math::Vec3 timeOffsets) const;
};

inline float NoiseProfile::Params::getValueAt(float time, float timeOffset) const
{
    const float t = (frequency * time) + timeOffset;
    if (constant)
        return std::cos(t * math::TwoPi) * amplitude * 0.5f;
    return (perlinNoise1D(t) - 0.5f) * amplitude;
}

inline math::Vec3 NoiseProfile::TransformParams::getValueAt(float time, math::Vec3 timeOffsets) const
{
    return math::Vec3{
        x.getValueAt(time, timeOffsets.x),
        y.getValueAt(time, timeOffsets.y),
        z.getValueAt(time, timeOffsets.z),
    };
}

inline math::Vec3 NoiseProfile::getCombinedFilterResults(
    const std::vector<TransformParams>& noise, float time, math::Vec3 timeOffsets)
{
    math::Vec3 result{};
    for (const TransformParams& channel : noise)
        result += channel.getValueAt(time, timeOffsets);
    return result;
}

inline math::Vec3 NoiseProfile::getCombinedPosition(float time, math::Vec3 timeOffsets) const
{
    return getCombinedFilterResults(positionNoise, time, timeOffsets);
}

inline math::Vec3 NoiseProfile::getCombinedOrientation(float time, math::Vec3 timeOffsets) const
{
    return getCombinedFilterResults(orientationNoise, time, timeOffsets);
}

}
