#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Standard blend curves", "[unit][math]")
{
    const auto checkCurve = [](const Curve& curve, float midpoint) {
        REQUIRE(curve.evaluate(0.0f) == Catch::Approx(0.0f));
        REQUIRE(curve.evaluate(1.0f) == Catch::Approx(1.0f));
        REQUIRE(curve.evaluate(0.5f) == Catch::Approx(midpoint));
        REQUIRE(curve.evaluate(-1.0f) == Catch::Approx(0.0f));
        REQUIRE(curve.evaluate(2.0f) == Catch::Approx(1.0f));
        for (int index = 1; index <= 100; ++index)
        {
            const float previous = curve.evaluate(static_cast<float>(index - 1) / 100.0f);
            const float current = curve.evaluate(static_cast<float>(index) / 100.0f);
            REQUIRE(current >= previous);
        }
    };

    checkCurve(Curve::linear(), 0.5f);
    checkCurve(Curve::easeInOut(), 0.5f);
    checkCurve(Curve::easeIn(), 0.675f);
    checkCurve(Curve::easeOut(), 0.325f);
    checkCurve(Curve::hardIn(), 0.125f);
    checkCurve(Curve::hardOut(), 0.875f);
}

TEST_CASE("Custom curve keyframes and clamping", "[unit][math]")
{
    const Curve curve({{0.0f, 0.0f, 1.0f, 1.0f}, {0.5f, 0.2f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}});
    REQUIRE(curve.size() == 3);
    REQUIRE(curve.evaluate(0.0f) == Catch::Approx(0.0f));
    REQUIRE(curve.evaluate(0.5f) == Catch::Approx(0.2f));
    REQUIRE(curve.evaluate(1.0f) == Catch::Approx(1.0f));
    REQUIRE(curve.evaluate(0.75f) > 0.2f);
    REQUIRE(curve.evaluate(0.75f) < 1.0f);
}

TEST_CASE("Curve result is clamped to unit range", "[unit][math]")
{
    const Curve curve({{0.0f, -1.0f, 0.0f, 0.0f}, {1.0f, 2.0f, 0.0f, 0.0f}});
    REQUIRE(curve.evaluate(0.0f) == Catch::Approx(0.0f));
    REQUIRE(curve.evaluate(1.0f) == Catch::Approx(1.0f));
    REQUIRE(curve.evaluate(0.5f) == Catch::Approx(0.5f));
}
