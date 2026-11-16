#include <cinerig/components/post_processing.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

namespace
{

constexpr float kMinFocusDistance = 0.01f;

float focusDistanceFor(
    const VirtualCamera& vcam,
    const CameraState& state,
    PostProcessing::FocusTrackingMode mode,
    TargetRef customTarget,
    float offset)
{
    float distance = offset;

    switch (mode)
    {
    case PostProcessing::FocusTrackingMode::LookAtTarget:
    {
        if (state.hasLookAt())
        {
            distance += (state.finalPosition() - state.referenceLookAt).length();
        }
        else if (const auto position = vcam.lookAtTargetPosition())
        {
            distance += (state.finalPosition() - *position).length();
        }
        break;
    }
    case PostProcessing::FocusTrackingMode::FollowTarget:
    {
        if (const auto position = vcam.followTargetPosition())
            distance += (state.finalPosition() - *position).length();
        break;
    }
    case PostProcessing::FocusTrackingMode::CustomTarget:
    {
        if (const auto position = vcam.targetPosition(customTarget))
            distance += (state.finalPosition() - *position).length();
        break;
    }
    case PostProcessing::FocusTrackingMode::Camera:
    case PostProcessing::FocusTrackingMode::None:
        break;
    }

    if (!std::isfinite(distance))
        distance = offset;
    return std::max(kMinFocusDistance, distance);
}

} // namespace

void PostProcessing::validate()
{
    if (!std::isfinite(weight))
        weight = 0.0f;
    weight = std::max(0.0f, weight);
    if (!std::isfinite(focusOffset))
        focusOffset = 0.0f;
}

void PostProcessing::reset(VirtualCamera& vcam)
{
    focusDistances_[&vcam] = std::max(kMinFocusDistance, focusOffset);
}

float PostProcessing::calculatedFocusDistance(
    const VirtualCamera& vcam) const noexcept
{
    const auto found = focusDistances_.find(&vcam);
    return found != focusDistances_.end() ? found->second : 0.0f;
}

void PostProcessing::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)deltaTime;
    if (stage != Stage::Finalize || !profile.isValid())
        return;

    if (focusTracking != FocusTrackingMode::None)
    {
        const float focusDistance = focusDistanceFor(
            vcam, state, focusTracking, focusTarget, focusOffset);
        focusDistances_[&vcam] = focusDistance;
        state.lens.physical.focusDistance = focusDistance;
    }

    CustomBlendable blendable;
    blendable.data = profile.data;
    blendable.type = profile.type;
    blendable.weight = weight;
    state.addCustomBlendable(std::move(blendable));
}

} // namespace cinerig

