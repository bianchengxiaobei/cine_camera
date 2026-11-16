#include <cinerig/components/decollider.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

namespace
{

std::optional<float> rayBoundsEntry(
    const Bounds& bounds,
    math::Vec3 origin,
    math::Vec3 direction,
    float radius,
    float maxDistance)
{
    const math::Vec3 expandedExtents = bounds.extents + math::Vec3(radius);
    const math::Vec3 minimum = bounds.center - expandedExtents;
    const math::Vec3 maximum = bounds.center + expandedExtents;

    float nearDistance = 0.0f;
    float farDistance = maxDistance;
    for (int axis = 0; axis < 3; ++axis)
    {
        const float axisDirection = direction[axis];
        const float axisOrigin = origin[axis];
        if (std::abs(axisDirection) < math::Epsilon)
        {
            if (axisOrigin < minimum[axis] || axisOrigin > maximum[axis])
                return std::nullopt;
            continue;
        }

        float first = (minimum[axis] - axisOrigin) / axisDirection;
        float second = (maximum[axis] - axisOrigin) / axisDirection;
        if (first > second)
            std::swap(first, second);
        nearDistance = std::max(nearDistance, first);
        farDistance = std::min(farDistance, second);
        if (nearDistance > farDistance)
            return std::nullopt;
    }

    if (nearDistance <= math::Epsilon)
        return std::nullopt;
    return nearDistance;
}

} // namespace

void Decollider::reset()
{
    cameraRadius = 0.4f;
    decollision = DecollisionSettings{};
    terrainResolution = TerrainSettings{};
    extraStates_.clear();
}

void Decollider::validate()
{
    cameraRadius = std::max(0.01f, cameraRadius);
    decollision.damping = math::clamp(decollision.damping, 0.0f, 10.0f);
    decollision.smoothingTime =
        math::clamp(decollision.smoothingTime, 0.0f, 2.0f);
    terrainResolution.maximumRaycast =
        std::max(0.0f, terrainResolution.maximumRaycast);
    terrainResolution.damping =
        math::clamp(terrainResolution.damping, 0.0f, 10.0f);
}

float Decollider::maxDampTime() const
{
    float result = 0.0f;
    if (decollision.enabled)
        result = std::max(result, decollision.damping);
    if (terrainResolution.enabled)
        result = std::max(result, terrainResolution.damping);
    return result;
}

Decollider::ExtraState& Decollider::extraState(CameraId cameraId)
{
    return extraStates_[cameraId];
}

float Decollider::updateDistanceSmoothing(
    ExtraState& extra,
    float distance,
    float smoothingTime,
    bool haveDisplacement,
    float now)
{
    if (haveDisplacement
        && (extra.smoothedDistance == 0.0f || distance <= extra.smoothedDistance))
    {
        extra.smoothedDistance = distance;
        extra.smoothingStartTime = now;
    }

    if (extra.smoothingStartTime != 0.0f
        && now - extra.smoothingStartTime < smoothingTime)
    {
        distance = std::min(distance, extra.smoothedDistance);
    }

    if (!haveDisplacement
        && now - extra.smoothingStartTime >= smoothingTime)
    {
        extra.smoothedDistance = extra.smoothingStartTime = 0.0f;
    }

    return distance;
}

float Decollider::resolveTerrain(
    const IPhysicsWorld& physics,
    const ExtraState& extra,
    math::Vec3 cameraPosition,
    math::Vec3 up,
    float deltaTime) const
{
    if (!terrainResolution.enabled
        || terrainResolution.maximumRaycast <= math::Epsilon
        || terrainResolution.terrainLayers.bits == 0u)
    {
        return 0.0f;
    }

    const auto hit = physics.sphereCast(
        {cameraPosition + up * terrainResolution.maximumRaycast, -up},
        cameraRadius + math::Epsilon,
        terrainResolution.maximumRaycast,
        QueryFilter{terrainResolution.terrainLayers, 0});
    float displacement = hit.has_value()
        ? terrainResolution.maximumRaycast - hit->distance + math::Epsilon
        : 0.0f;

    if (deltaTime >= 0.0f
        && terrainResolution.damping > math::Epsilon
        && displacement < extra.previousTerrainDisplacement)
    {
        displacement = extra.previousTerrainDisplacement
            + math::standardDamp(
                displacement - extra.previousTerrainDisplacement,
                terrainResolution.damping,
                deltaTime);
    }
    return displacement;
}

math::Vec3 Decollider::decollideCamera(
    const IPhysicsWorld& physics,
    math::Vec3 cameraPosition,
    math::Vec3 targetPoint) const
{
    LayerMask layers = decollision.obstacleLayers;
    if (terrainResolution.enabled)
        layers.bits &= ~terrainResolution.terrainLayers.bits;
    if (layers.bits == 0u)
        return math::Vec3::zero;

    math::Vec3 direction = cameraPosition - targetPoint;
    const float capsuleLength = direction.length();
    if (capsuleLength <= math::Epsilon)
        return math::Vec3::zero;

    direction /= capsuleLength;
    const QueryFilter filter{layers, 0};
    const float overlapRadius = std::max(0.0f, cameraRadius - math::Epsilon);
    const auto overlaps = physics.overlapSphere(
        cameraPosition,
        overlapRadius,
        filter);
    if (overlaps.empty())
        return math::Vec3::zero;

    float bestDistance = std::numeric_limits<float>::max();
    for (const OverlapHit& overlap : overlaps)
    {
        std::optional<float> entryDistance;
        const auto bounds = physics.bounds(overlap.entity);
        if (bounds.has_value())
        {
            entryDistance = rayBoundsEntry(
                *bounds,
                targetPoint,
                direction,
                cameraRadius,
                capsuleLength + cameraRadius);
        }
        else
        {
            const auto hit = physics.raycast(
                {targetPoint, direction},
                capsuleLength + cameraRadius,
                filter);
            if (hit.has_value() && hit->entity == overlap.entity)
                entryDistance = hit->distance;
        }

        if (!entryDistance.has_value())
            continue;

        float distance = *entryDistance - cameraRadius;
        if (distance < cameraRadius)
        {
            distance = std::max(
                0.01f,
                distance + (cameraRadius - distance) * 0.5f);
        }
        bestDistance = std::min(bestDistance, distance);
    }

    if (bestDistance == std::numeric_limits<float>::max())
        return math::Vec3::zero;

    bestDistance = math::clamp(bestDistance, 0.01f, capsuleLength);
    return targetPoint + direction * bestDistance - cameraPosition;
}

math::Vec3 Decollider::applySmoothingAndDamping(
    math::Vec3 displacement,
    math::Vec3 targetPoint,
    math::Vec3 oldCameraPosition,
    ExtraState& extra,
    float deltaTime,
    float now) const
{
    math::Vec3 newOffset = oldCameraPosition + displacement - targetPoint;
    float newOffsetMagnitude = newOffset.length();

    if (deltaTime >= 0.0f && newOffsetMagnitude > cameraRadius)
    {
        const math::Vec3 newOffsetDirection = newOffset / newOffsetMagnitude;
        if (decollision.smoothingTime > math::Epsilon)
        {
            newOffsetMagnitude = updateDistanceSmoothing(
                extra,
                newOffsetMagnitude,
                decollision.smoothingTime,
                !math::almostZero(displacement),
                now);
            displacement =
                (targetPoint + newOffsetDirection * newOffsetMagnitude)
                - oldCameraPosition;
        }

        if (decollision.damping > math::Epsilon
            && newOffsetMagnitude > extra.previousDistanceFromTarget)
        {
            float previousOffsetMagnitude = extra.previousDistanceFromTarget;
            const float previousOffsetMagnitude2 =
                (oldCameraPosition - targetPoint).length()
                - extra.previousDecollisionDisplacement.length();
            if (std::abs(newOffsetMagnitude - previousOffsetMagnitude2)
                < std::abs(newOffsetMagnitude - previousOffsetMagnitude))
            {
                previousOffsetMagnitude = previousOffsetMagnitude2;
            }

            newOffsetMagnitude = previousOffsetMagnitude
                + math::standardDamp(
                    newOffsetMagnitude - previousOffsetMagnitude,
                    decollision.damping,
                    deltaTime);
            displacement =
                (targetPoint + newOffsetDirection * newOffsetMagnitude)
                - oldCameraPosition;
        }
    }

    extra.previousDistanceFromTarget = newOffsetMagnitude;
    extra.previousDecollisionDisplacement = displacement;
    return displacement;
}

void Decollider::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    if (stage != Stage::Body)
        return;

    ExtraState& extra = extraState(vcam.cameraId());
    const IPhysicsWorld* physics = vcam.physicsWorld();
    if (!vcam.previousStateIsValid)
        deltaTime = -1.0f;

    const math::Vec3 initialCameraPosition = state.correctedPosition();
    const math::Vec3 up = state.referenceUp;
    const bool hasLookAt = state.hasLookAt();
    const math::Vec3 lookAtPoint =
        hasLookAt ? state.referenceLookAt : initialCameraPosition;

    math::Vec3 resolutionTargetPoint = lookAtPoint;
    if (decollision.useFollowTarget.enabled)
    {
        const auto followPosition = vcam.followTargetPosition();
        if (followPosition.has_value())
        {
            const auto followRotation = vcam.followTargetRotation();
            const math::Vec3 offset = followRotation.has_value()
                ? math::rotate(
                    *followRotation,
                    math::Vec3::up * decollision.useFollowTarget.yOffset)
                : math::Vec3::up * decollision.useFollowTarget.yOffset;
            resolutionTargetPoint = *followPosition + offset;
        }
    }

    const math::Vec2 lookAtScreenOffset = hasLookAt
        ? math::getCameraRotationToTarget(
            state.rawOrientation,
            lookAtPoint - initialCameraPosition,
            up)
        : math::Vec2{};

    if (physics != nullptr)
    {
        extra.previousTerrainDisplacement = resolveTerrain(
            *physics,
            extra,
            state.correctedPosition(),
            up,
            deltaTime);
        state.positionCorrection += up * extra.previousTerrainDisplacement;

        if (decollision.enabled)
        {
            const math::Vec3 oldCameraPosition = state.correctedPosition();
            math::Vec3 displacement = decollideCamera(
                *physics,
                oldCameraPosition,
                resolutionTargetPoint);
            displacement = applySmoothingAndDamping(
                displacement,
                resolutionTargetPoint,
                oldCameraPosition,
                extra,
                deltaTime,
                vcam.currentTime());

            if (!math::almostZero(displacement))
            {
                state.positionCorrection += displacement;

                const float terrainDisplacement = terrainResolution.enabled
                    ? resolveTerrain(
                        *physics,
                        extra,
                        state.correctedPosition(),
                        up,
                        -1.0f)
                    : 0.0f;
                if (std::abs(terrainDisplacement) > math::Epsilon)
                {
                    state.positionCorrection += up * terrainDisplacement;
                    extra.previousTerrainDisplacement = 0.0f;
                }
            }
        }
    }

    const math::Vec3 newCameraPosition = state.correctedPosition();
    if (hasLookAt && !math::almostZero(initialCameraPosition - newCameraPosition))
    {
        math::Quat orientation = math::lookRotation(
            lookAtPoint - newCameraPosition,
            up);
        orientation = math::applyCameraRotation(
            orientation,
            {-lookAtScreenOffset.x, -lookAtScreenOffset.y},
            up);
        state.rawOrientation = orientation;

        if (deltaTime >= 0.0f)
        {
            const math::Vec3 previousDirection =
                extra.previousCorrectedCameraPosition - lookAtPoint;
            const math::Vec3 newDirection = newCameraPosition - lookAtPoint;
            if (previousDirection.sqrMagnitude() > math::Epsilon
                && newDirection.sqrMagnitude() > math::Epsilon)
            {
                state.rotationDampingBypass = math::safeFromToRotation(
                    previousDirection,
                    newDirection,
                    up);
            }
        }
    }
    extra.previousCorrectedCameraPosition = newCameraPosition;
}

} // namespace cinerig

