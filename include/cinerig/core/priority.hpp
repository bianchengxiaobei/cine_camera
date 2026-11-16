#pragma once

namespace cinerig
{

struct PrioritySettings
{
    bool enabled = false;
    int value = 0;

    constexpr PrioritySettings() = default;
    constexpr PrioritySettings(int priority) : enabled(true), value(priority) {}

    constexpr int effectiveValue() const
    {
        return enabled ? value : 0;
    }

    constexpr void setValue(int priority)
    {
        value = priority;
        enabled = true;
    }

    constexpr operator int() const
    {
        return effectiveValue();
    }
};

}
