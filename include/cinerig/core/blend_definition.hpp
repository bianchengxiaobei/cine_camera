#pragma once

#include <cinerig/math/curve.hpp>

namespace cinerig
{

enum class BlendStyle
{
    Cut,
    EaseInOut,
    EaseIn,
    EaseOut,
    HardIn,
    HardOut,
    Linear,
    Custom,
};

struct BlendDefinition
{
    BlendStyle style = BlendStyle::EaseInOut;
    float time = 2.0f;
    math::Curve customCurve{};

    BlendDefinition() = default;
    BlendDefinition(BlendStyle blendStyle, float blendTime)
        : style(blendStyle), time(blendTime)
    {
    }

    float blendTime() const
    {
        return style == BlendStyle::Cut ? 0.0f : time;
    }

    math::Curve curve() const
    {
        switch (style)
        {
        case BlendStyle::Cut:
            return {};
        case BlendStyle::EaseInOut:
            return math::Curve::easeInOut();
        case BlendStyle::EaseIn:
            return math::Curve::easeIn();
        case BlendStyle::EaseOut:
            return math::Curve::easeOut();
        case BlendStyle::HardIn:
            return math::Curve::hardIn();
        case BlendStyle::HardOut:
            return math::Curve::hardOut();
        case BlendStyle::Linear:
            return math::Curve::linear();
        case BlendStyle::Custom:
            return customCurve.empty() ? math::Curve::easeInOut() : customCurve;
        }
        return math::Curve::easeInOut();
    }
};

}
