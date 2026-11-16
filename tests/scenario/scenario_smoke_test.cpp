#include <cinerig/cinerig.hpp>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Scenario test target is wired", "[scenario]")
{
    REQUIRE(cinerig::versionString() != nullptr);
}
