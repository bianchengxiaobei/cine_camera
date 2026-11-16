#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Vec3 arithmetic and products", "[unit][math]")
{
    const Vec3 a{1.0f, 2.0f, 3.0f};
    const Vec3 b{4.0f, 5.0f, 6.0f};

    REQUIRE(approximately(a + b, Vec3{5.0f, 7.0f, 9.0f}));
    REQUIRE(approximately(b - a, Vec3{3.0f, 3.0f, 3.0f}));
    REQUIRE(approximately(a * 2.0f, Vec3{2.0f, 4.0f, 6.0f}));
    REQUIRE(approximately(b / 2.0f, Vec3{2.0f, 2.5f, 3.0f}));
    REQUIRE(dot(a, b) == Catch::Approx(32.0f));
    REQUIRE(approximately(cross(Vec3::right, Vec3::up), Vec3::back));
    REQUIRE(a.length() == Catch::Approx(std::sqrt(14.0f)));
    REQUIRE(normalize(Vec3{0.0f, 0.0f, 0.0f}).sqrMagnitude() == 0.0f);
    REQUIRE(approximately(normalize(Vec3{0.0f, 5.0f, 0.0f}), Vec3::up));
}

TEST_CASE("Vec3 interpolation and geometry helpers", "[unit][math]")
{
    REQUIRE(approximately(lerp(Vec3{0.0f, 0.0f, 0.0f}, Vec3{10.0f, 0.0f, 0.0f}, 0.25f), Vec3{2.5f, 0.0f, 0.0f}));
    REQUIRE(approximately(lerp(Vec3{1.0f, 1.0f, 1.0f}, Vec3{3.0f, 3.0f, 3.0f}, 2.0f), Vec3{3.0f, 3.0f, 3.0f}));
    REQUIRE(approximately(projectOnPlane(Vec3{1.0f, 2.0f, 3.0f}, Vec3::up), Vec3{1.0f, 0.0f, 3.0f}));

    const Vec3 halfway = slerp(Vec3::right, Vec3::up, 0.5f);
    REQUIRE(halfway.length() == Catch::Approx(1.0f));
    REQUIRE(dot(halfway, Vec3::right) == Catch::Approx(std::sqrt(0.5f)));
    REQUIRE(dot(halfway, Vec3::up) == Catch::Approx(std::sqrt(0.5f)));

    const Vec3 oppositeHalfway = slerp(Vec3::forward, Vec3::back, 0.5f);
    REQUIRE(oppositeHalfway.length() == Catch::Approx(1.0f));
    REQUIRE(std::abs(dot(oppositeHalfway, Vec3::forward)) < 1e-4f);

    REQUIRE(signedAngle(Vec3::up, Vec3::right, Vec3::forward) == Catch::Approx(HalfPi));
    REQUIRE(signedAngle(Vec3::up, Vec3::right, Vec3::back) == Catch::Approx(-HalfPi));
    REQUIRE(angle(Vec3::forward, Vec3::right) == Catch::Approx(HalfPi));
}

TEST_CASE("Scalar angle and approximation helpers", "[unit][math]")
{
    REQUIRE(normalizeAngle(Pi + 0.5f) == Catch::Approx(-Pi + 0.5f));
    REQUIRE(normalizeAngle(-Pi - 0.5f) == Catch::Approx(Pi - 0.5f));
    REQUIRE(normalizeAngleDegrees(190.0f) == Catch::Approx(-170.0f));
    REQUIRE(clamp01(-1.0f) == 0.0f);
    REQUIRE(clamp01(2.0f) == 1.0f);
    REQUIRE(almostZero(Vec3{0.00001f, 0.0f, 0.0f}));
    REQUIRE_FALSE(almostZero(Vec3{0.01f, 0.0f, 0.0f}));
    REQUIRE(approximately(1.0f, 1.00005f));
    REQUIRE_FALSE(approximately(1.0f, 1.01f));
}

TEST_CASE("closest point on segment", "[unit][math]")
{
    REQUIRE(closestPointOnSegment(Vec3{5.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{10.0f, 0.0f, 0.0f}) == Catch::Approx(0.5f));
    REQUIRE(closestPointOnSegment(Vec3{-5.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{10.0f, 0.0f, 0.0f}) == Catch::Approx(0.0f));
    REQUIRE(closestPointOnSegment(Vec3{15.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{10.0f, 0.0f, 0.0f}) == Catch::Approx(1.0f));
}
