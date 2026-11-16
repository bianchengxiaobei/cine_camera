#include <cinerig/core/spline.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include <cinerig/core/spline_settings.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{
namespace
{

float maxKnotPosition(const ISpline& spline)
{
    const float count = static_cast<float>(spline.knotCount());
    return spline.closed() && count >= 2.0f ? count : std::max(0.0f, count - 1.0f);
}

int curveCount(const ISpline& spline)
{
    const std::size_t count = spline.knotCount();
    if (count < 2)
        return 1;
    if (spline.closed())
        return static_cast<int>(count);
    return static_cast<int>(count - 1);
}

float toNormalizedPosition(
    const ISpline& spline,
    float position,
    PathIndexUnit units)
{
    switch (units)
    {
    case PathIndexUnit::Distance:
    {
        const float totalLength = spline.length();
        return totalLength > 0.0f ? math::clamp01(position / totalLength) : 0.0f;
    }
    case PathIndexUnit::Normalized:
        return math::clamp01(position);
    case PathIndexUnit::Knot:
    {
        const float maxKnot = maxKnotPosition(spline);
        return maxKnot > 0.0f ? math::clamp01(position / maxKnot) : 0.0f;
    }
    }
    return 0.0f;
}

float fromNormalizedPosition(
    const ISpline& spline,
    float normalized,
    PathIndexUnit units)
{
    switch (units)
    {
    case PathIndexUnit::Distance:
        return normalized * spline.length();
    case PathIndexUnit::Normalized:
        return normalized;
    case PathIndexUnit::Knot:
        return normalized * maxKnotPosition(spline);
    }
    return normalized;
}

} // namespace

float ISpline::convertPosition(
    float position,
    PathIndexUnit from,
    PathIndexUnit to) const
{
    if (from == to)
        return position;
    return fromNormalizedPosition(*this, toNormalizedPosition(*this, position, from), to);
}

float ISpline::closestPoint(
    math::Vec3 worldPosition,
    int searchResolution,
    int searchIterations) const
{
    const int sampleCount = std::max(1, searchResolution) * curveCount(*this);
    float bestDistance = std::numeric_limits<float>::max();
    float bestT = 0.0f;
    for (int sample = 0; sample <= sampleCount; ++sample)
    {
        const float t =
            static_cast<float>(sample) / static_cast<float>(sampleCount);
        const float distance =
            (evaluatePosition(t) - worldPosition).sqrMagnitude();
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestT = t;
        }
    }

    const float step = 1.0f / static_cast<float>(sampleCount);
    float low = std::max(0.0f, bestT - step);
    float high = std::min(1.0f, bestT + step);
    for (int iteration = 0; iteration < std::max(0, searchIterations); ++iteration)
    {
        const float third = (high - low) / 3.0f;
        const float first = low + third;
        const float second = high - third;
        const float firstDistance =
            (evaluatePosition(first) - worldPosition).sqrMagnitude();
        const float secondDistance =
            (evaluatePosition(second) - worldPosition).sqrMagnitude();
        if (firstDistance <= secondDistance)
            high = second;
        else
            low = first;
    }
    return math::clamp01((low + high) * 0.5f);
}

float splineMaxPosition(const ISpline& spline, PathIndexUnit unit)
{
    switch (unit)
    {
    case PathIndexUnit::Distance:
        return std::max(0.0f, spline.length());
    case PathIndexUnit::Normalized:
        return 1.0f;
    case PathIndexUnit::Knot:
        return maxKnotPosition(spline);
    }
    return 0.0f;
}

float splineStandardizePosition(
    const ISpline& spline,
    float position,
    PathIndexUnit unit,
    float* maxPosition)
{
    const float maximum = splineMaxPosition(spline, unit);
    if (maxPosition != nullptr)
        *maxPosition = maximum;
    if (std::isnan(position))
        return 0.0f;
    if (!spline.closed() || maximum <= 0.0f)
        return math::clamp(position, 0.0f, maximum);

    position = std::fmod(position, maximum);
    if (position < 0.0f)
        position += maximum;
    return position;
}

float SplineSettings::maxPosition() const
{
    return spline == nullptr ? 0.0f : splineMaxPosition(*spline, units);
}

float SplineSettings::standardizedPosition(float* maxPositionOut) const
{
    if (spline == nullptr)
    {
        if (maxPositionOut != nullptr)
            *maxPositionOut = 0.0f;
        return 0.0f;
    }
    return splineStandardizePosition(*spline, position, units, maxPositionOut);
}

float SplineSettings::normalizedPosition() const
{
    if (spline == nullptr)
        return 0.0f;
    const float standardized = standardizedPosition();
    return spline->convertPosition(
        standardized,
        units,
        PathIndexUnit::Normalized);
}

void SplineSettings::changeUnitPreservePosition(PathIndexUnit newUnits)
{
    if (spline != nullptr && newUnits != units)
        position = spline->convertPosition(position, units, newUnits);
    units = newUnits;
}

} // namespace cinerig
