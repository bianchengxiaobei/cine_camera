#include <cinerig/core/bezier_spline.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <cinerig/core/spline_helpers.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{
namespace
{

constexpr float kMinCurveLength = 1e-6f;

math::Vec3 normalizedOr(math::Vec3 value, math::Vec3 fallback)
{
    if (math::almostZero(value))
        return fallback;
    return normalize(value);
}

math::Vec3 orthogonalizedUp(math::Vec3 tangent, math::Vec3 preferred)
{
    math::Vec3 result = preferred - tangent * dot(preferred, tangent);
    if (math::almostZero(result))
    {
        result = math::Vec3::up - tangent * dot(math::Vec3::up, tangent);
        if (math::almostZero(result))
            result = math::Vec3::right - tangent * dot(math::Vec3::right, tangent);
    }
    return normalizedOr(result, math::Vec3::up);
}

} // namespace

BezierSpline::BezierSpline(std::vector<BezierKnot> knots, bool closed)
{
    setKnots(std::move(knots), closed);
}

BezierSpline BezierSpline::linear(
    const std::vector<math::Vec3>& points,
    bool closed)
{
    std::vector<BezierKnot> knots(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
        knots[index].position = points[index];

    if (points.size() >= 2)
    {
        const std::size_t curveCount = closed ? points.size() : points.size() - 1;
        for (std::size_t index = 0; index < curveCount; ++index)
        {
            const std::size_t next = (index + 1) % points.size();
            const math::Vec3 offset =
                (points[next] - points[index]) / 3.0f;
            knots[index].tangentOut = offset;
            knots[next].tangentIn = -offset;
        }
    }
    return BezierSpline(std::move(knots), closed);
}

BezierSpline BezierSpline::smooth(
    const std::vector<math::Vec3>& points,
    bool closed)
{
    if (points.size() < 3)
        return linear(points, closed);

    std::vector<BezierKnot> knots(points.size());
    for (std::size_t index = 0; index < points.size(); ++index)
        knots[index].position = points[index];

    std::vector<math::Vec3> ctrl1;
    std::vector<math::Vec3> ctrl2;
    if (closed)
    {
        computeSmoothControlPointsLooped(points, ctrl1, ctrl2);
        for (std::size_t index = 0; index < points.size(); ++index)
        {
            knots[index].tangentOut = ctrl1[index] - points[index];
            const std::size_t next = (index + 1) % points.size();
            knots[next].tangentIn = ctrl2[index] - points[next];
        }
    }
    else
    {
        computeSmoothControlPoints(points, ctrl1, ctrl2);
        for (std::size_t index = 0; index + 1 < points.size(); ++index)
        {
            knots[index].tangentOut = ctrl1[index] - points[index];
            knots[index + 1].tangentIn = ctrl2[index] - points[index + 1];
        }
    }
    return BezierSpline(std::move(knots), closed);
}

void BezierSpline::setKnots(std::vector<BezierKnot> knots, bool closed)
{
    knots_ = std::move(knots);
    closed_ = closed;
    rebuild();
}

void BezierSpline::clear()
{
    knots_.clear();
    curves_.clear();
    curveStartDistance_.clear();
    closed_ = false;
    length_ = 0.0f;
}

void BezierSpline::rebuild()
{
    curves_.clear();
    curveStartDistance_.clear();
    length_ = 0.0f;

    const std::size_t count = knots_.size();
    if (count < 2)
        return;

    const std::size_t curveCount = closed_ ? count : count - 1;
    curves_.reserve(curveCount);
    curveStartDistance_.reserve(curveCount);

    for (std::size_t index = 0; index < curveCount; ++index)
    {
        const std::size_t next = (index + 1) % count;
        Curve curve;
        curve.p0 = knots_[index].position;
        curve.p1 = knots_[index].position + knots_[index].tangentOut;
        curve.p2 = knots_[next].position + knots_[next].tangentIn;
        curve.p3 = knots_[next].position;

        math::Vec3 previous = curve.p0;
        float accumulated = 0.0f;
        for (int sample = 1; sample <= ArcSamplesPerCurve; ++sample)
        {
            const float parameter =
                static_cast<float>(sample) / static_cast<float>(ArcSamplesPerCurve);
            const math::Vec3 point = bezier3(
                parameter, curve.p0, curve.p1, curve.p2, curve.p3);
            accumulated += (point - previous).length();
            curve.sampleDistance[static_cast<std::size_t>(sample)] = accumulated;
            previous = point;
        }
        curve.length = accumulated;

        curveStartDistance_.push_back(length_);
        curves_.push_back(curve);
        length_ += curve.length;
    }
}

float BezierSpline::maxKnotPosition() const
{
    const float count = static_cast<float>(knots_.size());
    return closed_ && count >= 2.0f ? count : std::max(0.0f, count - 1.0f);
}

float BezierSpline::distanceAtLocalParameter(
    const Curve& curve,
    float parameter) const
{
    parameter = math::clamp01(parameter);
    if (curve.length <= kMinCurveLength)
        return 0.0f;
    const float scaled =
        parameter * static_cast<float>(ArcSamplesPerCurve);
    const int index = std::min(
        static_cast<int>(std::floor(scaled)),
        ArcSamplesPerCurve - 1);
    const float fraction = scaled - static_cast<float>(index);
    return math::lerp(
        curve.sampleDistance[static_cast<std::size_t>(index)],
        curve.sampleDistance[static_cast<std::size_t>(index + 1)],
        fraction);
}

float BezierSpline::localParameterAtDistance(
    const Curve& curve,
    float distance) const
{
    if (curve.length <= kMinCurveLength || distance <= 0.0f)
        return 0.0f;
    if (distance >= curve.length)
        return 1.0f;

    for (int sample = 1; sample <= ArcSamplesPerCurve; ++sample)
    {
        const float upper =
            curve.sampleDistance[static_cast<std::size_t>(sample)];
        if (upper < distance)
            continue;
        const float lower =
            curve.sampleDistance[static_cast<std::size_t>(sample - 1)];
        const float span = upper - lower;
        const float fraction = span > kMinCurveLength
            ? math::clamp01((distance - lower) / span)
            : 1.0f;
        return (static_cast<float>(sample - 1) + fraction)
            / static_cast<float>(ArcSamplesPerCurve);
    }
    return 1.0f;
}

BezierSpline::CurveLocation BezierSpline::locateNormalized(float t) const
{
    if (curves_.empty() || length_ <= kMinCurveLength)
        return {};

    t = math::clamp01(t);
    if (t <= 0.0f)
        return {0, 0.0f};
    if (t >= 1.0f)
        return {curves_.size() - 1, 1.0f};

    const float target = t * length_;
    const auto iterator = std::upper_bound(
        curveStartDistance_.begin(),
        curveStartDistance_.end(),
        target);
    std::size_t curve = 0;
    if (iterator != curveStartDistance_.begin())
        curve = static_cast<std::size_t>(iterator - curveStartDistance_.begin() - 1);
    curve = std::min(curve, curves_.size() - 1);

    const Curve& selected = curves_[curve];
    const float localDistance = target - curveStartDistance_[curve];
    return {curve, localParameterAtDistance(selected, localDistance)};
}

float BezierSpline::distanceAtKnot(float knot) const
{
    const float maximum = maxKnotPosition();
    if (curves_.empty() || maximum <= 0.0f)
        return 0.0f;

    knot = math::clamp(knot, 0.0f, maximum);
    if (knot >= maximum)
        return length_;

    const std::size_t curve = std::min(
        static_cast<std::size_t>(std::floor(knot)),
        curves_.size() - 1);
    return curveStartDistance_[curve]
        + distanceAtLocalParameter(
            curves_[curve],
            knot - static_cast<float>(curve));
}

float BezierSpline::normalizedAtKnot(float knot) const
{
    if (length_ <= kMinCurveLength)
        return 0.0f;
    return math::clamp01(distanceAtKnot(knot) / length_);
}

float BezierSpline::knotAtDistance(float distance) const
{
    const float maximum = maxKnotPosition();
    if (curves_.empty() || length_ <= kMinCurveLength || maximum <= 0.0f)
        return 0.0f;
    if (distance <= 0.0f)
        return 0.0f;
    if (distance >= length_)
        return maximum;

    const auto iterator = std::upper_bound(
        curveStartDistance_.begin(),
        curveStartDistance_.end(),
        distance);
    std::size_t curve = 0;
    if (iterator != curveStartDistance_.begin())
        curve = static_cast<std::size_t>(iterator - curveStartDistance_.begin() - 1);
    curve = std::min(curve, curves_.size() - 1);

    const float localDistance = distance - curveStartDistance_[curve];
    return std::min(
        maximum,
        static_cast<float>(curve)
            + localParameterAtDistance(curves_[curve], localDistance));
}

float BezierSpline::knotAtNormalized(float normalized) const
{
    if (normalized <= 0.0f)
        return 0.0f;
    if (normalized >= 1.0f)
        return maxKnotPosition();
    return knotAtDistance(normalized * length_);
}

math::Vec3 BezierSpline::evaluatePositionAt(const CurveLocation& location) const
{
    if (curves_.empty())
        return knots_.empty() ? math::Vec3{} : knots_.front().position;
    const Curve& curve = curves_[location.curve];
    return bezier3(
        location.localParameter,
        curve.p0,
        curve.p1,
        curve.p2,
        curve.p3);
}

math::Vec3 BezierSpline::evaluatePosition(float t) const
{
    if (curves_.empty())
        return knots_.empty() ? math::Vec3{} : knots_.front().position;
    return evaluatePositionAt(locateNormalized(t));
}

math::Vec3 BezierSpline::evaluateTangentAt(const CurveLocation& location) const
{
    if (curves_.empty())
        return math::Vec3::right;
    const Curve& curve = curves_[location.curve];
    math::Vec3 tangent = bezierTangent3(
        location.localParameter,
        curve.p0,
        curve.p1,
        curve.p2,
        curve.p3);
    if (math::almostZero(tangent))
        tangent = curve.p3 - curve.p0;
    if (math::almostZero(tangent))
        tangent = curve.p1 - curve.p0;
    if (math::almostZero(tangent))
        tangent = curve.p3 - curve.p2;
    return normalizedOr(tangent, math::Vec3::right);
}

math::Vec3 BezierSpline::evaluateTangent(float t) const
{
    if (curves_.empty())
        return math::Vec3::right;
    return evaluateTangentAt(locateNormalized(t));
}

math::Quat BezierSpline::rotationAt(const CurveLocation& location) const
{
    if (knots_.empty())
        return math::Quat::identity();
    const std::size_t start = location.curve;
    const std::size_t end = (start + 1) % knots_.size();
    return math::slerp(
        knots_[start].rotation,
        knots_[end].rotation,
        location.localParameter);
}

math::Vec3 BezierSpline::evaluateUp(float t) const
{
    if (curves_.empty() || knots_.empty())
        return math::Vec3::up;
    const CurveLocation location = locateNormalized(t);
    const math::Vec3 tangent = evaluateTangentAt(location);
    const math::Quat rotation = rotationAt(location);
    return orthogonalizedUp(tangent, rotation * math::Vec3::up);
}

math::Quat BezierSpline::evaluateRotation(float t) const
{
    if (curves_.empty() || knots_.empty())
        return math::Quat::identity();
    const CurveLocation location = locateNormalized(t);
    const math::Vec3 tangent = evaluateTangentAt(location);
    const math::Vec3 up = orthogonalizedUp(tangent, rotationAt(location) * math::Vec3::up);
    return math::lookRotation(tangent, up);
}

float BezierSpline::closestPoint(math::Vec3 worldPosition) const
{
    return closestPoint(worldPosition, ArcSamplesPerCurve, 20);
}

float BezierSpline::closestPoint(
    math::Vec3 worldPosition,
    int searchResolution,
    int searchIterations) const
{
    if (curves_.empty())
        return 0.0f;

    const int sampleCount = std::max(1, searchResolution)
        * std::max(1, static_cast<int>(curves_.size()));
    float bestDistance = std::numeric_limits<float>::max();
    float bestT = 0.0f;
    for (int sample = 0; sample <= sampleCount; ++sample)
    {
        const float t = static_cast<float>(sample) / static_cast<float>(sampleCount);
        const float distance = (evaluatePosition(t) - worldPosition).sqrMagnitude();
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

float BezierSpline::convertPosition(
    float position,
    PathIndexUnit from,
    PathIndexUnit to) const
{
    if (from == to)
        return position;

    float normalized = 0.0f;
    switch (from)
    {
    case PathIndexUnit::Distance:
        normalized = length_ > kMinCurveLength
            ? math::clamp01(position / length_)
            : 0.0f;
        break;
    case PathIndexUnit::Normalized:
        normalized = math::clamp01(position);
        break;
    case PathIndexUnit::Knot:
        normalized = normalizedAtKnot(position);
        break;
    }

    switch (to)
    {
    case PathIndexUnit::Distance:
        return normalized * length_;
    case PathIndexUnit::Normalized:
        return normalized;
    case PathIndexUnit::Knot:
        return knotAtNormalized(normalized);
    }
    return normalized;
}

} // namespace cinerig
