#pragma once

#include <optional>
#include <vector>

#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

using Ray = math::Ray;
using Bounds = math::Bounds;

struct LayerMask
{
    std::uint32_t bits = 0xFFFFFFFFu;

    constexpr bool contains(std::uint32_t layer) const noexcept
    {
        return layer < 32u && (bits & (std::uint32_t{1} << layer)) != 0u;
    }
};

struct QueryFilter
{
    LayerMask layers;
    EntityId ignoreEntity = 0;
};

struct RaycastHit
{
    EntityId entity = 0;
    math::Vec3 point{};
    math::Vec3 normal{};
    float distance = 0.0f;
};

struct SphereCastHit : RaycastHit
{
    float radius = 0.0f;
};

struct OverlapHit
{
    EntityId entity = 0;
    math::Vec3 point{};
    float distance = 0.0f;
};

// Non-owning physics adapter. The host owns the collision world and all
// returned data. Implementations must ignore ignoreEntity where applicable;
// overlap results are unordered.
class IPhysicsWorld
{
public:
    virtual ~IPhysicsWorld() = default;

    virtual std::optional<RaycastHit> raycast(
        const Ray& ray, float maxDistance, const QueryFilter& filter) const = 0;

    virtual std::optional<SphereCastHit> sphereCast(
        const Ray& ray, float radius, float maxDistance, const QueryFilter& filter) const = 0;

    virtual std::vector<OverlapHit> overlapSphere(
        math::Vec3 center, float radius, const QueryFilter& filter) const = 0;

    virtual std::optional<Bounds> bounds(EntityId id) const = 0;

    // Default implementation clamps to bounds. Adapters with exact collider
    // shapes should override this. If point is inside the collider, return it.
    virtual std::optional<math::Vec3> closestPoint(
        EntityId id,
        math::Vec3 point) const
    {
        const std::optional<Bounds> volume = bounds(id);
        if (!volume.has_value())
            return std::nullopt;
        return math::clamp(point, volume->min(), volume->max());
    }
};

}
