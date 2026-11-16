#include <cinerig/components/hard_look_at.hpp>

#include <cinerig/math/quat.hpp>

namespace cinerig
{

bool HardLookAt::isValid() const
{
    return isAttached() && lookAtTargetPosition().has_value();
}

void HardLookAt::mutateCameraState(CameraState& state, float deltaTime)
{
    (void)deltaTime;

    if (!isValid() || !state.hasLookAt())
        return;

    const math::Quat targetRotation = lookAtTargetRotation().value_or(math::Quat::identity());
    const math::Vec3 offset = math::rotate(targetRotation, lookAtOffset);
    const math::Vec3 direction = state.referenceLookAt + offset - state.correctedPosition();
    if (direction.length() <= math::Epsilon)
        return;

    const math::Vec3 normalizedDirection = math::normalize(direction);
    if (math::cross(normalizedDirection, state.referenceUp).length() < math::Epsilon)
        state.rawOrientation = math::fromToRotation(math::Vec3::forward, direction);
    else
        state.rawOrientation = math::lookRotation(direction, state.referenceUp);
}

} // namespace cinerig
