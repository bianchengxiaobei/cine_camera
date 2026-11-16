#pragma once

#include <cinerig/camera/camera_component.hpp>

namespace cinerig
{

class HardLockToTarget : public CameraComponent
{
public:
    float damping = 0.0f;

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    float maxDampTime() const override;
    void validate();

private:
    math::Vec3 previousTargetPosition_{};
};

} // namespace cinerig
