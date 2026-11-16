#pragma once

#include <cstdint>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/input_axis.hpp>
#include <cinerig/core/input_axis_owner.hpp>
#include <cinerig/core/modifier_interfaces.hpp>
#include <cinerig/core/orbit_rig.hpp>
#include <cinerig/core/tracker.hpp>

namespace cinerig
{

class OrbitalFollow
    : public CameraComponent
    , public IInputAxisOwner
    , public IModifierValueSource
    , public IModifiablePositionDamping
    , public IModifiableDistance
{
public:
    enum class OrbitStyle
    {
        Sphere,
        ThreeRing,
    };

    enum class ReferenceFrame
    {
        AxisCenter,
        ParentObject,
        TrackingTarget,
        LookAtTarget,
    };

    static InputAxis makeDefaultHorizontalAxis();
    static InputAxis makeDefaultVerticalAxis();
    static InputAxis makeDefaultRadialAxis();

    math::Vec3 targetOffset{0.0f, 0.0f, 0.0f};
    TrackerSettings trackerSettings{};
    OrbitStyle orbitStyle = OrbitStyle::Sphere;
    float radius = 10.0f;
    OrbitRigSettings orbits{};
    ReferenceFrame recenteringTarget = ReferenceFrame::TrackingTarget;
    InputAxis horizontalAxis = makeDefaultHorizontalAxis();
    InputAxis verticalAxis = makeDefaultVerticalAxis();
    InputAxis radialAxis = makeDefaultRadialAxis();

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    bool onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    float maxDampTime() const override;
    void getInputAxes(std::vector<InputAxisDescriptor>& axes) override;

    float getNormalizedModifierValue() override;
    math::Vec3 getPositionDamping() const override { return trackerSettings.positionDamping; }
    void setPositionDamping(math::Vec3 value) override { trackerSettings.positionDamping = value; }
    float getDistance() const override { return radius; }
    void setDistance(float value) override { radius = value; }

private:
    TrackerTarget trackingTarget() const;
    math::Vec4 axisValues() const;
    math::Vec4 cameraPoint(const math::Vec4& axisValues);
    void updateHorizontalCenter(math::Quat referenceOrientation);
    void inferAxesFromPositionSphere(
        math::Vec3 dir,
        float distance,
        const CameraState& state);
    void inferAxesFromPositionThreeRing(
        math::Vec3 dir,
        float distance,
        const CameraState& state);

    Tracker tracker_;
    OrbitSplineCache orbitCache_;
    math::Vec4 previousAxisValues_{};
    bool historyValid_ = false;
};

} // namespace cinerig
