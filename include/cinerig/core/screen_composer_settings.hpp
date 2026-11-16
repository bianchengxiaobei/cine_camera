#pragma once

#include <cinerig/math/geometry.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

struct ScreenComposerSettings
{
    struct DeadZoneSettings
    {
        bool enabled = false;
        math::Vec2 size{0.2f, 0.2f};
    };

    struct HardLimitSettings
    {
        bool enabled = false;
        math::Vec2 size{0.8f, 0.8f};
        math::Vec2 offset{0.0f, 0.0f};
    };

    math::Vec2 screenPosition{0.0f, 0.0f};
    DeadZoneSettings deadZone;
    HardLimitSettings hardLimits;

    static const ScreenComposerSettings Default;

    math::Vec2 effectiveDeadZoneSize() const;
    math::Vec2 effectiveHardLimitSize() const;
    math::Rect deadZoneRect() const;
    math::Rect hardLimitsRect() const;
    void validate();
    static ScreenComposerSettings lerp(
        const ScreenComposerSettings& a,
        const ScreenComposerSettings& b,
        float t);
    static bool approximately(const ScreenComposerSettings& a, const ScreenComposerSettings& b);
};

inline const ScreenComposerSettings ScreenComposerSettings::Default{};

inline math::Vec2 ScreenComposerSettings::effectiveDeadZoneSize() const
{
    return deadZone.enabled ? deadZone.size : math::Vec2{};
}

inline math::Vec2 ScreenComposerSettings::effectiveHardLimitSize() const
{
    return hardLimits.enabled ? hardLimits.size : math::Vec2{3.0f, 3.0f};
}

inline math::Rect ScreenComposerSettings::deadZoneRect() const
{
    const math::Vec2 size = effectiveDeadZoneSize();
    return {
        screenPosition.x - size.x * 0.5f + 0.5f,
        screenPosition.y - size.y * 0.5f + 0.5f,
        size.x,
        size.y,
    };
}

inline math::Rect ScreenComposerSettings::hardLimitsRect() const
{
    if (!hardLimits.enabled)
    {
        const math::Vec2 size = effectiveHardLimitSize();
        return {-size.x * 0.5f, -size.y * 0.5f, size.x, size.y};
    }

    const math::Vec2 deadZoneSize = effectiveDeadZoneSize();
    math::Rect result{
        screenPosition.x - hardLimits.size.x * 0.5f + 0.5f,
        screenPosition.y - hardLimits.size.y * 0.5f + 0.5f,
        hardLimits.size.x,
        hardLimits.size.y,
    };
    result.x += hardLimits.offset.x * 0.5f * (hardLimits.size.x - deadZoneSize.x);
    result.y += hardLimits.offset.y * 0.5f * (hardLimits.size.y - deadZoneSize.y);
    return result;
}

inline void ScreenComposerSettings::validate()
{
    screenPosition.x = math::clamp(screenPosition.x, -1.5f, 1.5f);
    screenPosition.y = math::clamp(screenPosition.y, -1.5f, 1.5f);
    deadZone.size.x = math::clamp(deadZone.size.x, 0.0f, 2.0f);
    deadZone.size.y = math::clamp(deadZone.size.y, 0.0f, 2.0f);
    hardLimits.size.x = math::clamp(hardLimits.size.x, deadZone.size.x, 3.0f);
    hardLimits.size.y = math::clamp(hardLimits.size.y, deadZone.size.y, 3.0f);
    hardLimits.offset.x = math::clamp(hardLimits.offset.x, -1.0f, 1.0f);
    hardLimits.offset.y = math::clamp(hardLimits.offset.y, -1.0f, 1.0f);
}
inline ScreenComposerSettings ScreenComposerSettings::lerp(
    const ScreenComposerSettings& a,
    const ScreenComposerSettings& b,
    float t)
{
    ScreenComposerSettings result;
    result.screenPosition = math::lerp(a.screenPosition, b.screenPosition, t);
    result.deadZone.enabled = a.deadZone.enabled || b.deadZone.enabled;
    result.deadZone.size = math::lerp(a.effectiveDeadZoneSize(), b.effectiveDeadZoneSize(), t);
    result.hardLimits.enabled = a.hardLimits.enabled || b.hardLimits.enabled;
    result.hardLimits.size = math::lerp(a.effectiveHardLimitSize(), b.effectiveHardLimitSize(), t);
    result.hardLimits.offset = math::lerp(a.hardLimits.offset, b.hardLimits.offset, t);
    return result;
}

inline bool ScreenComposerSettings::approximately(
    const ScreenComposerSettings& a,
    const ScreenComposerSettings& b)
{
    const math::Vec2 deadZoneA = a.effectiveDeadZoneSize();
    const math::Vec2 deadZoneB = b.effectiveDeadZoneSize();
    const math::Vec2 hardLimitsA = a.effectiveHardLimitSize();
    const math::Vec2 hardLimitsB = b.effectiveHardLimitSize();

    return math::approximately(a.screenPosition.x, b.screenPosition.x)
        && math::approximately(a.screenPosition.y, b.screenPosition.y)
        && math::approximately(deadZoneA.x, deadZoneB.x)
        && math::approximately(deadZoneA.y, deadZoneB.y)
        && math::approximately(hardLimitsA.x, hardLimitsB.x)
        && math::approximately(hardLimitsA.y, hardLimitsB.y)
        && math::approximately(a.hardLimits.offset.x, b.hardLimits.offset.x)
        && math::approximately(a.hardLimits.offset.y, b.hardLimits.offset.y);
}

}
