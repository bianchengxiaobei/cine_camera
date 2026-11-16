#include "sdl_physics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cinerig::examples
{
namespace
{

struct LocalHit
{
    float distance = 0.0f;
    math::Vec3 normal{};
};

bool segmentIntersectsBounds(
    math::Vec3 origin,
    math::Vec3 direction,
    float maxDistance,
    const Bounds& bounds,
    LocalHit& hit)
{
    float nearDistance = 0.0f;
    float farDistance = maxDistance;
    math::Vec3 nearNormal{};

    for (int axis = 0; axis < 3; ++axis)
    {
        const float directionComponent = direction[axis];
        const float originComponent = origin[axis];
        const float minimum = bounds.min()[axis];
        const float maximum = bounds.max()[axis];

        if (std::abs(directionComponent) < math::Epsilon)
        {
            if (originComponent < minimum || originComponent > maximum)
                return false;
            continue;
        }

        float first = (minimum - originComponent) / directionComponent;
        float second = (maximum - originComponent) / directionComponent;
        math::Vec3 firstNormal{};
        firstNormal[axis] = -1.0f;

        if (first > second)
        {
            std::swap(first, second);
            firstNormal[axis] = 1.0f;
        }

        if (first > nearDistance)
        {
            nearDistance = first;
            nearNormal = firstNormal;
        }
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance)
            return false;
    }

    hit.distance = nearDistance;
    hit.normal = nearNormal;
    return farDistance >= 0.0f;
}

bool segmentIntersectsSphere(
    math::Vec3 origin,
    math::Vec3 direction,
    float rayRadius,
    float maxDistance,
    math::BoundingSphere sphere,
    LocalHit& hit)
{
    const math::Vec3 toOrigin = origin - sphere.center;
    const float radius = sphere.radius + rayRadius;
    const float a = math::dot(direction, direction);
    if (a < math::Epsilon)
        return false;

    const float b = 2.0f * math::dot(toOrigin, direction);
    const float c = math::dot(toOrigin, toOrigin) - radius * radius;
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f)
        return false;

    const float root = std::sqrt(discriminant);
    float distance = (-b - root) / (2.0f * a);
    if (distance < 0.0f)
        distance = (-b + root) / (2.0f * a);
    if (distance < 0.0f || distance > maxDistance)
        return false;

    hit.distance = distance;
    hit.normal = math::normalize(origin + direction * distance - sphere.center);
    return true;
}

bool accepts(const QueryFilter& filter, std::uint32_t layer)
{
    return filter.layers.contains(layer);
}

float distanceToBounds(math::Vec3 point, const Bounds& bounds)
{
    const math::Vec3 closest = math::clamp(point, bounds.min(), bounds.max());
    return (point - closest).length();
}

}

void SdlPhysics::addBox(EntityId id, Bounds boundsValue, std::uint32_t layer)
{
    if (id != 0)
        boxes_[id] = BoxShape{boundsValue, layer};
}

void SdlPhysics::addSphere(EntityId id, math::BoundingSphere sphere, std::uint32_t layer)
{
    if (id != 0)
        spheres_[id] = SphereShape{sphere, layer};
}

void SdlPhysics::removeEntity(EntityId id)
{
    boxes_.erase(id);
    spheres_.erase(id);
}

void SdlPhysics::clear()
{
    boxes_.clear();
    spheres_.clear();
}

std::optional<RaycastHit> SdlPhysics::raycast(
    const Ray& ray, float maxDistance, const QueryFilter& filter) const
{
    const math::Vec3 direction = math::normalize(ray.direction);
    if (math::almostZero(direction) || maxDistance < 0.0f)
        return std::nullopt;

    bool found = false;
    RaycastHit best{};
    float bestDistance = maxDistance;

    for (const auto& [entity, shape] : boxes_)
    {
        if (entity == filter.ignoreEntity || !accepts(filter, shape.layer))
            continue;

        LocalHit hit;
        if (!segmentIntersectsBounds(ray.origin, direction, bestDistance, shape.bounds, hit))
            continue;
        if (hit.distance <= bestDistance)
        {
            bestDistance = hit.distance;
            best.entity = entity;
            best.point = ray.origin + direction * hit.distance;
            best.normal = hit.normal;
            best.distance = hit.distance;
            found = true;
        }
    }

    for (const auto& [entity, shape] : spheres_)
    {
        if (entity == filter.ignoreEntity || !accepts(filter, shape.layer))
            continue;

        LocalHit hit;
        if (!segmentIntersectsSphere(ray.origin, direction, 0.0f, bestDistance, shape.sphere, hit))
            continue;
        if (hit.distance <= bestDistance)
        {
            bestDistance = hit.distance;
            best.entity = entity;
            best.point = ray.origin + direction * hit.distance;
            best.normal = hit.normal;
            best.distance = hit.distance;
            found = true;
        }
    }

    return found ? std::optional<RaycastHit>{best} : std::nullopt;
}

std::optional<SphereCastHit> SdlPhysics::sphereCast(
    const Ray& ray, float radius, float maxDistance, const QueryFilter& filter) const
{
    const math::Vec3 direction = math::normalize(ray.direction);
    const float castRadius = std::max(0.0f, radius);
    if (math::almostZero(direction) || maxDistance < 0.0f)
        return std::nullopt;

    bool found = false;
    SphereCastHit best{};
    best.radius = castRadius;
    float bestDistance = maxDistance;

    for (const auto& [entity, shape] : boxes_)
    {
        if (entity == filter.ignoreEntity || !accepts(filter, shape.layer))
            continue;

        Bounds expanded = shape.bounds;
        expanded.extents += math::Vec3(castRadius);
        LocalHit hit;
        if (!segmentIntersectsBounds(ray.origin, direction, bestDistance, expanded, hit))
            continue;
        if (hit.distance <= bestDistance)
        {
            bestDistance = hit.distance;
            best.entity = entity;
            best.point = ray.origin + direction * hit.distance;
            best.normal = hit.normal;
            best.distance = hit.distance;
            found = true;
        }
    }

    for (const auto& [entity, shape] : spheres_)
    {
        if (entity == filter.ignoreEntity || !accepts(filter, shape.layer))
            continue;

        LocalHit hit;
        if (!segmentIntersectsSphere(
                ray.origin, direction, castRadius, bestDistance, shape.sphere, hit))
            continue;
        if (hit.distance <= bestDistance)
        {
            bestDistance = hit.distance;
            best.entity = entity;
            best.point = ray.origin + direction * hit.distance;
            best.normal = hit.normal;
            best.distance = hit.distance;
            found = true;
        }
    }

    return found ? std::optional<SphereCastHit>{best} : std::nullopt;
}

std::vector<OverlapHit> SdlPhysics::overlapSphere(
    math::Vec3 center, float radius, const QueryFilter& filter) const
{
    std::vector<OverlapHit> result;
    const float safeRadius = std::max(0.0f, radius);

    for (const auto& [entity, shape] : boxes_)
    {
        if (entity == filter.ignoreEntity || !accepts(filter, shape.layer))
            continue;
        const float distance = distanceToBounds(center, shape.bounds);
        if (distance <= safeRadius)
        {
            const math::Vec3 point = math::clamp(center, shape.bounds.min(), shape.bounds.max());
            result.push_back({entity, point, distance});
        }
    }

    for (const auto& [entity, shape] : spheres_)
    {
        if (entity == filter.ignoreEntity || !accepts(filter, shape.layer))
            continue;
        const math::Vec3 delta = center - shape.sphere.center;
        const float distance = std::max(0.0f, delta.length() - shape.sphere.radius);
        if (distance <= safeRadius)
        {
            const math::Vec3 point = math::almostZero(delta)
                ? shape.sphere.center
                : shape.sphere.center + math::normalize(delta) * shape.sphere.radius;
            result.push_back({entity, point, distance});
        }
    }

    return result;
}

std::optional<Bounds> SdlPhysics::bounds(EntityId id) const
{
    const auto box = boxes_.find(id);
    if (box != boxes_.end())
        return box->second.bounds;

    const auto sphere = spheres_.find(id);
    if (sphere != spheres_.end())
        return Bounds{sphere->second.sphere.center, math::Vec3(sphere->second.sphere.radius)};

    return std::nullopt;
}

std::optional<math::Vec3> SdlPhysics::closestPoint(
    EntityId id, math::Vec3 point) const
{
    std::optional<math::Vec3> result;
    float bestDistanceSquared = std::numeric_limits<float>::max();

    const auto box = boxes_.find(id);
    if (box != boxes_.end())
    {
        const math::Vec3 candidate = math::clamp(
            point,
            box->second.bounds.min(),
            box->second.bounds.max());
        bestDistanceSquared = (candidate - point).sqrMagnitude();
        result = candidate;
    }

    const auto sphere = spheres_.find(id);
    if (sphere != spheres_.end())
    {
        const math::Vec3 delta = point - sphere->second.sphere.center;
        const float distance = delta.length();
        const math::Vec3 candidate = distance <= sphere->second.sphere.radius
            ? point
            : sphere->second.sphere.center
                + math::normalize(delta) * sphere->second.sphere.radius;
        const float candidateDistanceSquared = (candidate - point).sqrMagnitude();
        if (!result.has_value() || candidateDistanceSquared < bestDistanceSquared)
            result = candidate;
    }

    return result;
}

} // namespace cinerig::examples

