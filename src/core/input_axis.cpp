#include <cinerig/core/input_axis.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/math/damp.hpp>

namespace cinerig
{

namespace
{

float wrapSign(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}

} // namespace

void InputAxisRecenteringSettings::validate()
{
    wait = std::max(0.0f, wait);
    time = std::max(0.0f, time);
}

InputAxis InputAxis::defaultMomentary()
{
    InputAxis axis;
    axis.range = {-1.0f, 1.0f};
    axis.restrictions = InputAxisRestriction::NoRecentering | InputAxisRestriction::Momentary;
    return axis;
}

float InputAxis::clampValue(float input) const
{
    const float size = range.y - range.x;
    if (!wrap || size < math::Epsilon)
        return math::clamp(input, range.x, range.y);

    float result = std::fmod(input - range.x, size);
    if (result < 0.0f)
        result += size;
    return result + range.x;
}

float InputAxis::getNormalizedValue() const
{
    const float clamped = clampValue(value);
    const float size = range.y - range.x;
    return (clamped - range.x) / (size > math::Epsilon ? size : 1.0f);
}

float InputAxis::getClampedValue() const
{
    return clampValue(value);
}

void InputAxis::validate()
{
    range.y = std::max(range.x, range.y);
    center = clampValue(center);
    value = clampValue(value);
    recentering.validate();
}

void InputAxis::reset()
{
    cancelRecentering();
    if (recentering.enabled
        && !hasInputAxisRestriction(restrictions, InputAxisRestriction::NoRecentering))
    {
        value = clampValue(center);
    }
}

bool InputAxis::trackValueChange(float currentTime)
{
    currentTime_ = currentTime;
    const float clamped = clampValue(value);
    if (clamped != lastValue_)
    {
        lastValueChangeTime_ = currentTime_;
        lastValue_ = clamped;
        return true;
    }
    return false;
}

void InputAxis::setValueAndLastValue(float input)
{
    value = input;
    lastValue_ = input;
}

void InputAxis::updateRecentering(float deltaTime, bool forceCancel, float currentTime)
{
    updateRecentering(deltaTime, forceCancel, currentTime, center);
}

void InputAxis::updateRecentering(
    float deltaTime,
    bool forceCancel,
    float currentTime,
    float targetCenter)
{
    currentTime_ = currentTime;

    if (hasInputAxisRestriction(
            restrictions,
            InputAxisRestriction::NoRecentering | InputAxisRestriction::Momentary))
    {
        return;
    }

    if (forceCancel)
    {
        cancelRecentering();
        return;
    }

    if ((forceRecenter_ || recentering.enabled) && deltaTime < 0.0f)
    {
        value = clampValue(targetCenter);
        cancelRecentering();
        return;
    }

    if (!forceRecenter_
        && (!recentering.enabled
            || currentTime_ - lastValueChangeTime_ < recentering.wait))
    {
        return;
    }

    float current = clampValue(value);
    const float centerValue = targetCenter;
    const float distance = std::abs(centerValue - current);
    if (distance < math::Epsilon || recentering.time < math::Epsilon)
    {
        current = centerValue;
        recenteringVelocity_ = 0.0f;
    }
    else
    {
        const float size = range.y - range.x;
        if (wrap && size > math::Epsilon && distance > size * 0.5f)
            current += wrapSign(centerValue - current) * size;

        current = math::smoothDamp(
            current,
            centerValue,
            recenteringVelocity_,
            recentering.time * 0.5f,
            9999.0f,
            deltaTime);
    }

    value = clampValue(current);
    lastValue_ = value;
    if (std::abs(value - centerValue) < math::Epsilon)
        forceRecenter_ = false;
}

void InputAxis::triggerRecentering()
{
    forceRecenter_ = true;
}

void InputAxis::cancelRecentering()
{
    lastValueChangeTime_ = currentTime_;
    lastValue_ = clampValue(value);
    recenteringVelocity_ = 0.0f;
    forceRecenter_ = false;
}

} // namespace cinerig
