#include <cinerig/core/noise_profile.hpp>

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

TEST_CASE("Noise constant channels use the cosine formula", "[unit][core][noise]")
{
    NoiseProfile::Params params;
    params.frequency = 1.0f;
    params.amplitude = 2.0f;
    params.constant = true;

    REQUIRE(params.getValueAt(0.0f, 0.0f) == Catch::Approx(1.0f));
    REQUIRE(params.getValueAt(0.25f, 0.0f) == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(params.getValueAt(0.5f, 0.0f) == Catch::Approx(-1.0f));
    REQUIRE(params.getValueAt(0.0f, 0.5f) == Catch::Approx(-1.0f));
}

TEST_CASE("Noise perlin channels use the centered noise formula", "[unit][core][noise]")
{
    NoiseProfile::Params params;
    params.frequency = 1.0f;
    params.amplitude = 2.0f;

    REQUIRE(params.getValueAt(0.0f, 0.0f) == Catch::Approx(0.0f).margin(1e-6f));
    REQUIRE(params.getValueAt(1.0f, 0.0f) == Catch::Approx(0.0f).margin(1e-6f));

    const float half = params.getValueAt(0.5f, 0.0f);
    REQUIRE(std::abs(half) <= 1.0f);

    params.frequency = 0.0f;
    REQUIRE(params.getValueAt(123.0f, 0.0f) == Catch::Approx(0.0f).margin(1e-6f));

    for (int i = 0; i < 32; ++i)
    {
        const float noise = perlinNoise1D(static_cast<float>(i) * 0.37f);
        REQUIRE(noise >= 0.0f);
        REQUIRE(noise <= 1.0f);
    }
}

TEST_CASE("Noise transform channels accumulate per axis", "[unit][core][noise]")
{
    NoiseProfile::TransformParams params;
    params.x = {0.0f, 2.0f, true};
    params.y = {0.0f, 0.0f, true};
    params.z = {0.0f, 4.0f, true};

    const math::Vec3 value = params.getValueAt(3.0f, {0.0f, 0.0f, 0.0f});
    REQUIRE(value.x == Catch::Approx(1.0f));
    REQUIRE(value.y == Catch::Approx(0.0f));
    REQUIRE(value.z == Catch::Approx(2.0f));

    NoiseProfile profile;
    profile.positionNoise.push_back(params);
    profile.positionNoise.push_back(params);
    profile.orientationNoise.push_back(params);

    const math::Vec3 position = profile.getCombinedPosition(0.0f, {});
    REQUIRE(position.x == Catch::Approx(2.0f));
    REQUIRE(position.y == Catch::Approx(0.0f));
    REQUIRE(position.z == Catch::Approx(4.0f));

    const math::Vec3 orientation = profile.getCombinedOrientation(0.0f, {});
    REQUIRE(orientation.x == Catch::Approx(1.0f));
    REQUIRE(orientation.y == Catch::Approx(0.0f));
    REQUIRE(orientation.z == Catch::Approx(2.0f));

    const NoiseProfile empty;
    const math::Vec3 zero = empty.getCombinedPosition(1.0f, {});
    REQUIRE(zero.x == 0.0f);
    REQUIRE(zero.y == 0.0f);
    REQUIRE(zero.z == 0.0f);
}
