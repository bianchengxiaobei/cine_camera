#include <cinerig/core/tracker.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/math/damp.hpp>
#include <cinerig/math/geometry.hpp>

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

float detachedDamp(
    float initial,
    float dampTime,
    float deltaTime,
    float followTargetAttachment)
{
    const float attachment = math::clamp01(followTargetAttachment);
    dampTime = math::lerp(std::max(1.0f, dampTime), dampTime, attachment);
    deltaTime = math::lerp(0.0f, deltaTime, attachment);
    return math::standardDamp(initial, dampTime, deltaTime);
}

math::Vec3 detachedDamp(
    math::Vec3 initial,
    math::Vec3 dampTime,
    float deltaTime,
    float followTargetAttachment)
{
    const float attachment = math::clamp01(followTargetAttachment);
    const math::Vec3 effectiveDampTime = math::lerp(
        math::max(math::Vec3::one, dampTime),
        dampTime,
        attachment);
    const float effectiveDeltaTime = math::lerp(0.0f, deltaTime, attachment);
    return math::standardDamp(initial, effectiveDampTime, effectiveDeltaTime);
}

} // namespace

void TrackerSettings::validate()
{
    positionDamping = nonNegative(positionDamping);
    rotationDamping = nonNegative(rotationDamping);
    quaternionDamping = std::max(0.0f, quaternionDamping);
}

math::Vec3 TrackerSettings::effectivePositionDamping() const
{
    math::Vec3 result = nonNegative(positionDamping);
    if (bindingMode == BindingMode::LazyFollow)
        result.x = 0.0f;
    return result;
}

math::Vec3 TrackerSettings::effectiveRotationDamping() const
{
    const math::Vec3 damping = nonNegative(rotationDamping);
    switch (bindingMode)
    {
    case BindingMode::LockToTargetNoRoll:
        return {damping.x, damping.y, 0.0f};
    case BindingMode::LockToTargetWithWorldUp:
        return {0.0f, damping.y, 0.0f};
    case BindingMode::WorldSpace:
    case BindingMode::LazyFollow:
        return math::Vec3::zero;
    case BindingMode::LockToTargetOnAssign:
    case BindingMode::LockToTarget:
    default:
        return damping;
    }
}

float TrackerSettings::maxDampTime() const
{
    const math::Vec3 position = effectivePositionDamping();
    const math::Vec3 rotation = angularDampingMode == AngularDampingMode::Euler
        ? effectiveRotationDamping()
        : math::Vec3{std::max(0.0f, quaternionDamping), 0.0f, 0.0f};
    return std::max(
        std::max(position.x, std::max(position.y, position.z)),
        std::max(rotation.x, std::max(rotation.y, rotation.z)));
}

void Tracker::initStateInfo(
    const TrackerTarget& target,
    float deltaTime,
    bool previousStateIsValid,
    const TrackerSettings& settings,
    math::Vec3 targetOffset,
    math::Vec3 worldUp,
    const CameraState& cameraState)
{
    const bool previousValid = deltaTime >= 0.0f && previousStateIsValid;
    if (previousTarget_ != target.ref || !previousValid)
    {
        previousTarget_ = target.ref;
        targetOrientationOnAssign_ = target.valid
            ? math::normalize(target.rotation)
            : math::Quat::identity();
    }

    if (!previousValid)
    {
        previousTargetPosition_ = target.valid ? target.position : math::Vec3::zero;
        previousReferenceOrientation_ = getReferenceOrientation(
            target,
            settings.bindingMode,
            targetOffset,
            worldUp,
            cameraState);
        hasPreviousReferenceOrientation_ = true;
    }
}

math::Quat Tracker::getReferenceOrientation(
    const TrackerTarget& target,
    BindingMode bindingMode,
    math::Vec3 targetOffset,
    math::Vec3 worldUp,
    const CameraState& cameraState) const
{
    if (bindingMode == BindingMode::WorldSpace)
        return math::Quat::identity();

    if (target.valid)
    {
        const math::Vec3 targetForward = math::rotate(target.rotation, math::Vec3::forward);
        switch (bindingMode)
        {
        case BindingMode::LockToTargetOnAssign:
            return targetOrientationOnAssign_;
        case BindingMode::LockToTargetWithWorldUp:
        {
            const math::Vec3 forward = math::projectOnPlane(targetForward, worldUp);
            if (!math::almostZero(forward))
                return math::lookRotation(forward, worldUp);
            break;
        }
        case BindingMode::LockToTargetNoRoll:
            return math::lookRotation(targetForward, worldUp);
        case BindingMode::LockToTarget:
            return target.rotation;
        case BindingMode::LazyFollow:
        {
            const math::Vec3 position =
                target.position + math::rotate(target.rotation, targetOffset);
            const math::Vec3 forward =
                math::projectOnPlane(position - cameraState.rawPosition, worldUp);
            if (!math::almostZero(forward))
                return math::lookRotation(forward, worldUp);
            break;
        }
        case BindingMode::WorldSpace:
            return math::Quat::identity();
        }
    }

    if (hasPreviousReferenceOrientation_)
        return math::normalize(previousReferenceOrientation_);
    return math::Quat::identity();
}

void Tracker::trackTarget(
    const TrackerTarget& target,
    float deltaTime,
    bool previousStateIsValid,
    math::Vec3 worldUp,
    math::Vec3 desiredCameraOffset,
    const TrackerSettings& settings,
    math::Vec3 targetOffset,
    CameraState& cameraState,
    float followTargetAttachment,
    math::Vec3& outTargetPosition,
    math::Quat& outTargetOrient)
{
    if (!target.valid)
    {
        outTargetPosition = previousTargetPosition_;
        outTargetOrient = hasPreviousReferenceOrientation_
            ? math::normalize(previousReferenceOrientation_)
            : math::Quat::identity();
        return;
    }

    const bool previousValid = deltaTime >= 0.0f && previousStateIsValid;
    const math::Quat referenceOrientation = getReferenceOrientation(
        target,
        settings.bindingMode,
        targetOffset,
        worldUp,
        cameraState);
    math::Quat dampedOrientation = referenceOrientation;

    if (previousValid
        && settings.bindingMode != BindingMode::LazyFollow
        && settings.bindingMode != BindingMode::WorldSpace)
    {
        if (settings.angularDampingMode == AngularDampingMode::Quaternion
            && settings.bindingMode == BindingMode::LockToTarget)
        {
            const float weight = detachedDamp(
                1.0f,
                settings.quaternionDamping,
                deltaTime,
                followTargetAttachment);
            dampedOrientation = math::slerp(
                previousReferenceOrientation_,
                referenceOrientation,
                weight);
        }
        else
        {
            math::Vec3 relative = math::eulerAngles(
                math::inverse(previousReferenceOrientation_) * referenceOrientation)
                * math::RadToDeg;
            for (int index = 0; index < 3; ++index)
            {
                relative[index] = math::normalizeAngleDegrees(relative[index]);
                if (std::abs(relative[index]) < 0.01f)
                    relative[index] = 0.0f;
            }
            relative = detachedDamp(
                relative,
                settings.effectiveRotationDamping(),
                deltaTime,
                followTargetAttachment);
            dampedOrientation = math::normalize(
                previousReferenceOrientation_
                * math::quatFromEuler(relative * math::DegToRad));
        }
    }

    previousReferenceOrientation_ = dampedOrientation;
    hasPreviousReferenceOrientation_ = true;

    const math::Quat offsetRotation = settings.bindingMode == BindingMode::LazyFollow
        ? target.rotation
        : dampedOrientation;
    const math::Vec3 targetPosition =
        target.position + math::rotate(offsetRotation, targetOffset);

    math::Vec3 currentPosition = previousTargetPosition_;
    const math::Vec3 previousOffset = previousValid
        ? previousOffset_
        : desiredCameraOffset;
    const math::Vec3 offsetDelta = desiredCameraOffset - previousOffset;
    if (offsetDelta.sqrMagnitude() > 0.01f)
    {
        const math::Quat offsetRotationDelta = math::safeFromToRotation(
            previousOffset,
            desiredCameraOffset,
            worldUp);
        currentPosition =
            targetPosition
            + math::rotate(offsetRotationDelta, previousTargetPosition_ - targetPosition);
    }
    previousOffset_ = desiredCameraOffset;

    math::Vec3 positionDelta = targetPosition - currentPosition;
    if (previousValid)
    {
        const math::Quat dampingSpace = math::almostZero(desiredCameraOffset)
            ? cameraState.rawOrientation
            : math::lookRotation(math::rotate(dampedOrientation, desiredCameraOffset), worldUp);
        math::Vec3 localDelta = math::rotate(math::inverse(dampingSpace), positionDelta);
        localDelta = detachedDamp(
            localDelta,
            settings.effectivePositionDamping(),
            deltaTime,
            followTargetAttachment);
        positionDelta = math::rotate(dampingSpace, localDelta);
    }
    currentPosition += positionDelta;

    previousTargetPosition_ = currentPosition;
    outTargetPosition = currentPosition;
    outTargetOrient = dampedOrientation;
}

math::Vec3 Tracker::getOffsetForMinimumTargetDistance(
    float followTargetAttachment,
    math::Vec3 dampedTargetPos,
    math::Vec3 cameraOffset,
    math::Vec3 cameraForward,
    math::Vec3 worldUp,
    math::Vec3 actualTargetPos)
{
    if (followTargetAttachment <= 1.0f - math::Epsilon)
        return math::Vec3::zero;

    cameraOffset = math::projectOnPlane(cameraOffset, worldUp);
    const float minDistance = cameraOffset.length() * 0.2f;
    if (minDistance <= 0.0f)
        return math::Vec3::zero;

    actualTargetPos = math::projectOnPlane(actualTargetPos, worldUp);
    dampedTargetPos = math::projectOnPlane(dampedTargetPos, worldUp);
    const math::Vec3 cameraPosition = dampedTargetPos + cameraOffset;
    const math::Vec3 toCamera = (dampedTargetPos - cameraPosition).normalized();
    const float distance = math::dot(actualTargetPos - cameraPosition, toCamera);
    if (distance >= minDistance)
        return math::Vec3::zero;

    math::Vec3 direction = actualTargetPos - dampedTargetPos;
    const float directionLength = direction.length();
    if (directionLength < 0.01f)
        direction = -math::projectOnPlane(cameraForward, worldUp);
    else
        direction /= directionLength;

    const math::Vec3 positionOffset = direction * (minDistance - distance);
    previousTargetPosition_ += positionOffset;
    return positionOffset;
}

void Tracker::onTargetObjectWarped(math::Vec3 positionDelta)
{
    previousTargetPosition_ += positionDelta;
}

void Tracker::setPreviousTargetPosition(math::Vec3 position)
{
    previousTargetPosition_ = position;
}

void Tracker::onForceCameraPosition(
    const TrackerTarget& target,
    const TrackerSettings& settings,
    math::Vec3 targetOffset,
    const CameraState& oldState,
    const CameraState& newState)
{
    if (!target.valid)
        return;

    const math::Vec3 worldUp = newState.referenceUp;
    const math::Quat oldOrientation = getReferenceOrientation(
        target,
        settings.bindingMode,
        targetOffset,
        worldUp,
        oldState);
    const math::Quat newOrientation = getReferenceOrientation(
        target,
        settings.bindingMode,
        targetOffset,
        worldUp,
        newState);

    const math::Vec3 cameraOffset = math::rotate(
        math::inverse(oldState.finalOrientation()),
        previousTargetPosition_ - oldState.finalPosition());
    previousTargetPosition_ =
        math::rotate(newState.finalOrientation(), cameraOffset)
        + newState.finalPosition();
    previousReferenceOrientation_ = newOrientation;
    hasPreviousReferenceOrientation_ = true;

    previousOffset_ = math::rotate(
        newOrientation,
        math::rotate(math::inverse(oldOrientation), previousOffset_));
    if (settings.bindingMode == BindingMode::WorldSpace)
    {
        const math::Quat rotationDelta =
            newState.finalOrientation() * math::inverse(oldState.finalOrientation());
        previousOffset_ = math::rotate(rotationDelta, previousOffset_);
    }
}

void Tracker::reset() noexcept
{
    previousTarget_ = TargetRef{};
    previousTargetPosition_ = math::Vec3::zero;
    previousReferenceOrientation_ = math::Quat::identity();
    previousOffset_ = math::Vec3::zero;
    targetOrientationOnAssign_ = math::Quat::identity();
    hasPreviousReferenceOrientation_ = false;
}

const math::Vec3& Tracker::previousTargetPosition() const noexcept
{
    return previousTargetPosition_;
}

const math::Quat& Tracker::previousReferenceOrientation() const noexcept
{
    return previousReferenceOrientation_;
}

const math::Vec3& Tracker::previousOffset() const noexcept
{
    return previousOffset_;
}

bool Tracker::hasPreviousReferenceOrientation() const noexcept
{
    return hasPreviousReferenceOrientation_;
}

} // namespace cinerig
