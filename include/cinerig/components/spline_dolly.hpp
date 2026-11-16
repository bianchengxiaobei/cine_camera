#pragma once

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/components/spline_roll.hpp>
#include <cinerig/core/spline_auto_dolly.hpp>
#include <cinerig/core/spline_settings.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class SplineDolly : public CameraComponent
{
public:
    enum class RotationMode
    {
        Default = 0,
        Spline,
        SplineNoRoll,
        FollowTarget,
        FollowTargetNoRoll,
    };

    struct DampingSettings
    {
        bool enabled = false;
        math::Vec3 position{};
        float angular = 0.0f;
    };

    SplineSettings spline{nullptr, 0.0f, PathIndexUnit::Normalized};
    const SplineRoll* splineRoll = nullptr;
    math::Vec3 splineOffset{};
    RotationMode cameraRotation = RotationMode::Default;
    DampingSettings damping{};
    SplineAutoDolly automaticDolly{};

    void reset();
    void validate();

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    float maxDampTime() const override;

private:
    math::Quat rotationAtSplinePoint(
        math::Quat splineRotation,
        math::Vec3 up,
        math::Quat defaultOrientation,
        bool& isDefault) const;

    float previousSplinePosition_ = 0.0f;
    math::Vec3 previousPosition_{};
    math::Quat previousRotation_{};
};

} // namespace cinerig
