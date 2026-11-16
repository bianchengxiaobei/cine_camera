#include <cinerig/core/spline_helpers.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/math/vec.hpp>

namespace cinerig
{
namespace
{

float component(math::Vec3 value, int axis)
{
    switch (axis)
    {
    case 0:
        return value.x;
    case 1:
        return value.y;
    default:
        return value.z;
    }
}

void setComponent(math::Vec3& value, int axis, float result)
{
    switch (axis)
    {
    case 0:
        value.x = result;
        break;
    case 1:
        value.y = result;
        break;
    default:
        value.z = result;
        break;
    }
}

} // namespace

math::Vec3 bezier3(
    float t,
    math::Vec3 p0,
    math::Vec3 p1,
    math::Vec3 p2,
    math::Vec3 p3)
{
    t = math::clamp01(t);
    const float d = 1.0f - t;
    return p0 * (d * d * d)
        + p1 * (3.0f * d * d * t)
        + p2 * (3.0f * d * t * t)
        + p3 * (t * t * t);
}

math::Vec3 bezierTangent3(
    float t,
    math::Vec3 p0,
    math::Vec3 p1,
    math::Vec3 p2,
    math::Vec3 p3)
{
    t = math::clamp01(t);
    return (-3.0f * p0 + 9.0f * p1 - 9.0f * p2 + 3.0f * p3) * (t * t)
        + (6.0f * p0 - 12.0f * p1 + 6.0f * p2) * t
        - 3.0f * p0 + 3.0f * p1;
}

void bezierTangentWeights3(
    math::Vec3 p0,
    math::Vec3 p1,
    math::Vec3 p2,
    math::Vec3 p3,
    math::Vec3& w0,
    math::Vec3& w1,
    math::Vec3& w2)
{
    w0 = -3.0f * p0 + 9.0f * p1 - 9.0f * p2 + 3.0f * p3;
    w1 = 6.0f * p0 - 12.0f * p1 + 6.0f * p2;
    w2 = -3.0f * p0 + 3.0f * p1;
}

float bezier1(float t, float p0, float p1, float p2, float p3)
{
    t = math::clamp01(t);
    const float d = 1.0f - t;
    return d * d * d * p0
        + 3.0f * d * d * t * p1
        + 3.0f * d * t * t * p2
        + t * t * t * p3;
}

float bezierTangent1(float t, float p0, float p1, float p2, float p3)
{
    t = math::clamp01(t);
    return (-3.0f * p0 + 9.0f * p1 - 9.0f * p2 + 3.0f * p3) * t * t
        + (6.0f * p0 - 12.0f * p1 + 6.0f * p2) * t
        - 3.0f * p0 + 3.0f * p1;
}

void computeSmoothControlPoints(
    const std::vector<math::Vec3>& knots,
    std::vector<math::Vec3>& ctrl1,
    std::vector<math::Vec3>& ctrl2)
{
    const int numPoints = static_cast<int>(knots.size());
    ctrl1.assign(knots.size(), math::Vec3{});
    ctrl2.assign(knots.size(), math::Vec3{});

    if (numPoints <= 0)
        return;
    if (numPoints == 1)
    {
        ctrl1[0] = ctrl2[0] = knots[0];
        return;
    }
    if (numPoints == 2)
    {
        ctrl1[0] = math::lerp(knots[0], knots[1], 0.33333f);
        ctrl2[0] = math::lerp(knots[0], knots[1], 0.66666f);
        return;
    }

    const int n = numPoints - 1;
    std::vector<float> a(static_cast<std::size_t>(numPoints));
    std::vector<float> b(static_cast<std::size_t>(numPoints));
    std::vector<float> c(static_cast<std::size_t>(numPoints));
    std::vector<float> r(static_cast<std::size_t>(numPoints));

    for (int axis = 0; axis < 3; ++axis)
    {
        a[0] = 0.0f;
        b[0] = 2.0f;
        c[0] = 1.0f;
        r[0] = component(knots[0], axis) + 2.0f * component(knots[1], axis);

        for (int i = 1; i < n - 1; ++i)
        {
            a[static_cast<std::size_t>(i)] = 1.0f;
            b[static_cast<std::size_t>(i)] = 4.0f;
            c[static_cast<std::size_t>(i)] = 1.0f;
            r[static_cast<std::size_t>(i)] =
                4.0f * component(knots[static_cast<std::size_t>(i)], axis)
                + 2.0f * component(knots[static_cast<std::size_t>(i + 1)], axis);
        }

        a[static_cast<std::size_t>(n - 1)] = 2.0f;
        b[static_cast<std::size_t>(n - 1)] = 7.0f;
        c[static_cast<std::size_t>(n - 1)] = 0.0f;
        r[static_cast<std::size_t>(n - 1)] =
            8.0f * component(knots[static_cast<std::size_t>(n - 1)], axis)
            + component(knots[static_cast<std::size_t>(n)], axis);

        for (int i = 1; i < n; ++i)
        {
            const float m = a[static_cast<std::size_t>(i)]
                / b[static_cast<std::size_t>(i - 1)];
            b[static_cast<std::size_t>(i)] -= m * c[static_cast<std::size_t>(i - 1)];
            r[static_cast<std::size_t>(i)] -= m * r[static_cast<std::size_t>(i - 1)];
        }

        setComponent(
            ctrl1[static_cast<std::size_t>(n - 1)],
            axis,
            r[static_cast<std::size_t>(n - 1)] / b[static_cast<std::size_t>(n - 1)]);
        for (int i = n - 2; i >= 0; --i)
        {
            setComponent(
                ctrl1[static_cast<std::size_t>(i)],
                axis,
                (r[static_cast<std::size_t>(i)]
                    - c[static_cast<std::size_t>(i)]
                        * component(ctrl1[static_cast<std::size_t>(i + 1)], axis))
                    / b[static_cast<std::size_t>(i)]);
        }

        for (int i = 0; i < n; ++i)
        {
            setComponent(
                ctrl2[static_cast<std::size_t>(i)],
                axis,
                2.0f * component(knots[static_cast<std::size_t>(i + 1)], axis)
                    - component(ctrl1[static_cast<std::size_t>(i + 1)], axis));
        }
        setComponent(
            ctrl2[static_cast<std::size_t>(n - 1)],
            axis,
            0.5f * (component(knots[static_cast<std::size_t>(n)], axis)
                + component(ctrl1[static_cast<std::size_t>(n - 1)], axis)));
    }
}

void computeSmoothControlPointsLooped(
    const std::vector<math::Vec3>& knots,
    std::vector<math::Vec3>& ctrl1,
    std::vector<math::Vec3>& ctrl2)
{
    const int numPoints = static_cast<int>(knots.size());
    ctrl1.assign(knots.size(), math::Vec3{});
    ctrl2.assign(knots.size(), math::Vec3{});
    if (numPoints <= 0)
        return;
    if (numPoints == 1)
    {
        ctrl1[0] = ctrl2[0] = knots[0];
        return;
    }

    const int margin = std::min(4, numPoints - 1);
    std::vector<math::Vec3> knotLooped(
        static_cast<std::size_t>(numPoints + 2 * margin));
    for (int i = 0; i < margin; ++i)
        knotLooped[static_cast<std::size_t>(i)] =
            knots[static_cast<std::size_t>(numPoints - (margin - i))];
    for (int i = 0; i < numPoints; ++i)
        knotLooped[static_cast<std::size_t>(i + margin)] =
            knots[static_cast<std::size_t>(i)];
    for (int i = 0; i < margin; ++i)
        knotLooped[static_cast<std::size_t>(numPoints + margin + i)] =
            knots[static_cast<std::size_t>(i)];

    std::vector<math::Vec3> ctrl1Looped;
    std::vector<math::Vec3> ctrl2Looped;
    computeSmoothControlPoints(knotLooped, ctrl1Looped, ctrl2Looped);
    for (int i = 0; i < numPoints; ++i)
    {
        ctrl1[static_cast<std::size_t>(i)] =
            ctrl1Looped[static_cast<std::size_t>(i + margin)];
        ctrl2[static_cast<std::size_t>(i)] =
            ctrl2Looped[static_cast<std::size_t>(i + margin)];
    }
}

} // namespace cinerig
