#include <cinerig/components/third_person_aim.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

void ThirdPersonAim::validate()
{
    aimDistance = std::max(1.0f, aimDistance);
}

math::Vec3 ThirdPersonAim::computeLookAtPoint(
    math::Vec3 cameraPosition,
    math::Vec3 playerPosition,
    math::Quat playerRotation,
    math::Vec3 forward) const
{
    math::Vec3 origin = cameraPosition;
    float distance = std::max(1.0f, aimDistance);

    const math::Vec3 local = math::rotate(
        math::inverse(playerRotation),
        playerPosition - cameraPosition);
    const float distanceAlongForward = -local.z;
    if (distanceAlongForward > 0.0f)
    {
        origin += forward * distanceAlongForward;
        distance -= distanceAlongForward;
    }
    distance = std::max(1.0f, distance);

    const VirtualCamera* camera = virtualCamera();
    const IPhysicsWorld* physics = camera != nullptr ? camera->physicsWorld() : nullptr;
    if (physics == nullptr || aimCollisionFilter.bits == 0u)
        return origin + forward * distance;

    const auto hit = physics->raycast(
        {origin, forward},
        distance,
        {aimCollisionFilter, ignoreEntity});
    return hit.has_value() ? hit->point : origin + forward * distance;
}

math::Vec3 ThirdPersonAim::computeAimTarget(
    math::Vec3 cameraLookAt,
    math::Vec3 playerPosition) const
{
    const math::Vec3 direction = cameraLookAt - playerPosition;
    const float distance = direction.length();
    if (distance < math::Epsilon)
        return cameraLookAt;

    const VirtualCamera* camera = virtualCamera();
    const IPhysicsWorld* physics = camera != nullptr ? camera->physicsWorld() : nullptr;
    if (physics == nullptr || aimCollisionFilter.bits == 0u)
        return cameraLookAt;

    const auto hit = physics->raycast(
        {playerPosition, direction / distance},
        distance,
        {aimCollisionFilter, ignoreEntity});
    return hit.has_value() ? hit->point : cameraLookAt;
}

void ThirdPersonAim::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)deltaTime;

    if (stage == Stage::Body)
    {
        if (!noiseCancellation)
            return;

        const TargetRef player = vcam.effectiveFollowTarget();
        const auto playerPosition = vcam.targetPosition(player);
        const auto playerRotation = vcam.targetRotation(player);
        if (!playerPosition.has_value() || !playerRotation.has_value())
            return;

        const math::Vec3 forward = math::rotate(*playerRotation, math::Vec3::forward);
        state.referenceLookAt = computeLookAtPoint(
            state.correctedPosition(),
            *playerPosition,
            *playerRotation,
            forward);
        aimTarget_ = computeAimTarget(state.referenceLookAt, *playerPosition);
    }
    else if (stage == Stage::Finalize)
    {
        if (noiseCancellation)
        {
            if (!state.hasLookAt())
                return;

            const math::Vec3 direction = state.referenceLookAt - state.finalPosition();
            if (direction.sqrMagnitude() > 0.01f)
            {
                state.rawOrientation = math::lookRotation(direction, state.referenceUp);
                state.orientationCorrection = math::Quat::identity();
            }
        }
        else
        {
            const TargetRef player = vcam.effectiveFollowTarget();
            const auto playerPosition = vcam.targetPosition(player);
            const auto playerRotation = vcam.targetRotation(player);
            if (!playerPosition.has_value() || !playerRotation.has_value())
                return;

            const math::Vec3 forward = math::rotate(
                state.correctedOrientation(),
                math::Vec3::forward);
            state.referenceLookAt = computeLookAtPoint(
                state.correctedPosition(),
                *playerPosition,
                *playerRotation,
                forward);
            aimTarget_ = computeAimTarget(state.referenceLookAt, *playerPosition);
        }
    }
}

} // namespace cinerig

