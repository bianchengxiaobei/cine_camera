#include <cinerig/cinerig.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstring>

TEST_CASE("CineRig library links and reports its version", "[unit]")
{
    REQUIRE(cinerig::versionMajor == 0);
    REQUIRE(cinerig::versionMinor == 1);
    REQUIRE(cinerig::versionPatch == 0);
    REQUIRE(std::strcmp(cinerig::versionString(), "0.1.0") == 0);
}
