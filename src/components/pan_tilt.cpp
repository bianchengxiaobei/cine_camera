#include <cinerig/components/pan_tilt.hpp>

#include <algorithm>
#include <cmath>
#include <optional>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

InputAxis PanTilt::makeDefaultPanAxis()
{
    InputAxis axis;
    axis.value = 0.0f;
    axis.center = 0.0f;
    axis.range = {-180.0f, 180.0f};
    axis.wrap = true;
    axis.recentering = InputAxisRecenteringSettings::Default;
    return axis;
}

InputAxis PanTilt::makeDefaultTiltAxis()
{
    InputAxis axis;
    axis.value = 0.0f;
    axis.center = 0.0f;
    axis.range = {-70.0f, 70.0f};
    axis.wrap = false;
    axis.recentering = InputAxisRecenteringSettings::Default;
    return axis;
}

bool PanTilt::isValid() const
{
    return isAttached();
}

void PanTilt::validate()
{
    panAxis.validate();
    tiltAxis.range.x = std::clamp(tiltAxis.range.x, -90.0f, 90.0f);
    tiltAxis.range.y = std::clamp(tiltAxis.range.y, -90.0f, 90.0f);
    tiltAxis.validate();
}

math::Quat PanTilt::getReferenceFrame(math::Vec3 up) const
{
    switch (referenceFrame)
    {
    case ReferenceFrame::ParentObject:
        if (VirtualCamera* camera = virtualCameraOrNull())
        {
            if (VirtualCamera* parent = camera->parentVirtualCamera())
                return parent->state().rawOrientation;
        }
        break;
    case ReferenceFrame::TrackingTarget:
        if (const auto rotation = followTargetRotation())
            return *rotation;
        break;
    case ReferenceFrame::LookAtTarget:
        if (const auto rotation = lookAtTargetRotation())
            return *rotation;
        break;
    case ReferenceFrame::World:
        break;
    }

    return math::fromToRotation(math::Vec3::up, up);
}

math::Vec2 PanTilt::anglesForDirection(
    math::Vec3 worldDirection,
    math::Vec3 up) const
{
    const math::Quat reference = getReferenceFrame(up);
    math::Vec3 local = math::rotate(math::inverse(reference), worldDirection);
    if (local.length() < math::Epsilon)
        return {panAxis.center, tiltAxis.center};

    local = local.normalized();

    float pan = 0.0f;
    const float horizontalSquared = local.x * local.x + local.z * local.z;
    if (horizontalSquared > math::Epsilon)
        pan = std::atan2(local.x, -local.z) * math::RadToDeg;

    const float tilt =
        -std::asin(std::clamp(local.y, -1.0f, 1.0f)) * math::RadToDeg;

    return {math::normalizeAngleDegrees(pan), tilt};
}

math::Vec2 PanTilt::getRecenterTarget() const
{
    if (recenterTarget == RecenterTargetMode::AxisCenter)
        return {panAxis.center, tiltAxis.center};

    const std::optional<math::Quat> rotation =
        recenterTarget == RecenterTargetMode::TrackingTargetForward
        ? followTargetRotation()
        : lookAtTargetRotation();
    if (!rotation.has_value())
        return {panAxis.center, tiltAxis.center};

    math::Vec3 up = math::Vec3::up;
    if (const VirtualCamera* camera = virtualCameraOrNull())
        up = camera->state().referenceUp;

    return anglesForDirection(
        math::rotate(*rotation, math::Vec3::forward),
        up);
}

void PanTilt::setAxesForRotation(math::Quat rotation, math::Vec3 up)
{
    const math::Vec2 angles = anglesForDirection(
        math::rotate(rotation, math::Vec3::forward),
        up);

    panAxis.cancelRecentering();
    tiltAxis.cancelRecentering();
    panAxis.setValueAndLastValue(panAxis.clampValue(angles.x));
    tiltAxis.setValueAndLastValue(tiltAxis.clampValue(angles.y));

    previousRotation_ = rotation;
    historyValid_ = true;
}

void PanTilt::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached())
        return;

    VirtualCamera& camera = virtualCamera();
    if (deltaTime < 0.0f || !camera.previousStateIsValid)
    {
        panAxis.cancelRecentering();
        tiltAxis.cancelRecentering();
    }

    const math::Quat reference = getReferenceFrame(state.referenceUp);
    const math::Quat rotation = reference * math::quatFromEuler({
        -tiltAxis.value * math::DegToRad,
        -panAxis.value * math::DegToRad,
        0.0f,
    });

    if (camera.previousStateIsValid && historyValid_)
    {
        state.rotationDampingBypass = state.rotationDampingBypass
            * math::safeFromToRotation(
                math::rotate(previousRotation_, math::Vec3::forward),
                math::rotate(rotation, math::Vec3::forward),
                state.referenceUp);
    }

    state.rawOrientation = rotation;
    previousRotation_ = rotation;
    historyValid_ = true;

    const float currentTime = camera.currentTime();
    bool gotPanInput = panAxis.trackValueChange(currentTime);
    bool gotTiltInput = tiltAxis.trackValueChange(currentTime);

    if (panAxis.recentering.time == tiltAxis.recentering.time)
    {
        const bool anyInput = gotPanInput || gotTiltInput;
        gotPanInput = anyInput;
        gotTiltInput = anyInput;
    }

    const math::Vec2 recenter = getRecenterTarget();
    panAxis.updateRecentering(
        deltaTime,
        gotPanInput,
        currentTime,
        recenter.x);
    tiltAxis.updateRecentering(
        deltaTime,
        gotTiltInput,
        currentTime,
        recenter.y);
}

void PanTilt::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    (void)pos;
    if (!isAttached())
        return;
    setAxesForRotation(rot, virtualCamera().state().referenceUp);
}

bool PanTilt::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    (void)worldUp;
    (void)deltaTime;

    panAxis.cancelRecentering();
    tiltAxis.cancelRecentering();

    if (from == nullptr || !isAttached() || virtualCamera().isLive())
        return false;

    const bool inheritsPosition =
        hasBlendHint(virtualCamera().blendHint, BlendHint::InheritPosition)
        || hasBlendHint(virtualCamera().state().blendHint, BlendHint::InheritPosition);
    if (!inheritsPosition)
        return false;

    setAxesForRotation(
        from->state().rawOrientation,
        virtualCamera().state().referenceUp);
    return true;
}

void PanTilt::getInputAxes(std::vector<InputAxisDescriptor>& axes)
{
    axes.push_back({"Look X (Pan)", &panAxis, InputAxisHint::X});
    axes.push_back({"Look Y (Tilt)", &tiltAxis, InputAxisHint::Y});
}

float PanTilt::getNormalizedModifierValue()
{
    const float range = tiltAxis.range.y - tiltAxis.range.x;
    return (tiltAxis.value - tiltAxis.range.x) / (range > 0.001f ? range : 1.0f) * 2.0f - 1.0f;
}

} // namespace cinerig

