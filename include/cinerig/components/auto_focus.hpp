#pragma once

#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/core/camera_target.hpp>

namespace cinerig
{

class AutoFocus : public CameraExtension
{
public:
    enum class FocusMode
    {
        None = 0,
        LookAtTarget,
        FollowTarget,
        CustomTarget,
        Camera,
    };

    FocusMode focusMode = FocusMode::None;
    TargetRef customTarget;
    float focusOffset = 0.0f;
    float damping = 0.2f;

    void validate();
    void reset(VirtualCamera& vcam);
    float currentFocusDistance(const VirtualCamera& vcam) const noexcept;
    float maxDampTime() const override;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    std::unordered_map<const VirtualCamera*, float> focusDistances_;
};

} // namespace cinerig
