#pragma once

#include <string>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/core/priority.hpp>
#include <cinerig/engine/camera_output.hpp>

namespace cinerig
{

// Exposes a host-engine camera to the Cinemachine camera stack so that it can
// participate in priority selection and blending. It is read-only: updating the
// external camera never writes back to the host entity.
class ExternalCamera : public VirtualCamera
{
public:
    explicit ExternalCamera(std::string name = "ExternalCamera");
    ~ExternalCamera() override;

    EntityId cameraEntity = 0;
    ICameraOutput* output = nullptr;
    TargetRef lookAtTarget{};

    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override;
    TargetRef effectiveLookAtTarget() const override;
};

} // namespace cinerig
