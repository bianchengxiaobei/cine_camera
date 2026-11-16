#pragma once

#include <cinerig/camera/camera_extension.hpp>

namespace cinerig
{

class Recomposer : public CameraExtension
{
public:
    Stage applyAfter = Stage::Finalize;
    float tilt = 0.0f;
    float pan = 0.0f;
    float dutch = 0.0f;
    float zoomScale = 1.0f;
    float followAttachment = 1.0f;
    float lookAtAttachment = 1.0f;

    void validate();

    void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam,
        CameraState& state,
        float deltaTime) override;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
};

} // namespace cinerig
