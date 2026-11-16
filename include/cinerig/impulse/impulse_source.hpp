#pragma once

#include <cinerig/impulse/impulse_definition.hpp>

namespace cinerig::impulse
{

class ImpulseSource
{
public:
    explicit ImpulseSource(ImpulseManager* manager = nullptr);
    virtual ~ImpulseSource() = default;

    ImpulseDefinition impulseDefinition{};
    math::Vec3 defaultVelocity{0.0f, -1.0f, 0.0f};
    math::Vec3 position{};
    ImpulseManager* impulseManager = nullptr;

    void reset();

    ImpulseEvent* generateImpulseAtPositionWithVelocity(
        math::Vec3 sourcePosition,
        math::Vec3 velocity);
    ImpulseEvent* generateImpulseWithVelocity(math::Vec3 velocity);
    ImpulseEvent* generateImpulseWithForce(float force);
    ImpulseEvent* generateImpulse();
};

} // namespace cinerig::impulse
