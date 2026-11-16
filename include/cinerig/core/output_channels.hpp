#pragma once

#include <cstdint>

namespace cinerig
{

enum class OutputChannels : std::uint32_t
{
    Default = 1u << 0,
    Channel01 = 1u << 1,
    Channel02 = 1u << 2,
    Channel03 = 1u << 3,
    Channel04 = 1u << 4,
    Channel05 = 1u << 5,
    Channel06 = 1u << 6,
    Channel07 = 1u << 7,
    Channel08 = 1u << 8,
    Channel09 = 1u << 9,
    Channel10 = 1u << 10,
    Channel11 = 1u << 11,
    Channel12 = 1u << 12,
    Channel13 = 1u << 13,
    Channel14 = 1u << 14,
    Channel15 = 1u << 15,
};

constexpr OutputChannels operator|(OutputChannels a, OutputChannels b) noexcept
{
    return static_cast<OutputChannels>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

constexpr OutputChannels operator&(OutputChannels a, OutputChannels b) noexcept
{
    return static_cast<OutputChannels>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

constexpr OutputChannels operator^(OutputChannels a, OutputChannels b) noexcept
{
    return static_cast<OutputChannels>(static_cast<std::uint32_t>(a) ^ static_cast<std::uint32_t>(b));
}

constexpr OutputChannels operator~(OutputChannels value) noexcept
{
    return static_cast<OutputChannels>(~static_cast<std::uint32_t>(value));
}

constexpr OutputChannels& operator|=(OutputChannels& a, OutputChannels b) noexcept
{
    a = a | b;
    return a;
}

constexpr OutputChannels& operator&=(OutputChannels& a, OutputChannels b) noexcept
{
    a = a & b;
    return a;
}

constexpr OutputChannels& operator^=(OutputChannels& a, OutputChannels b) noexcept
{
    a = a ^ b;
    return a;
}

constexpr bool hasAnyChannel(OutputChannels mask, OutputChannels channels) noexcept
{
    return (static_cast<std::uint32_t>(mask) & static_cast<std::uint32_t>(channels)) != 0u;
}

constexpr bool hasAllChannels(OutputChannels mask, OutputChannels channels) noexcept
{
    const std::uint32_t maskValue = static_cast<std::uint32_t>(mask);
    const std::uint32_t channelValue = static_cast<std::uint32_t>(channels);
    return (maskValue & channelValue) == channelValue;
}

constexpr std::uint32_t channelMask(OutputChannels channels) noexcept
{
    return static_cast<std::uint32_t>(channels);
}

constexpr OutputChannels channelsFromMask(std::uint32_t mask) noexcept
{
    return static_cast<OutputChannels>(mask);
}

}
