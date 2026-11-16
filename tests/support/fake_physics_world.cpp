#include "fake_physics_world.hpp"

#include <utility>

namespace cinerig::testing
{

std::optional<RaycastHit> FakePhysicsWorld::raycast(
    const Ray& ray, float maxDistance, const QueryFilter& filter) const
{
    ++raycastCalls;
    lastRay = ray;
    lastMaxDistance = maxDistance;
    lastFilter = filter;
    return nextRaycast;
}

std::optional<SphereCastHit> FakePhysicsWorld::sphereCast(
    const Ray& ray, float radius, float maxDistance, const QueryFilter& filter) const
{
    ++sphereCastCalls;
    lastRay = ray;
    lastSphereRadius = radius;
    lastMaxDistance = maxDistance;
    lastFilter = filter;
    if (!sphereCastResults.empty())
    {
        const std::optional<SphereCastHit> result = sphereCastResults.front();
        sphereCastResults.erase(sphereCastResults.begin());
        return result;
    }
    return nextSphereCast;
}

std::vector<OverlapHit> FakePhysicsWorld::overlapSphere(
    math::Vec3 center, float radius, const QueryFilter& filter) const
{
    ++overlapSphereCalls;
    lastOverlapCenter = center;
    lastOverlapRadius = radius;
    lastFilter = filter;
    return nextOverlapSphere;
}

std::optional<Bounds> FakePhysicsWorld::bounds(EntityId id) const
{
    ++boundsCalls;
    const auto it = boundsById.find(id);
    if (it != boundsById.end())
        return it->second;
    return defaultBounds;
}

void FakePhysicsWorld::reset()
{
    nextRaycast = std::nullopt;
    nextSphereCast = std::nullopt;
    sphereCastResults.clear();
    nextOverlapSphere.clear();
    defaultBounds = std::nullopt;
    boundsById.clear();
    raycastCalls = 0;
    sphereCastCalls = 0;
    overlapSphereCalls = 0;
    boundsCalls = 0;
    lastRay = Ray{};
    lastMaxDistance = 0.0f;
    lastSphereRadius = 0.0f;
    lastFilter = QueryFilter{};
    lastOverlapCenter = {};
    lastOverlapRadius = 0.0f;
}

}

