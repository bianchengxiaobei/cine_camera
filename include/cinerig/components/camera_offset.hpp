#pragma once

#include <cinerig/camera/camera_extension.hpp>

namespace cinerig
{

class CameraOffset : public CameraExtension
{
public:
    math::Vec3 offset{};
    Stage applyAfter = Stage::Aim;
    bool preserveComposition = false;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
