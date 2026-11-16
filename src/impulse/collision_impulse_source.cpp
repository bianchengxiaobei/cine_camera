#include <cinerig/impulse/collision_impulse_source.hpp>

#include <cmath>

namespace cinerig::impulse
{

namespace
{

bool isZero(math::Vec3 value) noexcept
{
    return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}

} // namespace

CollisionImpulseSource::CollisionImpulseSource(ImpulseManager* manager)
    : ImpulseSource(manager)
{
    reset();
}

void CollisionImpulseSource::reset()
{
    ImpulseSource::reset();
    layerMask = LayerMask{1u};
    ignoreTag.clear();
    useImpactDirection = false;
    scaleImpactWithMass = false;
    scaleImpactWithSpeed = false;
    enabled = true;
    hasRigidbody = false;
    rigidbodyMass = 1.0f;
    rigidbodyVelocity = math::Vec3::zero;
}

bool CollisionImpulseSource::passesFilter(const ImpactInfo& impact) const
{
    if (!impact.hasOther)
        return true;
    if (!layerMask.contains(impact.layer))
        return false;
    if (!ignoreTag.empty() && std::string_view{ignoreTag} == impact.tag)
        return false;
    return true;
}

ImpulseEvent* CollisionImpulseSource::onCollision(const ImpactInfo& impact)
{
    return generateImpactEvent(impact, impact.relativeVelocity);
}

ImpulseEvent* CollisionImpulseSource::onTrigger(const ImpactInfo& impact)
{
    return generateImpactEvent(impact, math::Vec3::zero);
}

ImpulseEvent* CollisionImpulseSource::generateImpactEvent(
    const ImpactInfo& impact,
    math::Vec3 relativeVelocity)
{
    if (!enabled || !passesFilter(impact))
        return nullptr;

    const bool fillVelocity = isZero(relativeVelocity);
    const bool needsBodyData =
        scaleImpactWithMass || scaleImpactWithSpeed || useImpactDirection;
    float mass = 1.0f;
    if (needsBodyData)
    {
        if (hasRigidbody)
        {
            if (scaleImpactWithMass)
                mass *= rigidbodyMass;
            if (fillVelocity)
                relativeVelocity = -rigidbodyVelocity;
        }
        if (impact.hasOther && impact.otherHasRigidbody)
        {
            if (scaleImpactWithMass)
                mass *= impact.otherMass;
            if (fillVelocity)
                relativeVelocity += impact.otherVelocity;
        }
    }

    if (scaleImpactWithSpeed)
        mass *= std::sqrt(relativeVelocity.length());

    math::Vec3 direction = defaultVelocity;
    if (useImpactDirection && !math::almostZero(relativeVelocity))
    {
        direction = -relativeVelocity.normalized() * defaultVelocity.length();
    }

    return generateImpulseWithVelocity(direction * mass);
}

} // namespace cinerig::impulse
