#include <cinerig/core/spline_auto_dolly.hpp>

#include <algorithm>

namespace cinerig
{

void SplineAutoDolly::NearestPointToTarget::validate()
{
    searchResolution = std::max(searchResolution, 1);
    searchIteration = std::max(searchIteration, 1);
}

float SplineAutoDolly::FixedSpeed::getSplinePosition(
    const ISpline& spline,
    const std::optional<math::Vec3>& targetPosition,
    float currentPosition,
    PathIndexUnit positionUnits,
    float deltaTime)
{
    (void)targetPosition;
    (void)positionUnits;
    if (spline.knotCount() > 0 && deltaTime > 0.0f)
        return currentPosition + speed * deltaTime;
    return currentPosition;
}

float SplineAutoDolly::NearestPointToTarget::getSplinePosition(
    const ISpline& spline,
    const std::optional<math::Vec3>& targetPosition,
    float currentPosition,
    PathIndexUnit positionUnits,
    float deltaTime)
{
    (void)deltaTime;
    if (!targetPosition.has_value() || spline.knotCount() == 0)
        return currentPosition;

    float normalized = spline.closestPoint(
        *targetPosition,
        searchResolution,
        searchIteration);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    const float position = spline.convertPosition(
        normalized,
        PathIndexUnit::Normalized,
        positionUnits);
    return position + positionOffset;
}

void SplineAutoDolly::validate()
{
    if (method)
        method->validate();
}

void SplineAutoDolly::reset()
{
    if (method)
        method->reset();
}

bool SplineAutoDolly::requiresTrackingTarget() const
{
    return method != nullptr && method->requiresTrackingTarget();
}

float SplineAutoDolly::getSplinePosition(
    const ISpline& spline,
    const std::optional<math::Vec3>& targetPosition,
    float currentPosition,
    PathIndexUnit positionUnits,
    float deltaTime) const
{
    if (!enabled || !method)
        return currentPosition;
    return method->getSplinePosition(
        spline,
        targetPosition,
        currentPosition,
        positionUnits,
        deltaTime);
}

} // namespace cinerig
