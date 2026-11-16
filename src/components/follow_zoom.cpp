#include <cinerig/components/follow_zoom.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

void FollowZoom::validate()
{
    width = std::max(0.0f, width);
    damping = std::max(0.0f, damping);
    fovRange.y = math::clamp(fovRange.y, 1.0f, 179.0f);
    fovRange.x = math::clamp(fovRange.x, 1.0f, fovRange.y);
}

void FollowZoom::reset(VirtualCamera& vcam)
{
    caches_[&vcam].previousFrameZoom = vcam.lens.fieldOfView;
}

float FollowZoom::maxDampTime() const
{
    return damping;
}

void FollowZoom::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    CameraCache& cache = caches_[&vcam];
    if (deltaTime < 0.0f || !vcam.previousStateIsValid)
        cache.previousFrameZoom = state.lens.fieldOfView;

    if (stage != Stage::Body)
        return;

    float fov = 179.0f;
    if (state.hasLookAt())
    {
        const float d = (state.referenceLookAt - state.correctedPosition()).length();
        if (d > math::Epsilon)
        {
            const float minWidth = d * 2.0f
                * std::tan(fovRange.x * math::DegToRad * 0.5f);
            const float maxWidth = d * 2.0f
                * std::tan(fovRange.y * math::DegToRad * 0.5f);
            float targetWidth = math::clamp(width, minWidth, maxWidth);

            if (deltaTime >= 0.0f && damping > 0.0f && vcam.previousStateIsValid)
            {
                const float currentWidth = d * 2.0f
                    * std::tan(cache.previousFrameZoom * math::DegToRad * 0.5f);
                const float delta = vcam.detachedLookAtTargetDamp(
                    targetWidth - currentWidth,
                    damping,
                    deltaTime);
                targetWidth = currentWidth + delta;
            }
            fov = 2.0f * std::atan(targetWidth / (2.0f * d)) * math::RadToDeg;
        }
    }

    cache.previousFrameZoom = math::clamp(fov, fovRange.x, fovRange.y);
    state.lens.fieldOfView = cache.previousFrameZoom;
}

} // namespace cinerig
