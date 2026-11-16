#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include <cinerig/engine/physics_world.hpp>

namespace cinerig::testing
{

// Configurable deterministic physics adapter. It records the last query and
// returns the configured results without performing collision detection.
class FakePhysicsWorld final : public IPhysicsWorld
{
public:
    std::optional<RaycastHit> raycast(
        const Ray& ray, float maxDistance, const QueryFilter& filter) const override;

    std::optional<SphereCastHit> sphereCast(
        const Ray& ray, float radius, float maxDistance, const QueryFilter& filter) const override;

    std::vector<OverlapHit> overlapSphere(
        math::Vec3 center, float radius, const QueryFilter& filter) const override;

    std::optional<Bounds> bounds(EntityId id) const override;

    void reset();

    std::optional<RaycastHit> nextRaycast;
    std::optional<SphereCastHit> nextSphereCast;
    mutable std::vector<std::optional<SphereCastHit>> sphereCastResults;
    std::vector<OverlapHit> nextOverlapSphere;
    std::optional<Bounds> defaultBounds;
    std::unordered_map<EntityId, Bounds> boundsById;

    mutable int raycastCalls = 0;
    mutable int sphereCastCalls = 0;
    mutable int overlapSphereCalls = 0;
    mutable int boundsCalls = 0;
    mutable Ray lastRay{};
    mutable float lastMaxDistance = 0.0f;
    mutable float lastSphereRadius = 0.0f;
    mutable QueryFilter lastFilter{};
    mutable math::Vec3 lastOverlapCenter{};
    mutable float lastOverlapRadius = 0.0f;
};

}

