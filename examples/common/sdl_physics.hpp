#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <cinerig/engine/physics_world.hpp>

namespace cinerig::examples
{

// Lightweight AABB/sphere physics stub for examples and later P3 tests.
// It is not a replacement for a real collision engine.
class SdlPhysics final : public IPhysicsWorld
{
public:
    struct BoxShape
    {
        Bounds bounds;
        std::uint32_t layer = 0;
    };

    struct SphereShape
    {
        math::BoundingSphere sphere;
        std::uint32_t layer = 0;
    };

    void addBox(EntityId id, Bounds bounds, std::uint32_t layer = 0);
    void addSphere(EntityId id, math::BoundingSphere sphere, std::uint32_t layer = 0);
    void removeEntity(EntityId id);
    void clear();

    std::optional<RaycastHit> raycast(
        const Ray& ray, float maxDistance, const QueryFilter& filter) const override;
    std::optional<SphereCastHit> sphereCast(
        const Ray& ray, float radius, float maxDistance, const QueryFilter& filter) const override;
    std::vector<OverlapHit> overlapSphere(
        math::Vec3 center, float radius, const QueryFilter& filter) const override;
    std::optional<Bounds> bounds(EntityId id) const override;
    std::optional<math::Vec3> closestPoint(
        EntityId id, math::Vec3 point) const override;

private:
    std::unordered_map<EntityId, BoxShape> boxes_;
    std::unordered_map<EntityId, SphereShape> spheres_;
};

}

