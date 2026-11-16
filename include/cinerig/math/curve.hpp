#pragma once

#include <cinerig/math/vec.hpp>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace cinerig::math
{

struct Keyframe
{
    float time = 0.0f;
    float value = 0.0f;
    float inTangent = 0.0f;
    float outTangent = 0.0f;
};

struct Curve
{
    std::vector<Keyframe> keyframes;

    Curve() = default;
    explicit Curve(std::vector<Keyframe> keys) : keyframes(std::move(keys))
    {
        std::sort(keyframes.begin(), keyframes.end(), [](const Keyframe& a, const Keyframe& b) {
            return a.time < b.time;
        });
    }

    static Curve linear();
    static Curve easeInOut();
    static Curve easeIn();
    static Curve easeOut();
    static Curve hardIn();
    static Curve hardOut();

    bool empty() const { return keyframes.empty(); }
    std::size_t size() const { return keyframes.size(); }

    float evaluate(float time) const
    {
        if (keyframes.empty())
            return 0.0f;

        const float clampedTime = clamp01(time);
        if (clampedTime <= keyframes.front().time)
            return clamp01(keyframes.front().value);
        if (clampedTime >= keyframes.back().time)
            return clamp01(keyframes.back().value);

        for (std::size_t index = 1; index < keyframes.size(); ++index)
        {
            const Keyframe& next = keyframes[index];
            if (clampedTime > next.time)
                continue;

            const Keyframe& previous = keyframes[index - 1];
            const float duration = next.time - previous.time;
            if (duration <= Epsilon)
                return clamp01(next.value);

            const float t = (clampedTime - previous.time) / duration;
            const float t2 = t * t;
            const float t3 = t2 * t;
            const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
            const float h10 = t3 - 2.0f * t2 + t;
            const float h01 = -2.0f * t3 + 3.0f * t2;
            const float h11 = t3 - t2;
            const float value = h00 * previous.value
                + h10 * duration * previous.outTangent
                + h01 * next.value
                + h11 * duration * next.inTangent;
            return clamp01(value);
        }

        return clamp01(keyframes.back().value);
    }
};

inline Curve Curve::linear()
{
    return Curve({
        {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
    });
}

inline Curve Curve::easeInOut()
{
    return Curve({
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f, 0.0f},
    });
}

inline Curve Curve::easeIn()
{
    return Curve({
        {0.0f, 0.0f, 1.0f, 1.4f},
        {1.0f, 1.0f, 0.0f, 1.0f},
    });
}

inline Curve Curve::easeOut()
{
    return Curve({
        {0.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.4f, 1.0f},
    });
}

inline Curve Curve::hardIn()
{
    return Curve({
        {0.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 3.0f, 1.0f},
    });
}

inline Curve Curve::hardOut()
{
    return Curve({
        {0.0f, 0.0f, 1.0f, 3.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
    });
}

}
