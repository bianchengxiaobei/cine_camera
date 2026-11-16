#pragma once

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/input_axis.hpp>
#include <cinerig/core/input_axis_owner.hpp>
#include <cinerig/core/modifier_interfaces.hpp>

namespace cinerig
{

class PanTilt
    : public CameraComponent
    , public IInputAxisOwner
    , public IModifierValueSource
{
public:
    enum class ReferenceFrame
    {
        ParentObject,
        World,
        TrackingTarget,
        LookAtTarget,
    };

    enum class RecenterTargetMode
    {
        AxisCenter,
        TrackingTargetForward,
        LookAtTargetForward,
    };

    static InputAxis makeDefaultPanAxis();
    static InputAxis makeDefaultTiltAxis();

    ReferenceFrame referenceFrame = ReferenceFrame::ParentObject;
    RecenterTargetMode recenterTarget = RecenterTargetMode::AxisCenter;
    InputAxis panAxis = makeDefaultPanAxis();
    InputAxis tiltAxis = makeDefaultTiltAxis();

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    bool onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    void getInputAxes(std::vector<InputAxisDescriptor>& axes) override;

    float getNormalizedModifierValue() override;
    math::Vec2 getRecenterTarget() const;
    void validate();

private:
    math::Quat getReferenceFrame(math::Vec3 up) const;
    void setAxesForRotation(math::Quat rotation, math::Vec3 up);
    math::Vec2 anglesForDirection(
        math::Vec3 worldDirection,
        math::Vec3 up) const;

    math::Quat previousRotation_{};
    bool historyValid_ = false;
};

} // namespace cinerig
