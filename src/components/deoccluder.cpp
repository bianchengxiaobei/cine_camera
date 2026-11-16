#include <cinerig/components/deoccluder.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

namespace
{

constexpr float kPrecisionSlush = 0.001f;
constexpr float kAngleThreshold = 0.1f;

math::Plane planeThrough(math::Vec3 normal, math::Vec3 point)
{
    return {normal, -math::dot(normal, point)};
}

LayerMask blockingLayers(const Deoccluder& deoccluder)
{
    return LayerMask{deoccluder.collideAgainst.bits & ~deoccluder.transparentLayers.bits};
}

QueryFilter filterFor(const Deoccluder& deoccluder, LayerMask layers)
{
    return QueryFilter{layers, deoccluder.ignoreEntity};
}

float clampRayToBounds(const Ray& ray, float distance, const Bounds& bounds)
{
    const auto applyPlane = [&](math::Plane plane) {
        float hitDistance = 0.0f;
        if (plane.raycast(ray, hitDistance) && hitDistance > math::Epsilon)
            distance = std::min(distance, hitDistance);
    };

    if (math::dot(ray.direction, math::Vec3::up) > 0.0f)
        applyPlane(planeThrough(math::Vec3::down, bounds.max()));
    else if (math::dot(ray.direction, math::Vec3::down) > 0.0f)
        applyPlane(planeThrough(math::Vec3::up, bounds.min()));

    if (math::dot(ray.direction, math::Vec3::right) > 0.0f)
        applyPlane(planeThrough(math::Vec3::left, bounds.max()));
    else if (math::dot(ray.direction, math::Vec3::left) > 0.0f)
        applyPlane(planeThrough(math::Vec3::right, bounds.min()));

    if (math::dot(ray.direction, math::Vec3::back) > 0.0f)
        applyPlane(planeThrough(math::Vec3::forward, bounds.max()));
    else if (math::dot(ray.direction, math::Vec3::forward) > 0.0f)
        applyPlane(planeThrough(math::Vec3::back, bounds.min()));

    return distance;
}

bool isZero(math::Vec3 value)
{
    return math::almostZero(value);
}

} // namespace

void Deoccluder::reset()
{
    collideAgainst = LayerMask{1u};
    ignoreEntity = 0;
    transparentLayers = LayerMask{0u};
    minimumDistanceFromTarget = 0.3f;
    avoidObstacles = ObstacleAvoidance{};
    qualityEvaluation = QualityEvaluation{};
    extraStates_.clear();
}

void Deoccluder::validate()
{
    avoidObstacles.distanceLimit = std::max(0.0f, avoidObstacles.distanceLimit);
    avoidObstacles.minimumOcclusionTime =
        std::max(0.0f, avoidObstacles.minimumOcclusionTime);
    avoidObstacles.cameraRadius = std::max(0.0f, avoidObstacles.cameraRadius);
    avoidObstacles.smoothingTime =
        math::clamp(avoidObstacles.smoothingTime, 0.0f, 2.0f);
    avoidObstacles.damping = math::clamp(avoidObstacles.damping, 0.0f, 10.0f);
    avoidObstacles.dampingWhenOccluded =
        math::clamp(avoidObstacles.dampingWhenOccluded, 0.0f, 10.0f);
    avoidObstacles.maximumEffort = std::clamp(
        avoidObstacles.maximumEffort,
        1,
        10);
    minimumDistanceFromTarget = std::max(0.01f, minimumDistanceFromTarget);

    qualityEvaluation.nearLimit = std::max(0.1f, qualityEvaluation.nearLimit);
    qualityEvaluation.farLimit =
        std::max(qualityEvaluation.nearLimit, qualityEvaluation.farLimit);
    qualityEvaluation.optimalDistance = math::clamp(
        qualityEvaluation.optimalDistance,
        qualityEvaluation.nearLimit,
        qualityEvaluation.farLimit);
    qualityEvaluation.maxQualityBoost =
        std::max(0.0f, qualityEvaluation.maxQualityBoost);
}

bool Deoccluder::isTargetObscured(CameraId cameraId) const
{
    const ExtraState* extra = findExtraState(cameraId);
    return extra != nullptr && extra->targetObscured;
}

bool Deoccluder::cameraWasDisplaced(CameraId cameraId) const
{
    return cameraDisplacementDistance(cameraId) > 0.0f;
}

float Deoccluder::cameraDisplacementDistance(CameraId cameraId) const
{
    const ExtraState* extra = findExtraState(cameraId);
    return extra == nullptr ? 0.0f : extra->previousDisplacement.length();
}

float Deoccluder::maxDampTime() const
{
    if (!avoidObstacles.enabled)
        return 0.0f;
    return std::max(
        avoidObstacles.damping,
        std::max(avoidObstacles.dampingWhenOccluded, avoidObstacles.smoothingTime));
}

Deoccluder::ExtraState& Deoccluder::extraState(CameraId cameraId)
{
    return extraStates_[cameraId];
}

const Deoccluder::ExtraState* Deoccluder::findExtraState(CameraId cameraId) const
{
    const auto iterator = extraStates_.find(cameraId);
    return iterator == extraStates_.end() ? nullptr : &iterator->second;
}

void Deoccluder::resetDistanceSmoothing(
    ExtraState& extra,
    float smoothingTime,
    float now)
{
    if (now - extra.smoothedTime >= smoothingTime)
        extra.smoothedDistance = extra.smoothedTime = 0.0f;
}

float Deoccluder::applyDistanceSmoothing(
    const ExtraState& extra,
    float distance,
    float smoothingTime,
    float now)
{
    if (extra.smoothedTime != 0.0f && smoothingTime > math::Epsilon
        && now - extra.smoothedTime < smoothingTime)
    {
        return std::min(distance, extra.smoothedDistance);
    }
    return distance;
}

void Deoccluder::updateDistanceSmoothing(
    ExtraState& extra,
    float distance,
    float now)
{
    if (!extra.stateIsValid || extra.smoothedDistance == 0.0f
        || distance < extra.smoothedDistance)
    {
        extra.smoothedDistance = distance;
        extra.smoothedTime = now;
    }
}

void Deoccluder::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    const CameraId cameraId = vcam.cameraId();
    ExtraState& extra = extraState(cameraId);

    if (stage == Stage::Body)
    {
        extra.targetObscured = false;
        if (!vcam.previousStateIsValid || deltaTime < 0.0f)
            extra.stateIsValid = false;

        const IPhysicsWorld* physics = vcam.physicsWorld();
        if (!avoidObstacles.enabled || physics == nullptr)
        {
            extra.stateIsValid = false;
            return;
        }

        const math::Vec3 initialCameraPosition = state.correctedPosition();
        const math::Vec3 up = state.referenceUp;
        const bool hasLookAt = state.hasLookAt();
        const math::Vec3 lookAtPoint =
            hasLookAt ? state.referenceLookAt : initialCameraPosition;

        bool hasResolutionTarget = hasLookAt;
        math::Vec3 resolutionTargetPoint = lookAtPoint;
        if (avoidObstacles.useFollowTarget.enabled)
        {
            const auto followPosition = vcam.followTargetPosition();
            if (followPosition.has_value())
            {
                hasResolutionTarget = true;
                const auto followRotation = vcam.followTargetRotation();
                const math::Vec3 offset = followRotation.has_value()
                    ? math::rotate(
                        *followRotation,
                        math::Vec3::up * avoidObstacles.useFollowTarget.yOffset)
                    : math::Vec3::up * avoidObstacles.useFollowTarget.yOffset;
                resolutionTargetPoint = *followPosition + offset;
            }
        }

        const math::Vec2 lookAtScreenOffset = hasLookAt
            ? math::getCameraRotationToTarget(
                state.rawOrientation,
                lookAtPoint - initialCameraPosition,
                up)
            : math::Vec2{};

        if (extra.stateIsValid)
        {
            extra.previousDisplacement = math::rotate(
                state.rotationDampingBypass,
                extra.previousDisplacement);
        }

        math::Vec3 displacement = hasResolutionTarget
            ? preserveLineOfSight(
                state,
                *physics,
                resolutionTargetPoint)
            : math::Vec3::zero;

        if (avoidObstacles.minimumOcclusionTime > math::Epsilon)
        {
            const float now = vcam.currentTime();
            if (isZero(displacement))
            {
                extra.occlusionStartTime = 0.0f;
            }
            else
            {
                if (extra.occlusionStartTime <= 0.0f)
                    extra.occlusionStartTime = now;
                if (extra.stateIsValid
                    && now - extra.occlusionStartTime
                        < avoidObstacles.minimumOcclusionTime)
                {
                    displacement = extra.previousDisplacement;
                }
            }
        }

        if (hasResolutionTarget && avoidObstacles.smoothingTime > math::Epsilon)
        {
            const float now = vcam.currentTime();
            if (!extra.stateIsValid)
                resetDistanceSmoothing(extra, 0.0f, now);

            const math::Vec3 position = initialCameraPosition + displacement;
            math::Vec3 direction = position - resolutionTargetPoint;
            float distance = direction.length();
            if (distance > math::Epsilon)
            {
                direction /= distance;
                if (!isZero(displacement))
                    updateDistanceSmoothing(extra, distance, now);
                distance = applyDistanceSmoothing(
                    extra,
                    distance,
                    avoidObstacles.smoothingTime,
                    now);
                displacement +=
                    (resolutionTargetPoint + direction * distance) - position;
            }
        }

        if (isZero(displacement))
        {
            resetDistanceSmoothing(
                extra,
                avoidObstacles.smoothingTime,
                vcam.currentTime());
        }

        const math::Vec3 radiusAdjustedPosition =
            initialCameraPosition + displacement;
        if (avoidObstacles.strategy != ResolutionStrategy::PullCameraForward)
        {
            displacement += respectCameraRadius(
                *physics,
                radiusAdjustedPosition,
                resolutionTargetPoint);
        }

        float dampTime = avoidObstacles.dampingWhenOccluded;
        if (hasResolutionTarget && extra.stateIsValid
            && avoidObstacles.dampingWhenOccluded + avoidObstacles.damping
                > math::Epsilon)
        {
            const float displacementMagnitude = displacement.sqrMagnitude();
            const float previousMagnitude =
                extra.previousDisplacement.sqrMagnitude();
            if (std::abs(displacementMagnitude - previousMagnitude)
                > math::Epsilon * math::Epsilon)
            {
                dampTime = displacementMagnitude > previousMagnitude
                    ? avoidObstacles.dampingWhenOccluded
                    : avoidObstacles.damping;

                if (displacementMagnitude < math::Epsilon
                    && dampTime < extra.previousDampTime)
                {
                    dampTime = extra.previousDampTime
                        + math::standardDamp(
                            dampTime - extra.previousDampTime,
                            dampTime,
                            deltaTime);
                }

                if (avoidObstacles.strategy == ResolutionStrategy::PullCameraForward)
                {
                    const math::Vec3 newOffset =
                        initialCameraPosition + displacement - resolutionTargetPoint;
                    const float newOffsetMagnitude = newOffset.length();
                    if (newOffsetMagnitude > math::Epsilon)
                    {
                        const math::Vec3 newOffsetDirection =
                            newOffset / newOffsetMagnitude;
                        float previousOffsetMagnitude =
                            extra.previousCameraOffset.length();
                        const float previousOffsetMagnitude2 =
                            (initialCameraPosition - resolutionTargetPoint).length()
                            - std::sqrt(previousMagnitude);
                        if (std::abs(newOffsetMagnitude - previousOffsetMagnitude2)
                            < std::abs(newOffsetMagnitude - previousOffsetMagnitude))
                        {
                            previousOffsetMagnitude = previousOffsetMagnitude2;
                        }

                        const float dampedMagnitude = previousOffsetMagnitude
                            + math::standardDamp(
                                newOffsetMagnitude - previousOffsetMagnitude,
                                dampTime,
                                deltaTime);
                        const math::Vec3 newCameraPosition =
                            resolutionTargetPoint + newOffsetDirection * dampedMagnitude;
                        displacement = newCameraPosition - initialCameraPosition;
                    }
                }
                else
                {
                    math::Vec3 previousDisplacement =
                        resolutionTargetPoint
                        + math::rotate(
                            state.rotationDampingBypass,
                            extra.previousCameraOffset)
                        - initialCameraPosition;
                    if (previousDisplacement.sqrMagnitude() > previousMagnitude)
                        previousDisplacement = extra.previousDisplacement;

                    displacement = previousDisplacement
                        + math::standardDamp(
                            displacement - previousDisplacement,
                            dampTime,
                            deltaTime);
                }
            }
        }

        state.positionCorrection += displacement;
        const math::Vec3 newCameraPosition = state.correctedPosition();

        if (hasLookAt && displacement.sqrMagnitude() > math::Epsilon)
        {
            math::Quat orientation = math::lookRotation(
                lookAtPoint - newCameraPosition,
                up);
            orientation = math::applyCameraRotation(
                orientation,
                {-lookAtScreenOffset.x, -lookAtScreenOffset.y},
                up);
            state.rawOrientation = orientation;

            if (extra.stateIsValid)
            {
                const math::Vec3 previousDirection =
                    extra.previousCameraPosition - lookAtPoint;
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

        extra.previousDisplacement = displacement;
        extra.previousCameraOffset = newCameraPosition - resolutionTargetPoint;
        extra.previousCameraPosition = newCameraPosition;
        extra.previousDampTime = dampTime;
        extra.stateIsValid = true;
    }

    if (stage == Stage::Finalize && qualityEvaluation.enabled && state.hasLookAt())
    {
        const IPhysicsWorld* physics = vcam.physicsWorld();
        extra.targetObscured = state.isTargetOffscreen()
            || (physics != nullptr && isTargetObscured(state, *physics));

        if (extra.targetObscured)
            state.shotQuality *= 0.2f;
        if (extra.stateIsValid && !isZero(extra.previousDisplacement))
            state.shotQuality *= 0.8f;

        float nearnessBoost = 0.0f;
        if (qualityEvaluation.optimalDistance > 0.0f)
        {
            float distance =
                (state.referenceLookAt - state.finalPosition()).length();
            if (distance <= qualityEvaluation.optimalDistance)
            {
                if (distance >= qualityEvaluation.nearLimit)
                {
                    const float range = qualityEvaluation.optimalDistance
                        - qualityEvaluation.nearLimit;
                    nearnessBoost = range > math::Epsilon
                        ? qualityEvaluation.maxQualityBoost
                            * (distance - qualityEvaluation.nearLimit) / range
                        : qualityEvaluation.maxQualityBoost;
                }
            }
            else
            {
                distance -= qualityEvaluation.optimalDistance;
                if (distance < qualityEvaluation.farLimit)
                {
                    nearnessBoost = qualityEvaluation.maxQualityBoost
                        * (1.0f - distance / qualityEvaluation.farLimit);
                }
            }
            state.shotQuality *= 1.0f + nearnessBoost;
        }
    }
}

math::Vec3 Deoccluder::preserveLineOfSight(
    CameraState& state,
    const IPhysicsWorld& physics,
    math::Vec3 targetPoint)
{
    const LayerMask layers = blockingLayers(*this);
    if (layers.bits == 0u)
        return math::Vec3::zero;

    const math::Vec3 cameraPosition = state.correctedPosition();
    math::Vec3 pulledPosition{};
    SphereCastHit hit{};
    if (!pullCameraInFrontOfNearestObstacle(
            physics,
            cameraPosition,
            targetPoint,
            pulledPosition,
            hit))
    {
        return math::Vec3::zero;
    }

    math::Vec3 newPosition = pulledPosition;
    if (avoidObstacles.strategy != ResolutionStrategy::PullCameraForward)
    {
        const math::Vec3 targetToCamera = cameraPosition - targetPoint;
        newPosition = pushCameraBack(
            physics,
            pulledPosition,
            targetToCamera,
            hit,
            targetPoint,
            planeThrough(state.referenceUp, cameraPosition),
            targetToCamera.length(),
            avoidObstacles.maximumEffort);
    }
    return newPosition - cameraPosition;
}

bool Deoccluder::pullCameraInFrontOfNearestObstacle(
    const IPhysicsWorld& physics,
    math::Vec3 cameraPosition,
    math::Vec3 targetPoint,
    math::Vec3& newPosition,
    SphereCastHit& hit) const
{
    newPosition = cameraPosition;
    math::Vec3 direction = cameraPosition - targetPoint;
    const float targetDistance = direction.length();
    if (targetDistance <= math::Epsilon)
        return false;

    direction /= targetDistance;
    const float minimumDistance =
        minimumDistanceFromTarget + avoidObstacles.cameraRadius + kPrecisionSlush;
    if (targetDistance <= minimumDistance)
        return false;

    float rayLength = std::max(
        targetDistance - minimumDistance - avoidObstacles.cameraRadius,
        kPrecisionSlush);
    if (avoidObstacles.distanceLimit > math::Epsilon)
        rayLength = std::min(avoidObstacles.distanceLimit, rayLength);

    const QueryFilter filter = filterFor(*this, blockingLayers(*this));
    const auto result = physics.sphereCast(
        {targetPoint + direction * minimumDistance, direction},
        avoidObstacles.cameraRadius,
        rayLength,
        filter);
    if (result.has_value())
    {
        hit = *result;
        newPosition =
            hit.point + hit.normal * (avoidObstacles.cameraRadius + kPrecisionSlush);
    }

    if ((targetPoint - newPosition).sqrMagnitude() < minimumDistance * minimumDistance)
        newPosition = targetPoint + direction * minimumDistance;

    return result.has_value();
}

math::Vec3 Deoccluder::pushCameraBack(
    const IPhysicsWorld& physics,
    math::Vec3 currentPosition,
    math::Vec3 pushDirection,
    const SphereCastHit& obstacle,
    math::Vec3 targetPoint,
    math::Plane startPlane,
    float targetDistance,
    int iterations) const
{
    math::Vec3 position = currentPosition;
    math::Vec3 direction{};
    if (!getWalkingDirection(
            physics,
            position,
            pushDirection,
            obstacle,
            direction))
    {
        return position;
    }

    Ray ray{position, direction};
    float distance = getPushBackDistance(
        ray,
        startPlane,
        targetDistance,
        targetPoint);
    if (distance <= math::Epsilon)
        return position;

    const auto bounds = physics.bounds(obstacle.entity);
    if (bounds.has_value())
    {
        const float clampedDistance =
            clampRayToBounds(ray, distance, *bounds);
        distance = std::min(distance, clampedDistance + kPrecisionSlush);
    }

    const QueryFilter filter = filterFor(*this, blockingLayers(*this));
    const auto hit = physics.sphereCast(
        ray,
        avoidObstacles.cameraRadius,
        distance,
        filter);
    if (hit.has_value())
    {
        const float adjustment =
            std::max(0.0f, hit->distance - kPrecisionSlush);
        position = ray.pointAt(adjustment);
        if (iterations > 1)
        {
            position = pushCameraBack(
                physics,
                position,
                direction,
                *hit,
                targetPoint,
                startPlane,
                targetDistance,
                iterations - 1);
        }
        return position;
    }

    position = ray.pointAt(distance);
    direction = position - targetPoint;
    const float targetDistanceNow = direction.length();
    if (targetDistanceNow < math::Epsilon)
        return currentPosition;

    direction /= targetDistanceNow;
    const auto visionHit = physics.sphereCast(
        {targetPoint, direction},
        avoidObstacles.cameraRadius,
        std::max(0.0f, targetDistanceNow - kPrecisionSlush),
        filter);
    if (visionHit.has_value())
        return currentPosition;

    ray = {position, direction};
    distance = getPushBackDistance(
        ray,
        startPlane,
        targetDistance,
        targetPoint);
    if (distance > math::Epsilon)
    {
        const auto finalHit = physics.sphereCast(
            ray,
            avoidObstacles.cameraRadius,
            distance,
            filter);
        if (finalHit.has_value())
        {
            const float adjustment =
                std::max(0.0f, finalHit->distance - kPrecisionSlush);
            position = ray.pointAt(adjustment);
            if (iterations > 1)
            {
                position = pushCameraBack(
                    physics,
                    position,
                    direction,
                    *finalHit,
                    targetPoint,
                    startPlane,
                    targetDistance,
                    iterations - 1);
            }
        }
        else
        {
            position = ray.pointAt(distance);
        }
    }

    return position;
}

bool Deoccluder::getWalkingDirection(
    const IPhysicsWorld& physics,
    math::Vec3 position,
    math::Vec3 pushDirection,
    const SphereCastHit& obstacle,
    math::Vec3& direction) const
{
    math::Vec3 secondNormal = obstacle.normal;
    const QueryFilter filter = filterFor(*this, blockingLayers(*this));
    const auto nearby = physics.overlapSphere(
        position,
        kPrecisionSlush * 5.0f,
        filter);
    for (const OverlapHit& hit : nearby)
    {
        if (hit.entity == obstacle.entity)
            continue;
        const auto bounds = physics.bounds(hit.entity);
        if (!bounds.has_value())
            continue;
        const math::Vec3 closest = math::clamp(
            position,
            bounds->min(),
            bounds->max());
        const math::Vec3 normal = position - closest;
        if (normal.sqrMagnitude() > math::Epsilon * math::Epsilon
            && !math::approximately(normal.normalized(), obstacle.normal))
        {
            secondNormal = normal.normalized();
            break;
        }
    }

    direction = math::cross(obstacle.normal, secondNormal);
    if (math::almostZero(direction))
    {
        direction = math::projectOnPlane(pushDirection, obstacle.normal);
    }
    else
    {
        const float dot = math::dot(direction, pushDirection);
        if (std::abs(dot) < math::Epsilon)
            return false;
        if (dot < 0.0f)
            direction = -direction;
    }

    if (math::almostZero(direction))
        return false;

    direction = math::normalize(direction);
    return true;
}

float Deoccluder::getPushBackDistance(
    const Ray& ray,
    math::Plane startPlane,
    float targetDistance,
    math::Vec3 targetPoint) const
{
    float maxDistance = targetDistance - (ray.origin - targetPoint).length();
    if (maxDistance < math::Epsilon)
        return 0.0f;
    if (avoidObstacles.strategy == ResolutionStrategy::PreserveCameraDistance)
        return maxDistance;

    float distance = 0.0f;
    if (!startPlane.raycast(ray, distance))
        distance = 0.0f;
    distance = std::min(maxDistance, distance);
    if (distance < math::Epsilon)
        return 0.0f;

    const float angle = std::abs(
        math::angle(startPlane.normal, ray.direction) - math::HalfPi);
    distance = math::lerp(0.0f, distance, angle / kAngleThreshold);
    return distance;
}

math::Vec3 Deoccluder::respectCameraRadius(
    const IPhysicsWorld& physics,
    math::Vec3 cameraPosition,
    math::Vec3 targetPoint) const
{
    math::Vec3 result = math::Vec3::zero;
    if (avoidObstacles.cameraRadius < math::Epsilon || collideAgainst.bits == 0u)
        return result;

    math::Vec3 direction = cameraPosition - targetPoint;
    const float distance = direction.length();
    if (distance > math::Epsilon)
        direction /= distance;

    const QueryFilter fullFilter = filterFor(*this, collideAgainst);
    const auto obstacles = physics.overlapSphere(
        cameraPosition,
        avoidObstacles.cameraRadius,
        fullFilter);
    const bool shouldResolve =
        (!obstacles.empty() && distance == 0.0f)
        || distance > minimumDistanceFromTarget;
    if (!shouldResolve)
        return result;

    math::Vec3 newCameraPosition = cameraPosition;
    for (const OverlapHit& obstacle : obstacles)
    {
        if (distance > minimumDistanceFromTarget)
        {
            math::Vec3 offset = newCameraPosition - targetPoint;
            const float offsetDistance = offset.length();
            if (offsetDistance > math::Epsilon)
            {
                offset /= offsetDistance;
                const auto hit = physics.raycast(
                    {targetPoint, offset},
                    offsetDistance + avoidObstacles.cameraRadius,
                    fullFilter);
                if (hit.has_value() && hit->entity == obstacle.entity)
                {
                    newCameraPosition = targetPoint
                        + offset * std::max(0.0f, hit->distance - kPrecisionSlush);
                }
            }
        }

        math::Vec3 pushDirection = newCameraPosition - obstacle.point;
        if (isZero(pushDirection))
        {
            const auto bounds = physics.bounds(obstacle.entity);
            if (bounds.has_value())
                pushDirection = newCameraPosition - bounds->center;
        }
        if (isZero(pushDirection))
            pushDirection = direction;
        if (isZero(pushDirection))
            continue;

        const float pushDistance =
            avoidObstacles.cameraRadius - obstacle.distance + kPrecisionSlush;
        if (pushDistance > 0.0f)
            newCameraPosition += math::normalize(pushDirection) * pushDistance;
    }
    result = newCameraPosition - cameraPosition;

    if (distance > math::Epsilon && minimumDistanceFromTarget > math::Epsilon)
    {
        const float minimumDistance = std::max(
            minimumDistanceFromTarget,
            avoidObstacles.cameraRadius) + kPrecisionSlush;
        const math::Vec3 newOffset = cameraPosition + result - targetPoint;
        if (newOffset.length() < minimumDistance)
            result = targetPoint - cameraPosition + direction * minimumDistance;
    }

    return result;
}

bool Deoccluder::isTargetObscured(
    const CameraState& state,
    const IPhysicsWorld& physics) const
{
    if (!state.hasLookAt())
        return false;

    math::Vec3 direction = state.referenceLookAt - state.correctedPosition();
    const float distance = direction.length();
    if (distance < std::max(minimumDistanceFromTarget, math::Epsilon))
        return true;
    direction /= distance;

    const auto hit = physics.sphereCast(
        {state.correctedPosition(), direction},
        avoidObstacles.cameraRadius,
        distance - minimumDistanceFromTarget,
        filterFor(*this, blockingLayers(*this)));
    return hit.has_value();
}

} // namespace cinerig




