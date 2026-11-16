#include <cinerig/core/output_channels.hpp>
#include <cinerig/core/priority.hpp>

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

TEST_CASE("Priority settings use zero unless enabled", "[unit][core][priority]")
{
    PrioritySettings priority;
    REQUIRE_FALSE(priority.enabled);
    REQUIRE(priority.effectiveValue() == 0);

    priority.value = 42;
    REQUIRE_FALSE(priority.enabled);
    REQUIRE(priority.effectiveValue() == 0);
    REQUIRE(static_cast<int>(priority) == 0);

    priority.enabled = true;
    REQUIRE(priority.effectiveValue() == 42);
    REQUIRE(static_cast<int>(priority) == 42);

    priority.setValue(-7);
    REQUIRE(priority.enabled);
    REQUIRE(priority.value == -7);
    REQUIRE(priority.effectiveValue() == -7);
}

TEST_CASE("Priority settings convert from integers and enable the value", "[unit][core][priority]")
{
    PrioritySettings priority{9};
    REQUIRE(priority.enabled);
    REQUIRE(priority.value == 9);
    REQUIRE(static_cast<int>(priority) == 9);

    priority = 3;
    REQUIRE(priority.enabled);
    REQUIRE(priority.value == 3);
    REQUIRE(static_cast<int>(priority) == 3);
}

TEST_CASE("Output channel flags compose and filter", "[unit][core][channels]")
{
    REQUIRE(static_cast<std::uint32_t>(OutputChannels::Default) == 1u);
    REQUIRE(static_cast<std::uint32_t>(OutputChannels::Channel01) == 2u);
    REQUIRE(static_cast<std::uint32_t>(OutputChannels::Channel15) == 32768u);

    const OutputChannels mask = OutputChannels::Default | OutputChannels::Channel01;
    REQUIRE(hasAnyChannel(mask, OutputChannels::Channel01));
    REQUIRE_FALSE(hasAnyChannel(mask, OutputChannels::Channel02));
    REQUIRE(hasAllChannels(mask, OutputChannels::Default | OutputChannels::Channel01));
    REQUIRE_FALSE(hasAllChannels(mask, OutputChannels::Default | OutputChannels::Channel02));

    OutputChannels mutableMask = OutputChannels::Default;
    mutableMask |= OutputChannels::Channel02;
    REQUIRE(hasAnyChannel(mutableMask, OutputChannels::Channel02));
    mutableMask &= OutputChannels::Channel02;
    REQUIRE(mutableMask == OutputChannels::Channel02);

    REQUIRE(channelMask(mask) == 3u);
    REQUIRE(channelsFromMask(5u) == (OutputChannels::Default | OutputChannels::Channel02));
    REQUIRE((OutputChannels::Default & OutputChannels::Channel01) == static_cast<OutputChannels>(0u));
}
