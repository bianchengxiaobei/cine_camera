#pragma once

#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class ISignalSource6D
{
public:
    virtual ~ISignalSource6D() = default;

    virtual float signalDuration() const = 0;
    virtual void getSignal(float timeSinceSignalStart, math::Vec3& pos, math::Quat& rot) const = 0;
};

} // namespace cinerig
