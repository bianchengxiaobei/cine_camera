#pragma once

#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/engine/post_processor.hpp>

namespace cinerig
{

class PostProcessing : public CameraExtension
{
public:
    enum class FocusTrackingMode
    {
        None = 0,
        LookAtTarget,
        FollowTarget,
        CustomTarget,
        Camera,
    };

    PostProcessProfile profile;
    float weight = 1.0f;
    FocusTrackingMode focusTracking = FocusTrackingMode::None;
    TargetRef focusTarget;
    float focusOffset = 0.0f;

    void validate();
    void reset(VirtualCamera& vcam);
    float calculatedFocusDistance(const VirtualCamera& vcam) const noexcept;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    std::unordered_map<const VirtualCamera*, float> focusDistances_;
};

} // namespace cinerig
