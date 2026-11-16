#include <cinerig/core/orbit_rig.hpp>

#include <algorithm>

namespace cinerig
{

bool OrbitSplineCache::settingsChanged(const OrbitRigSettings& settings) const
{
    return !valid_ || !(settings_ == settings);
}

void OrbitSplineCache::update(const OrbitRigSettings& settings)
{
    settings_ = settings;
    valid_ = true;

    const float curvature = math::clamp01(settings.splineCurvature);
    knots_[1] = {
        0.0f,
        settings.bottom.height,
        -settings.bottom.radius,
        -1.0f,
    };
    knots_[2] = {
        0.0f,
        settings.center.height,
        -settings.center.radius,
        0.0f,
    };
    knots_[3] = {
        0.0f,
        settings.top.height,
        -settings.top.radius,
        1.0f,
    };
    knots_[0] = math::lerp(
        knots_[1] + (knots_[1] - knots_[2]) * 0.5f,
        math::Vec4::zero,
        curvature);
    knots_[4] = math::lerp(
        knots_[3] + (knots_[3] - knots_[2]) * 0.5f,
        math::Vec4::zero,
        curvature);

    constexpr int n = 4;
    std::array<float, 5> a{};
    std::array<float, 5> b{};
    std::array<float, 5> c{};
    std::array<float, 5> r{};

    for (int axis = 0; axis < 4; ++axis)
    {
        a[0] = 0.0f;
        b[0] = 2.0f;
        c[0] = 1.0f;
        r[0] = knots_[0][axis] + 2.0f * knots_[1][axis];

        for (int index = 1; index < n - 1; ++index)
        {
            a[index] = 1.0f;
            b[index] = 4.0f;
            c[index] = 1.0f;
            r[index] = 4.0f * knots_[index][axis]
                + 2.0f * knots_[index + 1][axis];
        }

        a[n - 1] = 2.0f;
        b[n - 1] = 7.0f;
        c[n - 1] = 0.0f;
        r[n - 1] = 8.0f * knots_[n - 1][axis] + knots_[n][axis];

        for (int index = 1; index < n; ++index)
        {
            const float multiplier = a[index] / b[index - 1];
            b[index] -= multiplier * c[index - 1];
            r[index] -= multiplier * r[index - 1];
        }

        control1_[n - 1][axis] = r[n - 1] / b[n - 1];
        for (int index = n - 2; index >= 0; --index)
            control1_[index][axis] =
                (r[index] - c[index] * control1_[index + 1][axis]) / b[index];

        for (int index = 0; index < n - 1; ++index)
            control2_[index][axis] =
                2.0f * knots_[index + 1][axis] - control1_[index + 1][axis];
        control2_[n - 1][axis] =
            0.5f * (knots_[n][axis] + control1_[n - 1][axis]);
    }
}

math::Vec4 OrbitSplineCache::splineValue(float t) const
{
    if (!valid_)
        return math::Vec4::zero;

    t = math::clamp01(t);
    int index = 1;
    if (t > 0.5f)
    {
        t -= 0.5f;
        index = 2;
    }

    const float localT = t * 2.0f;
    math::Vec4 result = bezier3(
        localT,
        knots_[index],
        control1_[index],
        control2_[index],
        knots_[index + 1]);
    result.w = bezier1(
        localT,
        knots_[index].w,
        control1_[index].w,
        control2_[index].w,
        knots_[index + 1].w);
    return result;
}

math::Vec4 OrbitSplineCache::bezier3(
    float t,
    math::Vec4 p0,
    math::Vec4 p1,
    math::Vec4 p2,
    math::Vec4 p3)
{
    t = math::clamp01(t);
    const float inverse = 1.0f - t;
    return p0 * (inverse * inverse * inverse)
        + p1 * (3.0f * inverse * inverse * t)
        + p2 * (3.0f * inverse * t * t)
        + p3 * (t * t * t);
}

float OrbitSplineCache::bezier1(float t, float p0, float p1, float p2, float p3)
{
    t = math::clamp01(t);
    const float inverse = 1.0f - t;
    return inverse * inverse * inverse * p0
        + 3.0f * inverse * inverse * t * p1
        + 3.0f * inverse * t * t * p2
        + t * t * t * p3;
}

const OrbitRigSettings& OrbitSplineCache::settings() const noexcept
{
    return settings_;
}

bool OrbitSplineCache::isValid() const noexcept
{
    return valid_;
}

} // namespace cinerig
