#include <cinerig/impulse/fixed_signal.hpp>

#include <algorithm>

namespace cinerig::impulse
{
namespace
{

float axisDuration(const math::Curve& curve)
{
    if (curve.size() <= 1)
        return 0.0f;
    return curve.keyframes.back().time - curve.keyframes.front().time;
}

} // namespace

float FixedSignal::signalDuration() const
{
    return std::max(
        axisDuration(xCurve),
        std::max(axisDuration(yCurve), axisDuration(zCurve)));
}

void FixedSignal::getSignal(
    float timeSinceSignalStart,
    math::Vec3& pos,
    math::Quat& rot) const
{
    pos = {
        xCurve.evaluateUnclamped(timeSinceSignalStart),
        yCurve.evaluateUnclamped(timeSinceSignalStart),
        zCurve.evaluateUnclamped(timeSinceSignalStart),
    };
    rot = math::Quat::identity();
}

} // namespace cinerig::impulse
