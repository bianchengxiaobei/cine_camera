#include <cinerig/components/auto_focus.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig
{

namespace
{

constexpr float kMinFocusDistance = 0.1f;

float targetDistance(
    const VirtualCamera& vcam,
    const CameraState& state,
    AutoFocus::FocusMode mode,
    TargetRef customTarget,
    float offset)
{
    float distance = offset;

    switch (mode)
    {
    case AutoFocus::FocusMode::LookAtTarget:
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
    case AutoFocus::FocusMode::FollowTarget:
    {
        if (const auto position = vcam.followTargetPosition())
            distance += (state.finalPosition() - *position).length();
        break;
    }
    case AutoFocus::FocusMode::CustomTarget:
    {
        if (const auto position = vcam.targetPosition(customTarget))
            distance += (state.finalPosition() - *position).length();
        break;
    }
    case AutoFocus::FocusMode::Camera:
    case AutoFocus::FocusMode::None:
        break;
    }

    if (!std::isfinite(distance))
        distance = offset;
    return std::max(kMinFocusDistance, distance);
}

} // namespace

void AutoFocus::validate()
{
    if (!std::isfinite(damping))
        damping = 0.0f;
    damping = std::max(0.0f, damping);
    if (!std::isfinite(focusOffset))
        focusOffset = 0.0f;
}

void AutoFocus::reset(VirtualCamera& vcam)
{
    focusDistances_[&vcam] = std::max(kMinFocusDistance, focusOffset);
}

float AutoFocus::currentFocusDistance(
    const VirtualCamera& vcam) const noexcept
{
    const auto found = focusDistances_.find(&vcam);
    return found != focusDistances_.end() ? found->second : 0.0f;
}

float AutoFocus::maxDampTime() const
{
    return damping;
}

void AutoFocus::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    if (stage != Stage::Finalize || focusMode == FocusMode::None)
        return;

    const float target = targetDistance(
        vcam, state, focusMode, customTarget, focusOffset);

    float current = focusDistances_.try_emplace(&vcam, 0.0f).first->second;
    if (deltaTime >= 0.0f && vcam.previousStateIsValid && damping > 0.0f)
        current += math::standardDamp(target - current, damping, deltaTime);
    else
        current = target;

    focusDistances_[&vcam] = current;
    state.lens.physical.focusDistance = current;
}

} // namespace cinerig

