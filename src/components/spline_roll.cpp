#include <cinerig/components/spline_roll.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

void SplineRoll::reset()
{
    easing = true;
    roll.clear();
}

void SplineRoll::validate()
{
    for (RollData& data : roll)
    {
        if (!std::isfinite(data.position))
            data.position = 0.0f;
        if (!std::isfinite(data.value))
            data.value = 0.0f;
        data.position = std::clamp(data.position, 0.0f, 1.0f);
    }
    std::stable_sort(
        roll.begin(),
        roll.end(),
        [](const RollData& a, const RollData& b) {
            return a.position < b.position;
        });
}

float SplineRoll::evaluate(float normalizedPosition) const
{
    if (roll.empty())
        return 0.0f;

    normalizedPosition = std::isnan(normalizedPosition)
        ? 0.0f
        : std::clamp(normalizedPosition, 0.0f, 1.0f);
    if (normalizedPosition <= roll.front().position)
        return roll.front().value;

    for (std::size_t index = 1; index < roll.size(); ++index)
    {
        const RollData& before = roll[index - 1];
        const RollData& after = roll[index];
        if (normalizedPosition > after.position)
            continue;

        const float span = after.position - before.position;
        if (span <= math::Epsilon)
            return after.value;

        float t = (normalizedPosition - before.position) / span;
        if (easing)
            t = t * t * (3.0f - 2.0f * t);
        return before.value + (after.value - before.value) * t;
    }
    return roll.back().value;
}

math::Quat SplineRoll::apply(
    math::Quat rotation,
    float normalizedPosition) const
{
    if (roll.empty())
        return rotation;

    const float radians = evaluate(normalizedPosition) * math::DegToRad;
    const math::Quat localRoll = math::quatFromAxisAngle(
        math::Vec3::forward,
        radians);
    return math::normalize(rotation * localRoll);
}

} // namespace cinerig
