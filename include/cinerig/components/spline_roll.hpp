#pragma once

#include <vector>

#include <cinerig/math/quat.hpp>

namespace cinerig
{

// Normalized roll profile applied around a spline's local forward axis.
class SplineRoll
{
public:
    struct RollData
    {
        float position = 0.0f;
        float value = 0.0f;
    };

    bool easing = true;
    std::vector<RollData> roll;

    void reset();
    void validate();
    bool empty() const noexcept { return roll.empty(); }

    float evaluate(float normalizedPosition) const;
    math::Quat apply(
        math::Quat rotation,
        float normalizedPosition) const;
};

} // namespace cinerig
