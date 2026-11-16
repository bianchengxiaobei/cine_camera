#include <cinerig/components/hard_lock_to_target.hpp>

#include <algorithm>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

bool HardLockToTarget::isValid() const
{
    return followTargetPosition().has_value();
}

void HardLockToTarget::validate()
{
    damping = std::max(0.0f, damping);
}

void HardLockToTarget::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached())
        return;

    const auto targetPosition = followTargetPosition();
    if (!targetPosition.has_value())
        return;

    VirtualCamera& camera = virtualCamera();
    if (camera.previousStateIsValid && deltaTime >= 0.0f)
    {
        const math::Vec3 delta = *targetPosition - previousTargetPosition_;
        state.rawPosition = previousTargetPosition_
            + camera.detachedFollowTargetDamp(delta, damping, deltaTime);
    }
    else
    {
        state.rawPosition = *targetPosition;
    }

    previousTargetPosition_ = state.rawPosition;
}

void HardLockToTarget::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    if (!isAttached())
        return;
    if (virtualCamera().isEntityTarget(virtualCamera().effectiveFollowTarget(), target))
        previousTargetPosition_ += positionDelta;
}

float HardLockToTarget::maxDampTime() const
{
    return damping;
}

} // namespace cinerig

