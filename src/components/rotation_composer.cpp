#include <cinerig/components/rotation_composer.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig
{

namespace
{

bool sameRect(const math::Rect& a, const math::Rect& b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

float clampVerticalFov(float degrees)
{
    return math::clamp(degrees, 0.01f, 179.0f);
}

} // namespace

void RotationComposer::FovCache::update(
    const LensSettings& lens,
    const math::Rect& softGuide,
    const math::Rect& hardGuide,
    float targetDistance)
{
    const float lensAspect = lens.aspect > math::Epsilon ? lens.aspect : 1.0f;
    bool recalculate = orthographic != lens.orthographic
        || !math::approximately(aspect, lensAspect)
        || !sameRect(deadZoneRect, softGuide)
        || !sameRect(hardLimitsRect, hardGuide)
        || (screenBounds.x == 0.0f && screenBounds.y == 0.0f);

    if (lens.orthographic)
    {
        const float orthoOverDistance =
            std::abs(lens.orthographicSize / std::max(targetDistance, math::Epsilon));
        const float previousOrthoOverDistance = orthographicSizeOverDistance;
        if (previousOrthoOverDistance == 0.0f
            || (previousOrthoOverDistance > math::Epsilon
                && std::abs(orthoOverDistance - previousOrthoOverDistance)
                        / previousOrthoOverDistance
                    > previousOrthoOverDistance * 0.01f))
        {
            recalculate = true;
        }

        if (recalculate)
        {
            halfFovRad = {
                std::atan(lensAspect * orthoOverDistance),
                std::atan(orthoOverDistance),
            };
            fov = {halfFovRad.x * 2.0f, halfFovRad.y * 2.0f};
            orthographicSizeOverDistance = orthoOverDistance;
        }
    }
    else
    {
        const float halfVerticalFov =
            clampVerticalFov(lens.fieldOfView) * math::DegToRad * 0.5f;
        if (!math::approximately(halfFovRad.y, halfVerticalFov)
            || !math::approximately(halfFovRad.x, std::atan(std::tan(halfVerticalFov) * lensAspect)))
        {
            recalculate = true;
        }

        if (recalculate)
        {
            halfFovRad = {
                std::atan(std::tan(halfVerticalFov) * lensAspect),
                halfVerticalFov,
            };
            fov = {halfFovRad.x * 2.0f, halfFovRad.y * 2.0f};
            orthographicSizeOverDistance = 0.0f;
        }
    }

    if (!recalculate)
        return;

    aspect = lensAspect;
    orthographic = lens.orthographic;
    screenBounds = {std::tan(halfFovRad.x), std::tan(halfFovRad.y)};
    deadZoneRect = softGuide;
    hardLimitsRect = hardGuide;

    const math::Vec2 softMin = screenToAngle({softGuide.xMin(), softGuide.yMin()});
    const math::Vec2 softMax = screenToAngle({softGuide.xMax(), softGuide.yMax()});
    fovSoftGuideRect = {
        softMin.x,
        softMin.y,
        softMax.x - softMin.x,
        softMax.y - softMin.y,
    };

    const math::Vec2 hardMin = screenToAngle({hardGuide.xMin(), hardGuide.yMin()});
    const math::Vec2 hardMax = screenToAngle({hardGuide.xMax(), hardGuide.yMax()});
    fovHardGuideRect = {
        hardMin.x,
        hardMin.y,
        hardMax.x - hardMin.x,
        hardMax.y - hardMin.y,
    };
}

math::Vec2 RotationComposer::FovCache::screenToAngle(math::Vec2 point) const
{
    return {
        (halfFovRad.x + std::atan(2.0f * (point.x - 0.5f) * screenBounds.x))
            / (2.0f * halfFovRad.x),
        (halfFovRad.y + std::atan(2.0f * (point.y - 0.5f) * screenBounds.y))
            / (2.0f * halfFovRad.y),
    };
}

math::Vec3 RotationComposer::FovCache::directionFromScreen(math::Vec2 point) const
{
    return {
        2.0f * (point.x - 0.5f) * screenBounds.x,
        2.0f * (point.y - 0.5f) * screenBounds.y,
        -1.0f,
    };
}

bool RotationComposer::isValid() const
{
    return isAttached() && lookAtTargetPosition().has_value();
}

void RotationComposer::prePipelineMutate(CameraState& state, float deltaTime)
{
    if (!isValid() || !state.hasLookAt())
    {
        trackedPointValid = false;
        return;
    }

    state.referenceLookAt = getLookAtPointAndSetTrackedPoint(
        state.referenceLookAt,
        state.referenceUp,
        deltaTime);
    trackedPointValid = true;
}

math::Vec3 RotationComposer::getLookAtPointAndSetTrackedPoint(
    math::Vec3 lookAt,
    math::Vec3 up,
    float deltaTime)
{
    math::Vec3 position = lookAt;
    if (const auto rotation = lookAtTargetRotation())
        position += math::rotate(*rotation, targetOffset);

    const TargetRef target = virtualCamera().effectiveLookAtTarget();
    const bool targetChanged = !hasPreviousLookAtTarget || target != previousLookAtTarget;
    previousLookAtTarget = target;
    hasPreviousLookAtTarget = true;

    if (!lookahead.enabled || lookahead.time < math::Epsilon)
    {
        predictor.reset();
        trackedPoint_ = position;
        return trackedPoint_;
    }

    const bool resetLookahead =
        targetChanged || !virtualCamera().previousStateIsValid;
    predictor.smoothing = lookahead.smoothing;
    predictor.addPosition(position, resetLookahead ? -1.0f : deltaTime);

    math::Vec3 delta = predictor.predictDelta(lookahead.time);
    if (lookahead.ignoreY)
        delta = math::projectOnPlane(delta, up);
    trackedPoint_ = position + delta;
    return trackedPoint_;
}

void RotationComposer::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isValid() || !state.hasLookAt())
        return;

    if (!trackedPointValid)
    {
        state.referenceLookAt = getLookAtPointAndSetTrackedPoint(
            state.referenceLookAt,
            state.referenceUp,
            deltaTime);
        trackedPointValid = true;
    }

    if (!math::almostZero(trackedPoint_ - state.referenceLookAt))
    {
        const math::Vec3 mid = math::lerp(
            state.correctedPosition(),
            state.referenceLookAt,
            0.5f);
        const math::Vec3 toLookAt = state.referenceLookAt - mid;
        const math::Vec3 toTracked = trackedPoint_ - mid;
        if (math::dot(toLookAt, toTracked) < 0.0f)
        {
            const float lookAtDistance = math::length(state.referenceLookAt - mid);
            const float trackedDistance = math::length(state.referenceLookAt - trackedPoint_);
            const float t = trackedDistance > math::Epsilon
                ? lookAtDistance / trackedDistance
                : 0.0f;
            trackedPoint_ = math::lerp(state.referenceLookAt, trackedPoint_, t);
        }
    }

    const float targetDistance = math::length(trackedPoint_ - state.correctedPosition());
    if (targetDistance < math::Epsilon)
    {
        if (deltaTime >= 0.0f && virtualCamera().previousStateIsValid && historyValid)
            state.rawOrientation = cameraOrientationPrevFrame;
        return;
    }

    cache.update(
        state.lens,
        composition.deadZoneRect(),
        composition.hardLimitsRect(),
        targetDistance);

    const bool canDamp = deltaTime >= 0.0f
        && virtualCamera().previousStateIsValid
        && historyValid;

    math::Quat rigOrientation = state.rawOrientation;
    if (!canDamp)
    {
        rigOrientation = math::lookRotation(
            rigOrientation * math::Vec3::forward,
            state.referenceUp);
        math::Rect rect = cache.fovSoftGuideRect;
        if (centerOnActivate)
        {
            const math::Vec2 center = rect.center();
            rect = {center.x, center.y, 0.0f, 0.0f};
        }
        rotateToScreenBounds(
            state,
            rect,
            state.referenceLookAt,
            rigOrientation,
            cache.fov,
            -1.0f);
    }
    else
    {
        const math::Vec3 direction = lookAtPrevFrame - cameraPositionPrevFrame;
        if (math::almostZero(direction))
        {
            rigOrientation = math::lookRotation(
                cameraOrientationPrevFrame * math::Vec3::forward,
                state.referenceUp);
        }
        else
        {
            const math::Vec3 bypassedDirection =
                state.rotationDampingBypass * direction;

            if (!math::approximately(composition.screenPosition, compositionLastFrame.screenPosition))
            {
                const math::Vec2 previousScreenPoint =
                    compositionLastFrame.screenPosition + math::Vec2{0.5f, 0.5f};
                const math::Vec2 currentScreenPoint =
                    composition.screenPosition + math::Vec2{0.5f, 0.5f};
                const math::Vec3 previousDirection =
                    cache.directionFromScreen(previousScreenPoint);
                const math::Vec3 currentDirection =
                    cache.directionFromScreen(currentScreenPoint);

                math::Quat screenRotation = math::applyCameraRotation(
                    math::Quat::identity(),
                    screenOffsetPrevFrame,
                    state.referenceUp);
                screenRotation = screenRotation * math::safeFromToRotation(
                    previousDirection,
                    currentDirection,
                    state.referenceUp);
                screenOffsetPrevFrame = math::getCameraRotationToTarget(
                    math::Quat::identity(),
                    screenRotation * math::Vec3::forward,
                    state.referenceUp);
            }

            rigOrientation = math::lookRotation(bypassedDirection, state.referenceUp);
            rigOrientation = math::applyCameraRotation(
                rigOrientation,
                -screenOffsetPrevFrame,
                state.referenceUp);
        }

        rotateToScreenBounds(
            state,
            cache.fovSoftGuideRect,
            trackedPoint_,
            rigOrientation,
            cache.fov,
            deltaTime);

        if (composition.hardLimits.enabled
            && virtualCamera().lookAtTargetAttachment > 1.0f - math::Epsilon)
        {
            rotateToScreenBounds(
                state,
                cache.fovHardGuideRect,
                state.referenceLookAt,
                rigOrientation,
                cache.fov,
                -1.0f);
        }
    }

    cameraPositionPrevFrame = state.correctedPosition();
    lookAtPrevFrame = trackedPoint_;
    cameraOrientationPrevFrame = math::normalize(rigOrientation);
    screenOffsetPrevFrame = math::getCameraRotationToTarget(
        cameraOrientationPrevFrame,
        lookAtPrevFrame - cameraPositionPrevFrame,
        state.referenceUp);
    compositionLastFrame = composition;
    historyValid = true;

    state.rawOrientation = cameraOrientationPrevFrame;
}

void RotationComposer::rotateToScreenBounds(
    CameraState& state,
    math::Rect screenRect,
    math::Vec3 trackedPoint,
    math::Quat& rigOrientation,
    math::Vec2 fov,
    float deltaTime) const
{
    const math::Vec3 targetDirection = trackedPoint - state.correctedPosition();
    math::Vec2 rotationToRect = math::getCameraRotationToTarget(
        rigOrientation,
        targetDirection,
        state.referenceUp);

    clampVerticalBounds(screenRect, targetDirection, state.referenceUp, fov.y);

    const float minVertical = (screenRect.yMin() - 0.5f) * fov.y;
    const float maxVertical = (screenRect.yMax() - 0.5f) * fov.y;
    if (rotationToRect.x < minVertical)
        rotationToRect.x -= minVertical;
    else if (rotationToRect.x > maxVertical)
        rotationToRect.x -= maxVertical;
    else
        rotationToRect.x = 0.0f;

    const float minScreenHorizontal = (screenRect.xMin() - 0.5f) * fov.x;
    const float maxScreenHorizontal = (screenRect.xMax() - 0.5f) * fov.x;
    const float minHorizontal = -maxScreenHorizontal;
    const float maxHorizontal = -minScreenHorizontal;
    if (rotationToRect.y < minHorizontal)
        rotationToRect.y -= minHorizontal;
    else if (rotationToRect.y > maxHorizontal)
        rotationToRect.y -= maxHorizontal;
    else
        rotationToRect.y = 0.0f;

    if (deltaTime >= 0.0f && virtualCamera().previousStateIsValid && historyValid)
    {
        rotationToRect.x = detachedLookAtDamp(rotationToRect.x, damping.y, deltaTime);
        rotationToRect.y = detachedLookAtDamp(rotationToRect.y, damping.x, deltaTime);
    }

    rigOrientation = math::applyCameraRotation(
        rigOrientation,
        rotationToRect,
        state.referenceUp);
}

bool RotationComposer::clampVerticalBounds(
    math::Rect& rect,
    math::Vec3 direction,
    math::Vec3 up,
    float verticalFovRadians)
{
    const float angleToUp = math::angle(direction, up);
    const float halfFov = verticalFovRadians * 0.5f + 1.0f * math::DegToRad;

    if (angleToUp < halfFov)
    {
        const float maxY = 1.0f - (halfFov - angleToUp) / verticalFovRadians;
        if (rect.yMax() > maxY)
        {
            rect.y = std::min(rect.yMin(), maxY);
            rect.height = std::min(rect.yMax(), maxY) - rect.y;
            return true;
        }
    }

    if (angleToUp > math::Pi - halfFov)
    {
        const float minY = (angleToUp - (math::Pi - halfFov)) / verticalFovRadians;
        if (minY > rect.yMin())
        {
            rect.y = std::max(rect.yMin(), minY);
            rect.height = std::max(rect.yMax(), minY) - rect.y;
            return true;
        }
    }

    return false;
}

float RotationComposer::detachedLookAtDamp(
    float initial,
    float dampTime,
    float deltaTime) const
{
    const float attachment = math::clamp01(virtualCamera().lookAtTargetAttachment);
    dampTime = math::lerp(std::max(1.0f, dampTime), dampTime, attachment);
    deltaTime = math::lerp(0.0f, deltaTime, attachment);
    return math::standardDamp(initial, dampTime, deltaTime);
}

void RotationComposer::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    if (!virtualCamera().isEntityTarget(virtualCamera().effectiveLookAtTarget(), target))
        return;

    cameraPositionPrevFrame += positionDelta;
    lookAtPrevFrame += positionDelta;
    predictor.applyTransformDelta(positionDelta);
}

void RotationComposer::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    CameraComponent::forceCameraPosition(pos, rot);
    predictor.applyRotationDelta(rot * math::inverse(cameraOrientationPrevFrame));
    cameraPositionPrevFrame = pos;
    cameraOrientationPrevFrame = rot;
}

float RotationComposer::maxDampTime() const
{
    return std::max(std::max(0.0f, damping.x), std::max(0.0f, damping.y));
}

} // namespace cinerig
