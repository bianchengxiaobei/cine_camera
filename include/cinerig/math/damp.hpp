#pragma once

#include <cinerig/math/vec.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace cinerig::math
{

inline constexpr float kNegligibleResidual = 0.01f;
inline constexpr float kLogNegligibleResidual = -4.605170186f;

inline float standardDamp(float initial, float dampTime, float deltaTime)
{
    if (dampTime < Epsilon || std::abs(initial) < Epsilon)
        return initial;
    if (deltaTime < Epsilon)
        return 0.0f;
    return initial * (1.0f - std::exp(kLogNegligibleResidual * deltaTime / dampTime));
}

inline Vec3 standardDamp(Vec3 initial, Vec3 dampTime, float deltaTime)
{
    return {
        standardDamp(initial.x, dampTime.x, deltaTime),
        standardDamp(initial.y, dampTime.y, deltaTime),
        standardDamp(initial.z, dampTime.z, deltaTime),
    };
}

inline Vec3 standardDamp(Vec3 initial, float dampTime, float deltaTime)
{
    return standardDamp(initial, Vec3{dampTime, dampTime, dampTime}, deltaTime);
}

inline Vec3 clampMagnitude(Vec3 value, float maxLength)
{
    if (maxLength <= 0.0f)
        return Vec3{};
    const float lengthSquared = value.sqrMagnitude();
    if (lengthSquared <= maxLength * maxLength)
        return value;
    return value.normalized() * maxLength;
}

inline float smoothDamp(
    float current,
    float target,
    float& velocity,
    float smoothTime,
    float maxSpeed,
    float deltaTime)
{
    if (deltaTime <= 0.0f)
        return current;

    smoothTime = std::max(0.0001f, smoothTime);
    const float omega = 2.0f / smoothTime;
    const float x = omega * deltaTime;
    const float exponential = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    float change = current - target;
    const float originalTarget = target;
    if (std::isfinite(maxSpeed))
    {
        const float maxChange = maxSpeed * smoothTime;
        change = clamp(change, -maxChange, maxChange);
    }
    target = current - change;

    const float temporary = (velocity + change * omega) * deltaTime;
    velocity = (velocity - temporary * omega) * exponential;
    float output = target + (change + temporary) * exponential;

    if ((originalTarget - current) * (output - originalTarget) > 0.0f)
    {
        output = originalTarget;
        velocity = (output - originalTarget) / deltaTime;
    }
    return output;
}

inline Vec3 smoothDamp(
    Vec3 current,
    Vec3 target,
    Vec3& velocity,
    float smoothTime,
    float maxSpeed,
    float deltaTime)
{
    if (deltaTime <= 0.0f)
        return current;

    smoothTime = std::max(0.0001f, smoothTime);
    const float omega = 2.0f / smoothTime;
    const float x = omega * deltaTime;
    const float exponential = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    Vec3 change = current - target;
    const Vec3 originalTarget = target;
    if (std::isfinite(maxSpeed))
        change = clampMagnitude(change, maxSpeed * smoothTime);
    target = current - change;

    const Vec3 temporary = (velocity + change * omega) * deltaTime;
    velocity = (velocity - temporary * omega) * exponential;
    Vec3 output = target + (change + temporary) * exponential;

    if (dot(originalTarget - current, output - originalTarget) > 0.0f)
    {
        output = originalTarget;
        velocity = (output - originalTarget) / deltaTime;
    }
    return output;
}

}
