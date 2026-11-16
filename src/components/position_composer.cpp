#include <cinerig/components/position_composer.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig
{

namespace
{

math::Vec3 nonNegative(math::Vec3 value)
{
    return {
        std::max(0.0f, value.x),
        std::max(0.0f, value.y),
        std::max(0.0f, value.z),
    };
}

float clampedFieldOfView(const LensSettings& lens)
{
    return math::clamp(lens.fieldOfView, 0.01f, 179.0f);
}

} // namespace

bool PositionComposer::isValid() const
{
    return isAttached() && followTargetPosition().has_value();
}

math::Rect PositionComposer::screenToOrtho(
    const math::Rect& screenRect,
    float orthoSize,
    float aspect)
{
    const float xScale = 2.0f * orthoSize * aspect;
    const float yScale = 2.0f * orthoSize;
    return {
        xScale * (screenRect.xMin() - 0.5f),
        yScale * (screenRect.yMin() - 0.5f),
        xScale * screenRect.width,
        yScale * screenRect.height,
    };
}

math::Vec3 PositionComposer::orthoOffsetToScreenBounds(
    math::Vec3 targetPosition,
    const math::Rect& screenRect)
{
    math::Vec3 delta{};
    if (targetPosition.x < screenRect.xMin())
        delta.x += targetPosition.x - screenRect.xMin();
    if (targetPosition.x > screenRect.xMax())
        delta.x += targetPosition.x - screenRect.xMax();
    if (targetPosition.y < screenRect.yMin())
        delta.y += targetPosition.y - screenRect.yMin();
    if (targetPosition.y > screenRect.yMax())
        delta.y += targetPosition.y - screenRect.yMax();
    return delta;
}

math::Vec3 PositionComposer::detachedFollowTargetDamp(
    math::Vec3 initial,
    math::Vec3 dampTime,
    float deltaTime) const
{
    const float attachment = math::clamp01(virtualCamera().followTargetAttachment);
    const math::Vec3 effectiveDampTime = math::lerp(
        math::max(math::Vec3::one, dampTime),
        dampTime,
        attachment);
    const float effectiveDeltaTime = math::lerp(0.0f, deltaTime, attachment);
    return math::standardDamp(initial, effectiveDampTime, effectiveDeltaTime);
}

void PositionComposer::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached())
        return;

    const TargetRef followTarget = virtualCamera().effectiveFollowTarget();
    const bool targetChanged =
        !hasPreviousFollowTarget || followTarget != previousFollowTarget;
    previousFollowTarget = followTarget;
    hasPreviousFollowTarget = true;

    const bool previousStateIsValid =
        deltaTime >= 0.0f
        && virtualCamera().previousStateIsValid
        && historyValid;

    if (!previousStateIsValid || targetChanged)
        predictor.reset();

    const auto targetPositionOpt = followTargetPosition();
    if (!targetPositionOpt.has_value())
    {
        inheritingPosition = false;
        return;
    }

    const math::Vec3 targetPosition = *targetPositionOpt;
    const auto targetRotation = followTargetRotation();
    const math::Vec3 trackingPoint = targetPosition
        + (targetRotation.has_value()
               ? math::rotate(*targetRotation, targetOffset)
               : targetOffset);

    const float desiredDistance = std::max(kMinimumCameraDistance, cameraDistance);
    const float desiredDeadZoneDepth = std::max(0.0f, deadZoneDepth);
    const math::Vec3 dampingValue = nonNegative(damping);

    if (!previousStateIsValid)
    {
        previousCameraPosition = state.rawPosition;
        previousRotation = state.rawOrientation;
        previousDesiredDistance = desiredDistance;
        previousComposition = composition;
        if (!inheritingPosition && centerOnActivate)
        {
            previousCameraPosition =
                targetPosition
                + math::rotate(state.rawOrientation, math::Vec3::back)
                    * desiredDistance;
        }
    }

    if (!state.hasLookAt() || math::approximately(state.referenceLookAt, targetPosition))
        state.referenceLookAt = targetPosition;

    if (!lookahead.enabled || lookahead.time < math::Epsilon)
    {
        predictor.reset();
    }
    else
    {
        predictor.smoothing = lookahead.smoothing;
        predictor.addPosition(trackingPoint, previousStateIsValid ? deltaTime : -1.0f);
    }

    math::Vec3 trackedPoint = trackingPoint;
    if (lookahead.enabled && lookahead.time >= math::Epsilon)
    {
        math::Vec3 delta = predictor.predictDelta(lookahead.time);
        if (lookahead.ignoreY)
            delta = math::projectOnPlane(delta, state.referenceUp);
        trackedPoint += delta;
    }

    const math::Quat localToWorld = state.rawOrientation;
    if (previousStateIsValid)
    {
        const math::Quat orientationDelta =
            localToWorld * math::inverse(previousRotation);
        const math::Vec3 direction =
            math::rotate(orientationDelta, previousCameraPosition - trackedPoint);
        previousCameraPosition = trackedPoint + direction;

        const float distanceChange = desiredDistance - previousDesiredDistance;
        if (std::abs(distanceChange) > math::Epsilon)
            previousCameraPosition += direction.normalized() * distanceChange;
    }
    previousRotation = localToWorld;

    const math::Quat worldToLocal = math::inverse(localToWorld);
    math::Vec3 cameraPositionWorld = previousCameraPosition;
    math::Vec3 cameraPosition =
        math::rotate(worldToLocal, cameraPositionWorld);
    math::Vec3 targetPositionLocal =
        math::rotate(worldToLocal, trackedPoint) - cameraPosition;

    math::Vec3 cameraOffset{};
    const float targetDepth = -targetPositionLocal.z;
    const float cameraMin = std::max(
        kMinimumCameraDistance,
        desiredDistance - desiredDeadZoneDepth * 0.5f);
    const float cameraMax = std::max(
        cameraMin,
        desiredDistance + desiredDeadZoneDepth * 0.5f);
    if (targetDepth < cameraMin)
        cameraOffset.z = cameraMin - targetDepth;
    else if (targetDepth > cameraMax)
        cameraOffset.z = cameraMax - targetDepth;

    const float screenDistance = targetDepth + cameraOffset.z;
    const float aspect = state.lens.aspect > math::Epsilon ? state.lens.aspect : 1.0f;
    const float screenSize = state.lens.orthographic
        ? state.lens.orthographicSize
        : std::tan(clampedFieldOfView(state.lens) * math::DegToRad * 0.5f)
            * std::max(screenDistance, math::Epsilon);

    const math::Rect softGuide = screenToOrtho(
        composition.deadZoneRect(),
        screenSize,
        aspect);

    if (!previousStateIsValid)
    {
        math::Rect rect = softGuide;
        if (centerOnActivate && !inheritingPosition)
        {
            const math::Vec2 center = rect.center();
            rect = {center.x, center.y, 0.0f, 0.0f};
        }
        cameraOffset += orthoOffsetToScreenBounds(targetPositionLocal, rect);
    }
    else
    {
        if (!math::approximately(
                composition.screenPosition,
                previousComposition.screenPosition))
        {
            const math::Vec2 screenDelta =
                composition.screenPosition - previousComposition.screenPosition;
            const math::Vec3 localMove{
                -screenDelta.x * screenSize * aspect * 2.0f,
                -screenDelta.y * screenSize * 2.0f,
                0.0f,
            };
            cameraPositionWorld += math::rotate(localToWorld, localMove);
            targetPositionLocal -= localMove;
            cameraPosition += localMove;
        }

        cameraOffset += orthoOffsetToScreenBounds(targetPositionLocal, softGuide);
        cameraOffset = detachedFollowTargetDamp(
            cameraOffset,
            dampingValue,
            deltaTime);

        if (composition.hardLimits.enabled
            && virtualCamera().followTargetAttachment > 1.0f - math::Epsilon)
        {
            const math::Rect hardGuide = screenToOrtho(
                composition.hardLimitsRect(),
                screenSize,
                aspect);
            const math::Vec3 realTargetPosition =
                math::rotate(worldToLocal, trackingPoint) - cameraPosition;
            cameraOffset += orthoOffsetToScreenBounds(
                realTargetPosition - cameraOffset,
                hardGuide);
        }
    }

    state.rawPosition =
        cameraPositionWorld + math::rotate(localToWorld, cameraOffset);
    previousCameraPosition = state.rawPosition;
    previousRotation = localToWorld;
    previousDesiredDistance = desiredDistance;
    previousComposition = composition;
    historyValid = true;
    inheritingPosition = false;
}

bool PositionComposer::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    (void)worldUp;
    (void)deltaTime;

    if (from == nullptr || virtualCamera().isLive())
        return false;

    const bool inheritsPosition =
        hasBlendHint(virtualCamera().blendHint, BlendHint::InheritPosition)
        || hasBlendHint(virtualCamera().state().blendHint, BlendHint::InheritPosition);
    if (!inheritsPosition)
        return false;

    const CameraState& fromState = from->state();
    previousCameraPosition = fromState.finalPosition();
    previousRotation = fromState.finalOrientation();
    previousDesiredDistance = std::max(kMinimumCameraDistance, cameraDistance);
    previousComposition = composition;
    historyValid = true;
    inheritingPosition = true;
    return true;
}

void PositionComposer::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    if (!isAttached())
        return;
    if (!virtualCamera().isEntityTarget(virtualCamera().effectiveFollowTarget(), target))
        return;

    previousCameraPosition += positionDelta;
    predictor.applyTransformDelta(positionDelta);
}

void PositionComposer::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    CameraComponent::forceCameraPosition(pos, rot);
    predictor.applyRotationDelta(rot * math::inverse(previousRotation));
    previousCameraPosition = pos;
    previousRotation = rot;
    previousDesiredDistance = std::max(kMinimumCameraDistance, cameraDistance);
    previousComposition = composition;
    historyValid = true;
}

float PositionComposer::maxDampTime() const
{
    return std::max(
        std::max(0.0f, damping.x),
        std::max(
            std::max(0.0f, damping.y),
            std::max(0.0f, damping.z)));
}

} // namespace cinerig
