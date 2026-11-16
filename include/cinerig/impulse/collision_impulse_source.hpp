#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <cinerig/engine/physics_world.hpp>
#include <cinerig/impulse/impulse_source.hpp>

namespace cinerig::impulse
{

class CollisionImpulseSource : public ImpulseSource
{
public:
    struct ImpactInfo
    {
        bool hasOther = true;
        std::uint32_t layer = 0;
        std::string_view tag{};
        bool otherHasRigidbody = false;
        float otherMass = 1.0f;
        math::Vec3 otherVelocity{};
        math::Vec3 relativeVelocity{};
    };

    explicit CollisionImpulseSource(ImpulseManager* manager = nullptr);

    LayerMask layerMask{1u};
    std::string ignoreTag{};
    bool useImpactDirection = false;
    bool scaleImpactWithMass = false;
    bool scaleImpactWithSpeed = false;
    bool enabled = true;
    bool hasRigidbody = false;
    float rigidbodyMass = 1.0f;
    math::Vec3 rigidbodyVelocity{};

    void reset();
    ImpulseEvent* onCollision(const ImpactInfo& impact);
    ImpulseEvent* onTrigger(const ImpactInfo& impact);

private:
    bool passesFilter(const ImpactInfo& impact) const;
    ImpulseEvent* generateImpactEvent(
        const ImpactInfo& impact,
        math::Vec3 relativeVelocity);
};

} // namespace cinerig::impulse
