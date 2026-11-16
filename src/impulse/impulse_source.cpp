#include <cinerig/impulse/impulse_source.hpp>

namespace cinerig::impulse
{

ImpulseSource::ImpulseSource(ImpulseManager* manager)
    : impulseManager(manager)
{
    reset();
}

void ImpulseSource::reset()
{
    impulseDefinition = ImpulseDefinition{};
    impulseDefinition.impulseChannel = 1u;
    impulseDefinition.impulseShape = ImpulseDefinition::ImpulseShape::Bump;
    impulseDefinition.impulseDuration = 0.2f;
    impulseDefinition.impulseType = ImpulseDefinition::ImpulseType::Uniform;
    impulseDefinition.dissipationDistance = 100.0f;
    impulseDefinition.dissipationRate = 0.25f;
    impulseDefinition.propagationSpeed = 343.0f;
    defaultVelocity = {0.0f, -1.0f, 0.0f};
}

ImpulseEvent* ImpulseSource::generateImpulseAtPositionWithVelocity(
    math::Vec3 sourcePosition,
    math::Vec3 velocity)
{
    if (impulseManager == nullptr)
        return nullptr;
    return impulseDefinition.createAndReturnEvent(
        *impulseManager,
        sourcePosition,
        velocity);
}

ImpulseEvent* ImpulseSource::generateImpulseWithVelocity(math::Vec3 velocity)
{
    return generateImpulseAtPositionWithVelocity(position, velocity);
}

ImpulseEvent* ImpulseSource::generateImpulseWithForce(float force)
{
    return generateImpulseWithVelocity(defaultVelocity * force);
}

ImpulseEvent* ImpulseSource::generateImpulse()
{
    return generateImpulseWithVelocity(defaultVelocity);
}

} // namespace cinerig::impulse
