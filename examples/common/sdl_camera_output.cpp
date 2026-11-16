#include "sdl_camera_output.hpp"

#include <algorithm>
#include <cmath>

namespace cinerig::examples
{

math::Mat4 makeViewMatrix(math::Vec3 position, math::Quat orientation)
{
    const math::Mat4 cameraToWorld =
        math::Mat4::translation(position) * math::Mat4::rotation(orientation);
    return math::inverseAffine(cameraToWorld);
}

math::Mat4 makePerspectiveProjection(float fieldOfViewDegrees, float aspect, float nearPlane, float farPlane)
{
    const float safeAspect = aspect > math::Epsilon ? aspect : 1.0f;
    const float safeFov = math::clamp(fieldOfViewDegrees, 0.01f, 179.0f);
    const float safeNear = std::max(nearPlane, 0.0001f);
    const float safeFar = std::max(farPlane, safeNear + 0.001f);
    const float focal = 1.0f / std::tan(safeFov * math::DegToRad * 0.5f);

    math::Mat4 result;
    result.at(0, 0) = focal / safeAspect;
    result.at(1, 1) = focal;
    result.at(2, 2) = (safeFar + safeNear) / (safeNear - safeFar);
    result.at(2, 3) = (2.0f * safeFar * safeNear) / (safeNear - safeFar);
    result.at(3, 2) = -1.0f;
    return result;
}

math::Mat4 makeOrthographicProjection(float halfHeight, float aspect, float nearPlane, float farPlane)
{
    const float safeAspect = aspect > math::Epsilon ? aspect : 1.0f;
    const float safeHalfHeight = std::max(halfHeight, 0.0001f);
    const float halfWidth = safeHalfHeight * safeAspect;
    const float safeNear = std::max(nearPlane, 0.0001f);
    const float safeFar = std::max(farPlane, safeNear + 0.001f);

    math::Mat4 result = math::Mat4::identity();
    result.at(0, 0) = 1.0f / halfWidth;
    result.at(1, 1) = 1.0f / safeHalfHeight;
    result.at(2, 2) = -2.0f / (safeFar - safeNear);
    result.at(2, 3) = -(safeFar + safeNear) / (safeFar - safeNear);
    return result;
}

SdlCameraOutput::SdlCameraOutput()
{
    state_.lens = defaultLens_;
    state_.lens.aspect = aspect_;
    updateMatrices();
}

SdlCameraOutput::SdlCameraOutput(const LensSettings& defaultLens, float aspect)
    : defaultLens_(defaultLens), aspect_(aspect > math::Epsilon ? aspect : 1.0f)
{
    state_.lens = defaultLens_;
    state_.lens.aspect = aspect_;
    updateMatrices();
}

void SdlCameraOutput::apply(const AppliedCamera& camera)
{
    applied_ = camera;

    // No active Cinemachine camera: keep the last host camera matrices.
    if (!camera.hasCamera)
    {
        hasCamera_ = false;
        return;
    }

    const LensSettings previousLens = state_.lens;
    state_ = camera.state;
    hasCamera_ = true;

    if (camera.aspect > math::Epsilon)
        aspect_ = camera.aspect;

    if (hasBlendHint(state_.blendHint, BlendHint::NoLens))
        state_.lens = previousLens;
    else
        defaultLens_ = state_.lens;

    state_.lens.aspect = aspect_;
    defaultLens_.aspect = aspect_;

    if (!hasBlendHint(state_.blendHint, BlendHint::NoPosition))
        position_ = state_.finalPosition();
    if (!hasBlendHint(state_.blendHint, BlendHint::NoOrientation))
        orientation_ = state_.finalOrientation();

    updateMatrices();
}

LensSettings SdlCameraOutput::outputLens() const
{
    return defaultLens_;
}

float SdlCameraOutput::aspect() const
{
    return aspect_;
}

void SdlCameraOutput::setAspect(float aspect)
{
    aspect_ = aspect > math::Epsilon ? aspect : 1.0f;
    state_.lens.aspect = aspect_;
    updateMatrices();
}

void SdlCameraOutput::setViewportSize(math::Vec2 size)
{
    const float width = std::max(1.0f, size.x);
    const float height = std::max(1.0f, size.y);
    setAspect(width / height);
}

void SdlCameraOutput::setDefaultLens(const LensSettings& lens)
{
    defaultLens_ = lens;
    defaultLens_.aspect = aspect_;
    state_.lens = defaultLens_;
    updateMatrices();
}

void SdlCameraOutput::setOutputLens(const LensSettings& lens)
{
    setDefaultLens(lens);
}

void SdlCameraOutput::setProjectionMode(LensSettings::OverrideMode mode)
{
    defaultLens_.modeOverride = mode;
    state_.lens.modeOverride = mode;
    if (mode == LensSettings::OverrideMode::Orthographic)
    {
        defaultLens_.orthographic = true;
        state_.lens.orthographic = true;
    }
    else if (mode == LensSettings::OverrideMode::Perspective
        || mode == LensSettings::OverrideMode::Physical)
    {
        defaultLens_.orthographic = false;
        state_.lens.orthographic = false;
    }
    updateMatrices();
}

void SdlCameraOutput::toggleProjection()
{
    const bool orthographic =
        state_.lens.modeOverride == LensSettings::OverrideMode::Orthographic
        || (state_.lens.modeOverride == LensSettings::OverrideMode::None && state_.lens.orthographic);
    setProjectionMode(orthographic
            ? LensSettings::OverrideMode::Perspective
            : LensSettings::OverrideMode::Orthographic);
}

void SdlCameraOutput::reset()
{
    applied_ = AppliedCamera{};
    state_ = CameraState::makeDefault();
    state_.lens = defaultLens_;
    state_.lens.aspect = aspect_;
    hasCamera_ = false;
    position_ = math::Vec3::zero;
    orientation_ = math::Quat::identity();
    updateMatrices();
}

bool SdlCameraOutput::hasCamera() const noexcept
{
    return hasCamera_;
}

const CameraState& SdlCameraOutput::state() const noexcept
{
    return state_;
}

const AppliedCamera& SdlCameraOutput::appliedCamera() const noexcept
{
    return applied_;
}

math::Mat4 SdlCameraOutput::viewMatrix() const
{
    return view_;
}

math::Mat4 SdlCameraOutput::projectionMatrix() const
{
    return projection_;
}

math::Vec3 SdlCameraOutput::position() const
{
    return position_;
}

math::Quat SdlCameraOutput::orientation() const
{
    return orientation_;
}

void SdlCameraOutput::updateMatrices()
{
    view_ = makeViewMatrix(position_, orientation_);

    const LensSettings& lens = state_.lens;
    const bool orthographic =
        lens.modeOverride == LensSettings::OverrideMode::Orthographic
        || (lens.modeOverride == LensSettings::OverrideMode::None && lens.orthographic);

    if (orthographic)
    {
        projection_ = makeOrthographicProjection(
            lens.orthographicSize, aspect_, lens.nearClipPlane, lens.farClipPlane);
    }
    else
    {
        projection_ = makePerspectiveProjection(
            lens.fieldOfView, aspect_, lens.nearClipPlane, lens.farClipPlane);
    }
}

}
