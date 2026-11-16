#include <cinerig/components/follow.hpp>

#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

TrackerTarget Follow::trackingTarget() const
{
    TrackerTarget target;
    if (!isAttached())
        return target;

    const auto position = followTargetPosition();
    const auto rotation = followTargetRotation();
    if (!position.has_value() || !rotation.has_value())
        return target;

    target.ref = virtualCamera().effectiveFollowTarget();
    target.position = *position;
    target.rotation = *rotation;
    target.valid = true;
    return target;
}

bool Follow::isValid() const
{
    return trackingTarget().valid;
}

math::Vec3 Follow::effectiveOffset() const
{
    math::Vec3 offset = followOffset;
    if (trackerSettings.bindingMode == BindingMode::LazyFollow)
    {
        offset.x = 0.0f;
        offset.z = -std::abs(offset.z);
    }
    return offset;
}

void Follow::mutateCameraState(CameraState& state, float deltaTime)
{
    const TrackerTarget target = trackingTarget();
    VirtualCamera& camera = virtualCamera();
    const math::Vec3 offset = effectiveOffset();

    targetTracker_.initStateInfo(
        target,
        deltaTime,
        camera.previousStateIsValid,
        trackerSettings,
        math::Vec3::zero,
        state.referenceUp,
        state);

    if (!target.valid)
        return;

    math::Vec3 targetPosition;
    math::Quat targetOrientation;
    targetTracker_.trackTarget(
        target,
        deltaTime,
        camera.previousStateIsValid,
        state.referenceUp,
        offset,
        trackerSettings,
        math::Vec3::zero,
        state,
        camera.followTargetAttachment,
        targetPosition,
        targetOrientation);

    const math::Vec3 rotatedOffset = math::rotate(targetOrientation, offset);
    state.referenceUp = math::rotate(targetOrientation, math::Vec3::up);

    targetPosition += targetTracker_.getOffsetForMinimumTargetDistance(
        camera.followTargetAttachment,
        targetPosition,
        rotatedOffset,
        math::rotate(state.rawOrientation, math::Vec3::forward),
        state.referenceUp,
        target.position);

    state.rawPosition = targetPosition + rotatedOffset;
}

void Follow::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    if (!isAttached())
        return;
    if (virtualCamera().isEntityTarget(virtualCamera().effectiveFollowTarget(), target))
        targetTracker_.onTargetObjectWarped(positionDelta);
}

void Follow::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    if (!isAttached())
        return;

    const TrackerTarget target = trackingTarget();
    if (!target.valid)
        return;

    const CameraState oldState = virtualCamera().state();
    CameraState newState = oldState;
    newState.rawPosition = pos;
    newState.rawOrientation = rot;
    newState.positionCorrection = math::Vec3::zero;
    newState.orientationCorrection = math::Quat::identity();
    targetTracker_.onForceCameraPosition(
        target,
        trackerSettings,
        math::Vec3::zero,
        oldState,
        newState);
}

float Follow::maxDampTime() const
{
    return trackerSettings.maxDampTime();
}

} // namespace cinerig
