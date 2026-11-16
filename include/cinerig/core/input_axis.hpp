#pragma once

#include <cstdint>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

enum class InputAxisRestriction : std::uint32_t
{
    None = 0,
    RangeIsDriven = 1u << 0,
    NoRecentering = 1u << 1,
    Momentary = 1u << 2,
};

constexpr InputAxisRestriction operator|(InputAxisRestriction a, InputAxisRestriction b) noexcept
{
    return static_cast<InputAxisRestriction>(
        static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr InputAxisRestriction operator&(InputAxisRestriction a, InputAxisRestriction b) noexcept
{
    return static_cast<InputAxisRestriction>(
        static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr InputAxisRestriction operator~(InputAxisRestriction value) noexcept
{
    return static_cast<InputAxisRestriction>(~static_cast<std::uint32_t>(value));
}

constexpr InputAxisRestriction& operator|=(InputAxisRestriction& a, InputAxisRestriction b) noexcept
{
    a = a | b;
    return a;
}

constexpr InputAxisRestriction& operator&=(InputAxisRestriction& a, InputAxisRestriction b) noexcept
{
    a = a & b;
    return a;
}

constexpr bool hasInputAxisRestriction(
    InputAxisRestriction value,
    InputAxisRestriction restriction) noexcept
{
    return (static_cast<std::uint32_t>(value)
            & static_cast<std::uint32_t>(restriction))
        != 0u;
}

struct InputAxisRecenteringSettings
{
    bool enabled = false;
    float wait = 1.0f;
    float time = 2.0f;

    static const InputAxisRecenteringSettings Default;

    void validate();
};

inline const InputAxisRecenteringSettings InputAxisRecenteringSettings::Default{};

struct InputAxis
{
    float value = 0.0f;
    float center = 0.0f;
    math::Vec2 range{};
    bool wrap = false;
    InputAxisRecenteringSettings recentering{};
    InputAxisRestriction restrictions = InputAxisRestriction::None;

    static InputAxis defaultMomentary();

    float clampValue(float input) const;
    float getNormalizedValue() const;
    float getClampedValue() const;
    void validate();
    void reset();

    bool trackValueChange(float currentTime);
    void setValueAndLastValue(float input);
    void updateRecentering(float deltaTime, bool forceCancel, float currentTime);
    void updateRecentering(
        float deltaTime,
        bool forceCancel,
        float currentTime,
        float center);
    void triggerRecentering();
    void cancelRecentering();

private:
    float currentTime_ = 0.0f;
    float lastValue_ = 0.0f;
    float lastValueChangeTime_ = 0.0f;
    float recenteringVelocity_ = 0.0f;
    bool forceRecenter_ = false;
};

} // namespace cinerig
