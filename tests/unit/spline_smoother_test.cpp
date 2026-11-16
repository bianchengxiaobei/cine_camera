#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/spline_smoother.hpp>
#include <cinerig/core/bezier_spline.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

using namespace cinerig;
using Catch::Approx;

namespace
{

const std::vector<math::Vec3> kOpenPoints{
    {-8.0f, 1.0f, -5.0f},
    {-4.0f, 3.0f, -2.0f},
    {-5.0f, 2.0f, 3.0f},
    {0.0f, 4.0f, 5.0f},
    {4.0f, 1.0f, 1.0f},
    {8.0f, 3.0f, -3.0f},
};

const std::vector<math::Vec3> kClosedPoints{
    {-7.0f, 1.0f, -4.0f},
    {0.0f, 3.0f, -7.0f},
    {7.0f, 1.0f, -3.0f},
    {5.0f, 4.0f, 4.0f},
    {-2.0f, 2.0f, 6.0f},
    {-7.0f, 3.0f, 2.0f},
};

BezierSpline makeRaw(
    const std::vector<math::Vec3>& points,
    bool closed)
{
    BezierSpline spline = BezierSpline::linear(points, closed);
    std::vector<BezierKnot> knots = spline.knots();
    for (std::size_t index = 0; index < knots.size(); ++index)
    {
        knots[index].rotation = math::quatFromAxisAngle(
            math::Vec3::forward,
            0.15f * static_cast<float>(index));
    }
    spline.setKnots(std::move(knots), closed);
    return spline;
}

bool finiteQuat(math::Quat value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z)
        && std::isfinite(value.w);
}

bool finiteVec(math::Vec3 value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

} // namespace

TEST_CASE("Spline smoother defaults reset and short splines are no-ops", "[unit][spline][smoother]")
{
    SplineSmoother smoother;
    REQUIRE(smoother.spline == nullptr);
    REQUIRE(smoother.autoSmooth);
    REQUIRE_FALSE(smoother.isValid());
    REQUIRE_FALSE(smoother.smoothNow());
    REQUIRE_FALSE(smoother.onSplineModified());

    BezierSpline spline = BezierSpline::linear(
        {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}},
        false);
    const std::vector<BezierKnot> before = spline.knots();
    smoother.spline = &spline;
    smoother.autoSmooth = false;
    REQUIRE(smoother.isValid());
    REQUIRE_FALSE(smoother.smoothNow());
    REQUIRE_FALSE(smoother.onSplineModified());

    const std::vector<BezierKnot>& after = spline.knots();
    REQUIRE(after.size() == before.size());
    for (std::size_t index = 0; index < after.size(); ++index)
    {
        REQUIRE(math::approximately(after[index].position, before[index].position));
        REQUIRE(math::approximately(after[index].tangentIn, before[index].tangentIn));
        REQUIRE(math::approximately(after[index].tangentOut, before[index].tangentOut));
        REQUIRE(after[index].rotation.x == before[index].rotation.x);
        REQUIRE(after[index].rotation.y == before[index].rotation.y);
        REQUIRE(after[index].rotation.z == before[index].rotation.z);
        REQUIRE(after[index].rotation.w == before[index].rotation.w);
    }

    smoother.reset();
    REQUIRE(smoother.spline == nullptr);
    REQUIRE(smoother.autoSmooth);
}

TEST_CASE("Spline smoother matches open smooth control points and frames", "[unit][spline][smoother]")
{
    BezierSpline spline = makeRaw(kOpenPoints, false);
    const BezierSpline expected = BezierSpline::smooth(kOpenPoints, false);
    SplineSmoother smoother;
    smoother.spline = &spline;

    const math::Vec3 originalUp = math::rotate(
        spline.knots().front().rotation,
        math::Vec3::up);
    REQUIRE(smoother.smoothNow());

    const std::vector<BezierKnot>& knots = spline.knots();
    REQUIRE(knots.size() == kOpenPoints.size());
    REQUIRE(spline.length() == Approx(expected.length()).margin(1e-4f));
    for (std::size_t index = 0; index < knots.size(); ++index)
    {
        REQUIRE(math::approximately(knots[index].position, kOpenPoints[index]));
        REQUIRE(finiteQuat(knots[index].rotation));
        REQUIRE(finiteVec(knots[index].tangentIn));
        REQUIRE(finiteVec(knots[index].tangentOut));

        const math::Vec3 forward = math::normalize(
            math::rotate(knots[index].rotation, math::Vec3::forward));
        const math::Vec3 up = math::rotate(knots[index].rotation, math::Vec3::up);
        REQUIRE(std::abs(math::dot(forward, up)) < 1e-4f);

        if (index > 0 && index + 1 < knots.size())
        {
            REQUIRE(math::approximately(
                knots[index].tangentIn,
                -knots[index].tangentOut,
                2e-4f));
        }
    }

    REQUIRE(math::approximately(knots.front().tangentIn, math::Vec3::zero));
    REQUIRE(math::approximately(knots.back().tangentOut, math::Vec3::zero));
    REQUIRE(math::dot(
        math::rotate(knots.front().rotation, math::Vec3::forward),
        spline.evaluateTangent(0.0f)) > 0.999f);
    REQUIRE(math::dot(
        math::rotate(knots.back().rotation, math::Vec3::forward),
        spline.evaluateTangent(1.0f)) > 0.999f);
    REQUIRE(math::dot(
        math::rotate(knots.front().rotation, math::Vec3::up),
        originalUp) > 0.5f);

    for (int sample = 0; sample <= 32; ++sample)
    {
        const float t = static_cast<float>(sample) / 32.0f;
        REQUIRE(math::approximately(
            spline.evaluatePosition(t),
            expected.evaluatePosition(t),
            2e-4f));
    }

    const std::vector<BezierKnot> once = knots;
    REQUIRE(smoother.smoothNow());
    const std::vector<BezierKnot>& twice = spline.knots();
    for (std::size_t index = 0; index < twice.size(); ++index)
    {
        REQUIRE(math::approximately(twice[index].position, once[index].position));
        REQUIRE(math::approximately(twice[index].tangentIn, once[index].tangentIn));
        REQUIRE(math::approximately(twice[index].tangentOut, once[index].tangentOut));
        REQUIRE(std::abs(math::dot(twice[index].rotation, once[index].rotation))
            > 0.99999f);
    }
}

TEST_CASE("Spline smoother closes mirrored loops", "[unit][spline][smoother]")
{
    BezierSpline spline = makeRaw(kClosedPoints, true);
    const BezierSpline expected = BezierSpline::smooth(kClosedPoints, true);
    SplineSmoother smoother;
    smoother.spline = &spline;

    REQUIRE(smoother.smoothNow());
    REQUIRE(spline.closed());
    REQUIRE(spline.knotCount() == kClosedPoints.size());
    REQUIRE(spline.length() == Approx(expected.length()).margin(2e-2f));

    const std::vector<BezierKnot>& knots = spline.knots();
    for (std::size_t index = 0; index < knots.size(); ++index)
    {
        INFO("closed knot index " << index);
        REQUIRE(math::approximately(knots[index].position, kClosedPoints[index]));
        REQUIRE(math::approximately(
            knots[index].tangentIn,
            -knots[index].tangentOut,
            2e-4f));
        REQUIRE(finiteQuat(knots[index].rotation));
    }

    for (int sample = 0; sample <= 24; ++sample)
    {
        const float t = static_cast<float>(sample) / 24.0f;
        REQUIRE(math::approximately(
            spline.evaluatePosition(t),
            expected.evaluatePosition(t),
            2e-2f));
    }

    REQUIRE(math::dot(
        spline.evaluateTangent(0.0f),
        spline.evaluateTangent(1.0f)) > 0.9999f);
}

TEST_CASE("Spline smoother auto hook respects autoSmooth", "[unit][spline][smoother]")
{
    BezierSpline spline = makeRaw(kOpenPoints, false);
    const std::vector<BezierKnot> rawKnots = spline.knots();
    SplineSmoother smoother;
    smoother.spline = &spline;
    smoother.autoSmooth = false;

    REQUIRE_FALSE(smoother.onSplineModified());
    REQUIRE(math::approximately(spline.knots()[1].tangentIn, rawKnots[1].tangentIn));

    smoother.autoSmooth = true;
    REQUIRE(smoother.onSplineModified());
    REQUIRE_FALSE(math::approximately(
        spline.knots()[1].tangentIn,
        rawKnots[1].tangentIn));

    smoother.autoSmooth = false;
    REQUIRE(smoother.smoothNow());
}

TEST_CASE("Spline smoother survives degenerate and parallel inputs", "[unit][spline][smoother]")
{
    BezierSpline coincident = BezierSpline::linear(
        {
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            {2.0f, 0.0f, 0.0f},
        },
        false);
    SplineSmoother smoother;
    smoother.spline = &coincident;
    REQUIRE(smoother.smoothNow());
    for (const BezierKnot& knot : coincident.knots())
    {
        REQUIRE(finiteQuat(knot.rotation));
        REQUIRE(finiteVec(knot.tangentIn));
        REQUIRE(finiteVec(knot.tangentOut));
    }

    BezierSpline vertical = BezierSpline::linear(
        {
            {0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f, 0.0f},
            {0.0f, 4.0f, 0.0f},
            {1.0f, 6.0f, 0.0f},
        },
        false);
    smoother.spline = &vertical;
    REQUIRE(smoother.smoothNow());
    for (const BezierKnot& knot : vertical.knots())
    {
        REQUIRE(finiteQuat(knot.rotation));
        const math::Vec3 forward = math::rotate(
            knot.rotation,
            math::Vec3::forward);
        const math::Vec3 up = math::rotate(knot.rotation, math::Vec3::up);
        REQUIRE(finiteVec(forward));
        REQUIRE(finiteVec(up));
        REQUIRE(std::abs(math::dot(
            math::normalize(forward),
            math::normalize(up))) < 1e-4f);
    }
}
