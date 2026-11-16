#pragma once

#include <cinerig/camera/camera_component.hpp>

namespace cinerig
{

class HardLookAt : public CameraComponent
{
public:
    math::Vec3 lookAtOffset{};

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
};

} // namespace cinerig
