#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace cinerig::math;

TEST_CASE("Standard damp follows the exponential residual formula", "[unit][math]")
{
    REQUIRE(standardDamp(100.0f, 1.0f, 1.0f) == Catch::Approx(99.0f).margin(1e-4f));
    REQUIRE(standardDamp(100.0f, 1.0f, 0.0f) == 0.0f);
    REQUIRE(standardDamp(100.0f, 0.0f, 1.0f) == 100.0f);
    REQUIRE(standardDamp(0.0f, 1.0f, 1.0f) == 0.0f);
    REQUIRE(standardDamp(-100.0f, 1.0f, 1.0f) == Catch::Approx(-99.0f).margin(1e-4f));

    const Vec3 damped = standardDamp(Vec3{100.0f, 50.0f, -20.0f}, 1.0f, 1.0f);
    REQUIRE(damped.x == Catch::Approx(99.0f).margin(1e-4f));
    REQUIRE(damped.y == Catch::Approx(49.5f).margin(1e-4f));
    REQUIRE(damped.z == Catch::Approx(-19.8f).margin(1e-4f));
}

TEST_CASE("Smooth damp approaches the target and stays finite", "[unit][math]")
{
    Vec3 velocity{};
    Vec3 current{};
    const Vec3 target{10.0f, 0.0f, 0.0f};

    for (int frame = 0; frame < 300; ++frame)
        current = smoothDamp(current, target, velocity, 0.5f, std::numeric_limits<float>::infinity(), 1.0f / 60.0f);

    REQUIRE(approximately(current, target, 0.01f));
    REQUIRE(velocity.length() < 0.1f);

    const Vec3 clamped = smoothDamp(Vec3{}, Vec3{100.0f, 0.0f, 0.0f}, velocity, 1.0f, 1.0f, 1.0f);
    REQUIRE(clamped.length() < 100.0f);
    REQUIRE(std::isfinite(clamped.x));
}
