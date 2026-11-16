#include <cmath>
#include <limits>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/cinerig.hpp>

using namespace cinerig;
using Catch::Approx;

namespace
{

bool approxVec(math::Vec3 a, math::Vec3 b, float tolerance = 1e-4f)
{
    return (a - b).length() <= tolerance;
}

BezierSpline makeClosedSquare()
{
    return BezierSpline::linear(
        {
            {7.0f, 0.0f, -6.0f},
            {13.0f, 0.0f, -6.0f},
            {13.0f, 0.0f, 1.0f},
            {7.0f, 0.0f, 1.0f},
        },
        true);
}

} // namespace

TEST_CASE("Spline settings and position helpers have stable defaults", "[unit][spline]")
{
    const SplineSettings settings;
    REQUIRE_FALSE(settings.isValid());
    REQUIRE(settings.spline == nullptr);
    REQUIRE(settings.position == 0.0f);
    REQUIRE(settings.units == PathIndexUnit::Distance);
    REQUIRE(settings.maxPosition() == 0.0f);
    REQUIRE(settings.standardizedPosition() == 0.0f);
    REQUIRE(settings.normalizedPosition() == 0.0f);

    const BezierSpline empty;
    REQUIRE_FALSE(empty.isValid());
    REQUIRE(empty.length() == 0.0f);
    REQUIRE(empty.knotCount() == 0);
    REQUIRE_FALSE(empty.closed());
    REQUIRE(empty.curveCount() == 0);
    REQUIRE(empty.evaluatePosition(0.5f).x == 0.0f);
    REQUIRE(empty.evaluateTangent(0.5f).x == 1.0f);
    REQUIRE(empty.evaluateUp(0.5f).y == 1.0f);
    REQUIRE(empty.closestPoint({1.0f, 2.0f, 3.0f}) == 0.0f);
}

TEST_CASE("Bezier helpers evaluate endpoints and tangents", "[unit][spline]")
{
    const math::Vec3 p0{0.0f, 0.0f, 0.0f};
    const math::Vec3 p1{1.0f, 2.0f, 0.0f};
    const math::Vec3 p2{3.0f, 2.0f, 0.0f};
    const math::Vec3 p3{4.0f, 0.0f, 0.0f};

    REQUIRE(approxVec(bezier3(0.0f, p0, p1, p2, p3), p0));
    REQUIRE(approxVec(bezier3(1.0f, p0, p1, p2, p3), p3));
    REQUIRE(bezier1(0.25f, 0.0f, 1.0f, 2.0f, 3.0f) == Approx(0.75f));

    const math::Vec3 tangent = bezierTangent3(0.5f, p0, p1, p2, p3);
    math::Vec3 w0;
    math::Vec3 w1;
    math::Vec3 w2;
    bezierTangentWeights3(p0, p1, p2, p3, w0, w1, w2);
    const math::Vec3 weighted = w0 * 0.25f + w1 * 0.5f + w2;
    REQUIRE(approxVec(tangent, weighted));
    REQUIRE(bezierTangent1(0.0f, 0.0f, 1.0f, 2.0f, 3.0f) == Approx(3.0f));
}

TEST_CASE("Smooth control points are finite and ordered on a line", "[unit][spline]")
{
    const std::vector<math::Vec3> knots{
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f},
        {3.0f, 0.0f, 0.0f},
    };
    std::vector<math::Vec3> ctrl1;
    std::vector<math::Vec3> ctrl2;
    computeSmoothControlPoints(knots, ctrl1, ctrl2);
    REQUIRE(ctrl1.size() == knots.size());
    REQUIRE(ctrl2.size() == knots.size());
    for (std::size_t index = 0; index + 1 < knots.size(); ++index)
    {
        REQUIRE(std::isfinite(ctrl1[index].x));
        REQUIRE(std::isfinite(ctrl2[index].x));
        REQUIRE(ctrl1[index].x >= knots[index].x);
        REQUIRE(ctrl2[index].x <= knots[index + 1].x);
        REQUIRE(ctrl1[index].y == Approx(0.0f));
        REQUIRE(ctrl2[index].z == Approx(0.0f));
    }

    computeSmoothControlPointsLooped(knots, ctrl1, ctrl2);
    REQUIRE(ctrl1.size() == knots.size());
    for (std::size_t index = 0; index < knots.size(); ++index)
    {
        REQUIRE(std::isfinite(ctrl1[index].x));
        REQUIRE(std::isfinite(ctrl2[index].x));
    }
}

TEST_CASE("Linear spline has exact arc length and unit conversion", "[unit][spline]")
{
    const BezierSpline spline = makeClosedSquare();
    REQUIRE(spline.isValid());
    REQUIRE(spline.closed());
    REQUIRE(spline.knotCount() == 4);
    REQUIRE(spline.curveCount() == 4);
    REQUIRE(spline.length() == Approx(26.0f));

    REQUIRE(approxVec(spline.evaluatePosition(0.0f), {7.0f, 0.0f, -6.0f}));
    REQUIRE(approxVec(spline.evaluatePosition(0.125f), {10.25f, 0.0f, -6.0f}));
    REQUIRE(approxVec(spline.evaluatePosition(0.5f), {13.0f, 0.0f, 1.0f}));
    REQUIRE(approxVec(spline.evaluatePosition(1.0f), {7.0f, 0.0f, -6.0f}));

    REQUIRE(spline.convertPosition(13.0f, PathIndexUnit::Distance, PathIndexUnit::Knot)
        == Approx(2.0f));
    REQUIRE(spline.convertPosition(2.0f, PathIndexUnit::Knot, PathIndexUnit::Normalized)
        == Approx(0.5f));
    REQUIRE(spline.convertPosition(0.125f, PathIndexUnit::Knot, PathIndexUnit::Distance)
        == Approx(0.75f));
    REQUIRE(spline.convertPosition(9.5f, PathIndexUnit::Distance, PathIndexUnit::Knot)
        == Approx(1.5f));
    REQUIRE(spline.convertPosition(
        1.5f, PathIndexUnit::Knot, PathIndexUnit::Normalized)
        == Approx(9.5f / 26.0f));

    const float knotPoint = spline.convertPosition(
        0.125f, PathIndexUnit::Knot, PathIndexUnit::Normalized);
    REQUIRE(approxVec(spline.evaluatePosition(knotPoint), {7.75f, 0.0f, -6.0f}));
    REQUIRE(approxVec(
        spline.evaluatePosition(spline.convertPosition(
            3.5f, PathIndexUnit::Knot, PathIndexUnit::Normalized)),
        {7.0f, 0.0f, -2.5f}));
    REQUIRE(spline.closestPoint({13.5f, 0.0f, -3.0f})
        == Approx(9.0f / 26.0f).margin(2e-3f));
}

TEST_CASE("Spline settings preserve the point when units change", "[unit][spline]")
{
    const BezierSpline spline = makeClosedSquare();
    SplineSettings settings;
    settings.spline = &spline;
    settings.units = PathIndexUnit::Normalized;
    settings.position = 0.5f;

    REQUIRE(settings.isValid());
    REQUIRE(settings.maxPosition() == Approx(1.0f));
    REQUIRE(settings.normalizedPosition() == Approx(0.5f));
    REQUIRE(approxVec(
        spline.evaluatePosition(settings.normalizedPosition()),
        {13.0f, 0.0f, 1.0f}));

    settings.changeUnitPreservePosition(PathIndexUnit::Distance);
    REQUIRE(settings.units == PathIndexUnit::Distance);
    REQUIRE(settings.position == Approx(13.0f));
    REQUIRE(settings.maxPosition() == Approx(26.0f));

    settings.changeUnitPreservePosition(PathIndexUnit::Knot);
    REQUIRE(settings.position == Approx(2.0f));
    REQUIRE(settings.maxPosition() == Approx(4.0f));
    REQUIRE(settings.normalizedPosition() == Approx(0.5f));

    settings.position = 0.5f;
    REQUIRE(settings.normalizedPosition() == Approx(3.0f / 26.0f));

    settings.changeUnitPreservePosition(PathIndexUnit::Normalized);
    REQUIRE(settings.position == Approx(3.0f / 26.0f));
    REQUIRE(approxVec(
        spline.evaluatePosition(settings.normalizedPosition()),
        {10.0f, 0.0f, -6.0f}));
}

TEST_CASE("Open and closed spline positions standardize differently", "[unit][spline]")
{
    const BezierSpline closed = makeClosedSquare();
    float maximum = 0.0f;
    REQUIRE(splineStandardizePosition(closed, -1.0f, PathIndexUnit::Distance, &maximum)
        == Approx(25.0f));
    REQUIRE(maximum == Approx(26.0f));
    REQUIRE(splineStandardizePosition(closed, 4.0f, PathIndexUnit::Knot)
        == Approx(0.0f));
    REQUIRE(splineStandardizePosition(closed, 1.25f, PathIndexUnit::Normalized)
        == Approx(0.25f));
    REQUIRE(splineStandardizePosition(
        closed, std::numeric_limits<float>::quiet_NaN(), PathIndexUnit::Distance)
        == 0.0f);

    const BezierSpline open = BezierSpline::linear(
        {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}},
        false);
    REQUIRE(splineMaxPosition(open, PathIndexUnit::Distance) == Approx(10.0f));
    REQUIRE(splineMaxPosition(open, PathIndexUnit::Knot) == Approx(1.0f));
    REQUIRE(splineStandardizePosition(open, -2.0f, PathIndexUnit::Distance)
        == Approx(0.0f));
    REQUIRE(splineStandardizePosition(open, 99.0f, PathIndexUnit::Knot)
        == Approx(1.0f));
}

TEST_CASE("Open Bezier spline exposes tangent, up and closest point", "[unit][spline]")
{
    const BezierSpline spline = BezierSpline::linear(
        {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}},
        false);
    REQUIRE(spline.length() == Approx(10.0f));
    REQUIRE(approxVec(spline.evaluatePosition(0.5f), {5.0f, 0.0f, 0.0f}));
    REQUIRE(approxVec(spline.evaluateTangent(0.5f), math::Vec3::right));
    REQUIRE(approxVec(spline.evaluateUp(0.5f), math::Vec3::up));
    REQUIRE(spline.closestPoint({3.0f, 2.0f, 0.0f}) == Approx(0.3f).margin(1e-3f));

    const math::Quat rotation = spline.evaluateRotation(0.5f);
    REQUIRE(math::dot(rotation * math::Vec3::forward, math::Vec3::right)
        == Approx(1.0f).margin(1e-4f));

    const BezierSpline vertical = BezierSpline::linear(
        {{0.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f}},
        false);
    const math::Vec3 tangent = vertical.evaluateTangent(0.5f);
    const math::Vec3 up = vertical.evaluateUp(0.5f);
    REQUIRE(approxVec(tangent, math::Vec3::up));
    REQUIRE(std::abs(math::dot(tangent, up)) < 1e-4f);
    REQUIRE(up.length() == Approx(1.0f));
}

TEST_CASE("Smooth Bezier spline rebuilds and reports a stable frame", "[unit][spline]")
{
    BezierSpline spline = BezierSpline::smooth(
        {
            {0.0f, 0.0f, -6.0f},
            {6.0f, 0.0f, -3.0f},
            {6.0f, 0.0f, 4.0f},
            {0.0f, 0.0f, 7.0f},
            {-6.0f, 0.0f, 3.0f},
            {-6.0f, 0.0f, -4.0f},
        },
        true);
    REQUIRE(spline.curveCount() == 6);
    REQUIRE(spline.length() > 20.0f);

    for (int index = 0; index <= 20; ++index)
    {
        const float t = static_cast<float>(index) / 20.0f;
        const math::Vec3 tangent = spline.evaluateTangent(t);
        const math::Vec3 up = spline.evaluateUp(t);
        REQUIRE(tangent.length() == Approx(1.0f).margin(1e-4f));
        REQUIRE(up.length() == Approx(1.0f).margin(1e-4f));
        REQUIRE(std::abs(math::dot(tangent, up)) < 1e-4f);
    }

    spline.setKnots({}, false);
    REQUIRE_FALSE(spline.isValid());
    REQUIRE(spline.length() == 0.0f);
    REQUIRE(spline.curveCount() == 0);

    spline.setKnots(
        {{{0.0f, 0.0f, 0.0f}, {}, {}, math::Quat::identity()},
         {{2.0f, 0.0f, 0.0f}, {}, {}, math::Quat::identity()}},
        false);
    REQUIRE(spline.isValid());
    REQUIRE(spline.length() == Approx(2.0f));
    spline.clear();
    REQUIRE_FALSE(spline.isValid());
}
