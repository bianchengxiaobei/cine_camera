#pragma once

#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class FollowZoom : public CameraExtension
{
public:
    float width = 2.0f;
    float damping = 1.0f;
    math::Vec2 fovRange{3.0f, 60.0f};

    void validate();
    void reset(VirtualCamera& vcam);
    float maxDampTime() const override;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    struct CameraCache
    {
        float previousFrameZoom = 0.0f;
    };

    std::unordered_map<const VirtualCamera*, CameraCache> caches_;
};

} // namespace cinerig
