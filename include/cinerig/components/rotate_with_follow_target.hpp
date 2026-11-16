#pragma once

#include <cinerig/camera/camera_component.hpp>

namespace cinerig
{

class RotateWithFollowTarget : public CameraComponent
{
public:
    float damping = 0.0f;

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    float maxDampTime() const override;

private:
    math::Quat previousReferenceOrientation_{};
};

} // namespace cinerig
