#pragma once

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/engine/physics_world.hpp>

namespace cinerig
{

class ThirdPersonAim : public CameraExtension
{
public:
    LayerMask aimCollisionFilter{1u};
    EntityId ignoreEntity = 0;
    float aimDistance = 200.0f;
    bool noiseCancellation = true;

    void validate();
    const math::Vec3& aimTarget() const noexcept { return aimTarget_; }

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    math::Vec3 computeLookAtPoint(
        math::Vec3 cameraPosition,
        math::Vec3 playerPosition,
        math::Quat playerRotation,
        math::Vec3 forward) const;
    math::Vec3 computeAimTarget(
        math::Vec3 cameraLookAt,
        math::Vec3 playerPosition) const;

    math::Vec3 aimTarget_{};
};

} // namespace cinerig
