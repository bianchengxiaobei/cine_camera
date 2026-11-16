#include <cinerig/components/recomposer.hpp>

#include <algorithm>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/quat.hpp>

namespace cinerig
{

void Recomposer::validate()
{
    zoomScale = std::max(0.01f, zoomScale);
    followAttachment = math::clamp01(followAttachment);
    lookAtAttachment = math::clamp01(lookAtAttachment);
}

void Recomposer::prePipelineMutateCameraStateCallback(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime)
{
    (void)state;
    (void)deltaTime;

    vcam.followTargetAttachment = followAttachment;
    vcam.lookAtTargetAttachment = lookAtAttachment;
}

void Recomposer::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)vcam;
    (void)deltaTime;

    if (stage != applyAfter)
        return;

    LensSettings lens = state.lens;

    const math::Quat tilted = state.rawOrientation
        * math::quatFromAxisAngle(math::Vec3::right, tilt * math::DegToRad);
    const math::Quat desired = math::quatFromAxisAngle(
        state.referenceUp,
        pan * math::DegToRad)
        * tilted;
    state.orientationCorrection =
        math::inverse(state.correctedOrientation()) * desired;

    lens.dutch += dutch;
    if (zoomScale != 1.0f)
    {
        lens.orthographicSize *= zoomScale;
        lens.fieldOfView *= zoomScale;
    }
    state.lens = lens;
}

} // namespace cinerig
