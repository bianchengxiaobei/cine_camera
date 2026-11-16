#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Gaussian window kernel sizing", "[unit][math]")
{
    GaussianWindow1D<float> minimal(0.5f);
    REQUIRE(minimal.kernelSize() == 3);
    REQUIRE(minimal.bufferLength() == 3);
    REQUIRE(minimal.isEmpty());

    GaussianWindow1D<float> capped(4.0f, 10);
    REQUIRE(capped.kernelSize() == 21);
    REQUIRE(capped.bufferLength() == 21);
}

TEST_CASE("Gaussian window preserves constant input", "[unit][math]")
{
    GaussianWindow1D<float> scalar(0.5f);
    for (int index = 0; index < 20; ++index)
        REQUIRE(scalar.filter(3.0f) == Catch::Approx(3.0f).margin(1e-6f));
    REQUIRE_FALSE(scalar.isEmpty());

    GaussianWindow1D<Vec3> vector(1.0f);
    for (int index = 0; index < 20; ++index)
        REQUIRE(approximately(vector.filter(Vec3{1.0f, 2.0f, 3.0f}), Vec3{1.0f, 2.0f, 3.0f}, 1e-5f));
}

TEST_CASE("Gaussian window responds to a step", "[unit][math]")
{
    GaussianWindow1D<float> filter(1.0f);
    for (int index = 0; index < 20; ++index)
        filter.filter(0.0f);
    const float value = filter.filter(10.0f);
    REQUIRE(value > 0.0f);
    REQUIRE(value < 10.0f);
    filter.reset();
    REQUIRE(filter.isEmpty());
}
