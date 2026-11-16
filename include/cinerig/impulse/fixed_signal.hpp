#pragma once

#include <cinerig/core/signal_source.hpp>
#include <cinerig/math/curve.hpp>

namespace cinerig::impulse
{

class FixedSignal final : public ISignalSource6D
{
public:
    math::Curve xCurve;
    math::Curve yCurve;
    math::Curve zCurve;

    float signalDuration() const override;
    void getSignal(
        float timeSinceSignalStart,
        math::Vec3& pos,
        math::Quat& rot) const override;
};

} // namespace cinerig::impulse
