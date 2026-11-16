#include <cinerig/core/lookahead_settings.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

TEST_CASE("Lookahead settings are disabled with zero prediction by default", "[unit][core][lookahead]")
{
    const LookaheadSettings& settings = LookaheadSettings::Default;

    REQUIRE_FALSE(settings.enabled);
    REQUIRE(settings.time == 0.0f);
    REQUIRE(settings.smoothing == 0.0f);
    REQUIRE_FALSE(settings.ignoreY);
}
