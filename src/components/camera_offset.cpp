#include <cinerig/components/camera_offset.hpp>

#include <cinerig/math/geometry.hpp>
#include <cinerig/math/quat.hpp>

namespace cinerig
{

void CameraOffset::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)vcam;
    (void)deltaTime;

    if (stage != applyAfter)
        return;

    const bool preserveAim =
        preserveComposition && state.hasLookAt() && stage > Stage::Body;

    math::Vec2 screenOffset{};
    if (preserveAim)
    {
        screenOffset = math::getCameraRotationToTarget(
            state.rawOrientation,
            state.referenceLookAt - state.correctedPosition(),
            state.referenceUp);
    }

    const math::Vec3 worldOffset = math::rotate(state.rawOrientation, offset);
    state.positionCorrection += worldOffset;
    if (!preserveAim)
    {
        state.referenceLookAt += worldOffset;
        return;
    }

    math::Quat orientation = math::lookRotation(
        state.referenceLookAt - state.correctedPosition(),
        state.referenceUp);
    orientation = math::applyCameraRotation(
        orientation,
        {-screenOffset.x, -screenOffset.y},
        state.referenceUp);
    state.rawOrientation = orientation;
}

} // namespace cinerig
