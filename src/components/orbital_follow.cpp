#include <cinerig/components/orbital_follow.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

namespace
{

constexpr float kMinimumForceDistance = 0.001f;
constexpr float kSteepestDescentEpsilon = 0.005f;
constexpr int kSteepestDescentIterations = 5;

float mapTo01(float value, float from, float to)
{
    const float range = to - from;
    return std::abs(range) < math::Epsilon ? 0.0f : (value - from) / range;
}

} // namespace

InputAxis OrbitalFollow::makeDefaultHorizontalAxis()
{
    InputAxis axis;
    axis.value = 0.0f;
    axis.center = 0.0f;
    axis.range = {-180.0f, 180.0f};
    axis.wrap = true;
    axis.recentering = InputAxisRecenteringSettings::Default;
    return axis;
}

InputAxis OrbitalFollow::makeDefaultVerticalAxis()
{
    InputAxis axis;
    axis.value = 17.5f;
    axis.center = 17.5f;
    axis.range = {-10.0f, 45.0f};
    axis.wrap = false;
    axis.recentering = InputAxisRecenteringSettings::Default;
    return axis;
}

InputAxis OrbitalFollow::makeDefaultRadialAxis()
{
    InputAxis axis;
    axis.value = 1.0f;
    axis.center = 1.0f;
    axis.range = {1.0f, 1.0f};
    axis.wrap = false;
    axis.recentering = InputAxisRecenteringSettings::Default;
    return axis;
}

TrackerTarget OrbitalFollow::trackingTarget() const
{
    TrackerTarget target;
    if (!isAttached())
        return target;

    const auto position = followTargetPosition();
    const auto rotation = followTargetRotation();
    if (!position.has_value() || !rotation.has_value())
        return target;

    target.ref = virtualCamera().effectiveFollowTarget();
    target.position = *position;
    target.rotation = *rotation;
    target.valid = true;
    return target;
}

bool OrbitalFollow::isValid() const
{
    return trackingTarget().valid;
}

math::Vec4 OrbitalFollow::axisValues() const
{
    return {
        horizontalAxis.getClampedValue(),
        verticalAxis.getClampedValue(),
        radialAxis.getClampedValue(),
        verticalAxis.getNormalizedValue(),
    };
}

math::Vec4 OrbitalFollow::cameraPoint(const math::Vec4& values)
{
    math::Vec3 position{};
    float normalizedVertical = 0.0f;

    if (orbitStyle == OrbitStyle::ThreeRing)
    {
        if (orbitCache_.settingsChanged(orbits))
            orbitCache_.update(orbits);

        math::Vec4 value = orbitCache_.splineValue(values.w);
        value = value * values.z;
        const math::Quat horizontalRotation = math::quatFromAxisAngle(
            math::Vec3::up,
            values.x * math::DegToRad);
        const math::Vec3 rotated = math::rotate(
            horizontalRotation,
            math::Vec3{value.x, value.y, value.z});
        position = rotated;
        normalizedVertical = value.w;
    }
    else
    {
        const math::Quat rotation = math::quatFromEuler({
            values.y * math::DegToRad,
            values.x * math::DegToRad,
            0.0f,
        });
        const float distance = std::max(0.0f, radius) * values.z;
        position = math::rotate(rotation, math::Vec3{0.0f, 0.0f, -distance});
        normalizedVertical = values.w * 2.0f - 1.0f;
    }

    if (trackerSettings.bindingMode == BindingMode::LazyFollow)
        position.z = -std::abs(position.z);

    return {position.x, position.y, position.z, normalizedVertical};
}

void OrbitalFollow::updateHorizontalCenter(math::Quat referenceOrientation)
{
    math::Vec3 forward = math::Vec3::forward;

    switch (recenteringTarget)
    {
    case ReferenceFrame::AxisCenter:
        if (trackerSettings.bindingMode == BindingMode::LazyFollow)
            horizontalAxis.center = 0.0f;
        return;
    case ReferenceFrame::ParentObject:
        if (VirtualCamera* parent = virtualCamera().parentVirtualCamera())
            forward = math::rotate(parent->state().finalOrientation(), math::Vec3::forward);
        break;
    case ReferenceFrame::TrackingTarget:
    {
        const auto rotation = followTargetRotation();
        if (rotation.has_value())
            forward = math::rotate(*rotation, math::Vec3::forward);
        break;
    }
    case ReferenceFrame::LookAtTarget:
    {
        const auto rotation = lookAtTargetRotation();
        if (rotation.has_value())
            forward = math::rotate(*rotation, math::Vec3::forward);
        break;
    }
    }

    const math::Vec3 up = math::rotate(referenceOrientation, math::Vec3::up);
    const math::Vec3 projectedForward = math::projectOnPlane(forward, up);
    const math::Vec3 projectedReference =
        math::projectOnPlane(math::rotate(referenceOrientation, math::Vec3::forward), up);
    if (math::almostZero(projectedForward) || math::almostZero(projectedReference))
        return;

    horizontalAxis.center = math::normalizeAngleDegrees(
        -math::signedAngle(projectedForward, projectedReference, up) * math::RadToDeg);
}

void OrbitalFollow::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached())
        return;

    const TrackerTarget target = trackingTarget();
    VirtualCamera& camera = virtualCamera();
    tracker_.initStateInfo(
        target,
        deltaTime,
        camera.previousStateIsValid,
        trackerSettings,
        targetOffset,
        state.referenceUp,
        state);

    if (!target.valid)
        return;

    const float currentTime = camera.currentTime();
    const bool gotInputX = horizontalAxis.trackValueChange(currentTime);
    const bool gotInputY = verticalAxis.trackValueChange(currentTime);
    const bool gotInputZ = radialAxis.trackValueChange(currentTime);

    const math::Vec4 values = axisValues();
    const math::Vec4 point = cameraPoint(values);

    math::Vec3 targetPosition{};
    math::Quat targetOrientation{};
    tracker_.trackTarget(
        target,
        deltaTime,
        camera.previousStateIsValid,
        state.referenceUp,
        {point.x, point.y, point.z},
        trackerSettings,
        targetOffset,
        state,
        camera.followTargetAttachment,
        targetPosition,
        targetOrientation);

    math::Vec3 rotatedOffset = math::rotate(
        targetOrientation,
        math::Vec3{point.x, point.y, point.z});
    state.referenceUp = math::rotate(targetOrientation, math::Vec3::up);
    state.rotationDampingBypass = math::Quat::identity();

    (void)tracker_.getOffsetForMinimumTargetDistance(
        camera.followTargetAttachment,
        targetPosition,
        rotatedOffset,
        math::rotate(state.rawOrientation, math::Vec3::forward),
        state.referenceUp,
        target.position);

    state.rawPosition = targetPosition + rotatedOffset;

    const math::Vec3 targetPoint =
        target.position + math::rotate(target.rotation, targetOffset);
    const math::Vec3 lookAtOffset = state.hasLookAt()
        ? state.referenceLookAt - targetPoint
        : math::Vec3::zero;

    const math::Vec3 currentOffset = rotatedOffset - lookAtOffset;
    const math::Vec4 previousPoint = cameraPoint(previousAxisValues_);
    const math::Vec3 previousOffset = math::rotate(
        targetOrientation,
        math::Vec3{previousPoint.x, previousPoint.y, previousPoint.z}) - lookAtOffset;

    if (deltaTime >= 0.0f
        && camera.previousStateIsValid
        && historyValid_
        && previousOffset.length() > math::Epsilon
        && currentOffset.length() > math::Epsilon)
    {
        state.rotationDampingBypass = math::safeFromToRotation(
            previousOffset,
            currentOffset,
            state.referenceUp);
    }

    math::Vec4 storedValues = values;
    if (trackerSettings.bindingMode == BindingMode::LazyFollow)
    {
        horizontalAxis.setValueAndLastValue(0.0f);
        storedValues.x = 0.0f;
    }
    previousAxisValues_ = storedValues;

    if (horizontalAxis.recentering.enabled)
        updateHorizontalCenter(targetOrientation);

    bool cancelX = gotInputX;
    bool cancelY = gotInputY;
    bool cancelZ = gotInputZ;
    if (horizontalAxis.recentering.time == verticalAxis.recentering.time)
    {
        cancelX = cancelX || gotInputY;
        cancelY = cancelY || gotInputX;
    }
    if (horizontalAxis.recentering.time == radialAxis.recentering.time)
    {
        cancelX = cancelX || gotInputZ;
        cancelZ = cancelZ || gotInputX;
    }
    if (verticalAxis.recentering.time == radialAxis.recentering.time)
    {
        cancelY = cancelY || gotInputZ;
        cancelZ = cancelZ || gotInputY;
    }

    horizontalAxis.updateRecentering(deltaTime, cancelX, currentTime);
    verticalAxis.updateRecentering(deltaTime, cancelY, currentTime);
    radialAxis.updateRecentering(deltaTime, cancelZ, currentTime);
    historyValid_ = true;
}

void OrbitalFollow::inferAxesFromPositionSphere(
    math::Vec3 dir,
    float distance,
    const CameraState& state)
{
    (void)distance;
    const TrackerTarget target = trackingTarget();
    if (!target.valid)
        return;

    const math::Vec3 up = state.referenceUp;
    const math::Quat orientation = tracker_.getReferenceOrientation(
        target,
        trackerSettings.bindingMode,
        targetOffset,
        up,
        state);
    const math::Vec3 localDir = math::rotate(math::inverse(orientation), dir);
    const math::Quat rotation = math::safeFromToRotation(math::Vec3::forward, localDir, up);
    const math::Vec3 euler = math::eulerAngles(rotation);

    verticalAxis.value = verticalAxis.clampValue(
        math::normalizeAngleDegrees(euler.x * math::RadToDeg));
    horizontalAxis.value = horizontalAxis.clampValue(
        math::normalizeAngleDegrees(euler.y * math::RadToDeg));
    previousAxisValues_ = axisValues();
    historyValid_ = true;
}

void OrbitalFollow::inferAxesFromPositionThreeRing(
    math::Vec3 dir,
    float distance,
    const CameraState& state)
{
    const TrackerTarget target = trackingTarget();
    if (!target.valid)
        return;

    if (orbitCache_.settingsChanged(orbits))
        orbitCache_.update(orbits);

    const math::Vec3 up = state.referenceUp;
    const math::Quat orientation = tracker_.getReferenceOrientation(
        target,
        trackerSettings.bindingMode,
        targetOffset,
        up,
        state);

    const math::Vec3 forward = math::projectOnPlane(
        math::rotate(orientation, math::Vec3::forward),
        up);
    const math::Vec3 flatDir = math::projectOnPlane(dir, up);
    if (!math::almostZero(forward) && !math::almostZero(flatDir))
    {
        horizontalAxis.value = horizontalAxis.clampValue(
            math::normalizeAngleDegrees(
                math::signedAngle(forward, flatDir, up) * math::RadToDeg));
    }

    const math::Quat upRotation = math::safeFromToRotation(
        up,
        math::Vec3::up,
        up);
    math::Vec3 localDir = math::rotate(upRotation, dir);
    math::Vec3 flatLocalDir = localDir;
    flatLocalDir.y = 0.0f;
    if (!math::almostZero(flatLocalDir))
    {
        const float angle = math::signedAngle(
            flatLocalDir,
            math::Vec3::forward,
            math::Vec3::up);
        localDir = math::rotate(
            math::quatFromAxisAngle(math::Vec3::up, angle),
            localDir);
    }
    localDir.x = 0.0f;
    localDir = math::normalize(localDir);

    const math::Vec3 cameraOffset = localDir * distance;
    float bestT = 0.5f;
    float bestAngle = std::abs(math::signedAngle(
        cameraOffset,
        math::Vec3{
            orbitCache_.splineValue(0.5f).x,
            orbitCache_.splineValue(0.5f).y,
            orbitCache_.splineValue(0.5f).z},
        math::Vec3::right));

    constexpr float step = 0.1f;
    for (int index = 0; index <= 5; ++index)
    {
        const float offset = static_cast<float>(index) * step;
        const float candidates[2] = {0.5f + offset, 0.5f - offset};
        for (float candidate : candidates)
        {
            const math::Vec4 spline = orbitCache_.splineValue(candidate);
            const float angle = std::abs(math::signedAngle(
                cameraOffset,
                math::Vec3{spline.x, spline.y, spline.z},
                math::Vec3::right));
            if (angle < bestAngle)
            {
                bestAngle = angle;
                bestT = candidate;
            }
        }
    }

    for (int index = 0; index < kSteepestDescentIterations; ++index)
    {
        const math::Vec4 spline = orbitCache_.splineValue(bestT);
        const float angle = std::abs(math::signedAngle(
            cameraOffset,
            math::Vec3{spline.x, spline.y, spline.z},
            math::Vec3::right));
        const math::Vec4 behind = orbitCache_.splineValue(bestT - kSteepestDescentEpsilon);
        const math::Vec4 ahead = orbitCache_.splineValue(bestT + kSteepestDescentEpsilon);
        const float angleBehind = std::abs(math::signedAngle(
            cameraOffset,
            math::Vec3{behind.x, behind.y, behind.z},
            math::Vec3::right));
        const float angleAhead = std::abs(math::signedAngle(
            cameraOffset,
            math::Vec3{ahead.x, ahead.y, ahead.z},
            math::Vec3::right));
        const float slope = (angleAhead - angleBehind)
            / (2.0f * kSteepestDescentEpsilon);
        if (std::abs(slope) < kSteepestDescentEpsilon
            || std::abs(angle) < kSteepestDescentEpsilon)
        {
            break;
        }
        if (std::abs(slope) > math::Epsilon)
            bestT = math::clamp01(bestT - angle / slope);
    }

    const float verticalValue = bestT <= 0.5f
        ? math::lerp(verticalAxis.range.x, verticalAxis.center, mapTo01(bestT, 0.0f, 0.5f))
        : math::lerp(verticalAxis.center, verticalAxis.range.y, mapTo01(bestT, 0.5f, 1.0f));
    verticalAxis.value = verticalAxis.clampValue(verticalValue);
    previousAxisValues_ = axisValues();
    historyValid_ = true;
}

void OrbitalFollow::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    if (!isAttached())
        return;
    if (virtualCamera().isEntityTarget(virtualCamera().effectiveFollowTarget(), target))
        tracker_.onTargetObjectWarped(positionDelta);
}

void OrbitalFollow::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    if (!isAttached())
        return;

    const TrackerTarget target = trackingTarget();
    if (!target.valid)
        return;

    const CameraState oldState = virtualCamera().state();
    CameraState newState = oldState;
    newState.rawPosition = pos;
    newState.rawOrientation = rot;
    newState.positionCorrection = math::Vec3::zero;
    newState.orientationCorrection = math::Quat::identity();
    tracker_.onForceCameraPosition(
        target,
        trackerSettings,
        targetOffset,
        oldState,
        newState);

    const math::Quat referenceOrientation = tracker_.getReferenceOrientation(
        target,
        trackerSettings.bindingMode,
        targetOffset,
        newState.referenceUp,
        newState);
    const math::Quat offsetRotation = trackerSettings.bindingMode == BindingMode::LazyFollow
        ? target.rotation
        : referenceOrientation;
    const math::Vec3 targetPoint =
        target.position + math::rotate(offsetRotation, targetOffset);
    tracker_.setPreviousTargetPosition(targetPoint);

    const math::Vec3 direction = pos - targetPoint;
    const float distance = direction.length();
    if (distance > kMinimumForceDistance)
    {
        const math::Vec3 normalizedDirection = direction / distance;
        if (orbitStyle == OrbitStyle::ThreeRing)
            inferAxesFromPositionThreeRing(normalizedDirection, distance, newState);
        else
            inferAxesFromPositionSphere(normalizedDirection, distance, newState);
        previousAxisValues_ = axisValues();
    }
    else
    {
        previousAxisValues_ = axisValues();
    }
    historyValid_ = true;
}

bool OrbitalFollow::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    (void)worldUp;
    (void)deltaTime;

    horizontalAxis.cancelRecentering();
    verticalAxis.cancelRecentering();
    radialAxis.cancelRecentering();

    if (from == nullptr || virtualCamera().isLive())
        return false;

    const bool inheritsPosition =
        hasBlendHint(virtualCamera().blendHint, BlendHint::InheritPosition)
        || hasBlendHint(virtualCamera().state().blendHint, BlendHint::InheritPosition);
    if (!inheritsPosition)
        return false;

    const CameraState& fromState = from->state();
    const CameraState& currentState = virtualCamera().state();
    const bool alreadyForced =
        math::distance(currentState.finalPosition(), fromState.finalPosition())
            < kMinimumForceDistance
        && math::angle(currentState.finalOrientation(), fromState.finalOrientation())
            < kMinimumForceDistance;
    if (!alreadyForced)
        forceCameraPosition(fromState.finalPosition(), fromState.finalOrientation());
    return true;
}

float OrbitalFollow::maxDampTime() const
{
    return trackerSettings.maxDampTime();
}

void OrbitalFollow::getInputAxes(std::vector<InputAxisDescriptor>& axes)
{
    axes.push_back({"Look Orbit X", &horizontalAxis, InputAxisHint::X});
    axes.push_back({"Look Orbit Y", &verticalAxis, InputAxisHint::Y});
    axes.push_back({"Orbit Scale", &radialAxis, InputAxisHint::Y});
}

float OrbitalFollow::getNormalizedModifierValue()
{
    return cameraPoint(axisValues()).w / std::max(math::Epsilon, radialAxis.value);
}

} // namespace cinerig

