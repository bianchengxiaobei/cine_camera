#pragma once

#include <cinerig/core/camera_state.hpp>
#include <cinerig/engine/camera_output.hpp>
#include <cinerig/math/matrix.hpp>

namespace cinerig::examples
{

math::Mat4 makeViewMatrix(math::Vec3 position, math::Quat orientation);
math::Mat4 makePerspectiveProjection(float fieldOfViewDegrees, float aspect, float nearPlane, float farPlane);
math::Mat4 makeOrthographicProjection(float halfHeight, float aspect, float nearPlane, float farPlane);

// Applies CineRig output to a plain OpenGL camera. The class is intentionally
// standalone: constructing it does not require a window or an OpenGL context.
class SdlCameraOutput final : public ICameraOutput
{
public:
    SdlCameraOutput();
    explicit SdlCameraOutput(const LensSettings& defaultLens, float aspect = 16.0f / 9.0f);

    void apply(const AppliedCamera& camera) override;
    LensSettings outputLens() const override;
    float aspect() const override;

    void setAspect(float aspect);
    void setViewportSize(math::Vec2 size);
    void setDefaultLens(const LensSettings& lens);
    void setOutputLens(const LensSettings& lens);
    void setProjectionMode(LensSettings::OverrideMode mode);
    void toggleProjection();
    void reset();

    bool hasCamera() const noexcept;
    const CameraState& state() const noexcept;
    const AppliedCamera& appliedCamera() const noexcept;

    math::Mat4 viewMatrix() const;
    math::Mat4 projectionMatrix() const;
    math::Vec3 position() const;
    math::Quat orientation() const;

private:
    void updateMatrices();

    LensSettings defaultLens_ = LensSettings::Default;
    CameraState state_ = CameraState::makeDefault();
    AppliedCamera applied_{};
    float aspect_ = 16.0f / 9.0f;
    bool hasCamera_ = false;
    math::Vec3 position_{};
    math::Quat orientation_{};
    math::Mat4 view_ = math::Mat4::identity();
    math::Mat4 projection_ = math::Mat4::identity();
};

}
