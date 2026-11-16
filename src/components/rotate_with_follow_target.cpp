#include <cinerig/components/rotate_with_follow_target.hpp>

#include <algorithm>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig
{

bool RotateWithFollowTarget::isValid() const
{
    return isAttached() && followTargetPosition().has_value();
}

void RotateWithFollowTarget::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isValid())
        return;

    const math::Quat targetRotation =
        followTargetRotation().value_or(math::Quat::identity());
    math::Quat dampedOrientation = targetRotation;
    if (deltaTime >= 0.0f)
    {
        const float attachment = math::clamp01(virtualCamera().followTargetAttachment);
        const float effectiveDamping = math::lerp(
            std::max(1.0f, damping),
            damping,
            attachment);
        const float effectiveDeltaTime = math::lerp(0.0f, deltaTime, attachment);
        const float weight = math::standardDamp(1.0f, effectiveDamping, effectiveDeltaTime);
        dampedOrientation = math::slerp(previousReferenceOrientation_, targetRotation, weight);
    }

    previousReferenceOrientation_ = dampedOrientation;
    state.rawOrientation = dampedOrientation;
    state.referenceUp = math::rotate(dampedOrientation, math::Vec3::up);
}

float RotateWithFollowTarget::maxDampTime() const
{
    return damping;
}

} // namespace cinerig
