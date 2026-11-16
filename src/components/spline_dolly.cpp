#include <cinerig/components/spline_dolly.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig
{

void SplineDolly::reset()
{
    spline = {nullptr, 0.0f, PathIndexUnit::Normalized};
    splineRoll = nullptr;
    splineOffset = {};
    cameraRotation = RotationMode::Default;
    damping = {};
    automaticDolly = {};
    previousSplinePosition_ = 0.0f;
    previousPosition_ = {};
    previousRotation_ = {};
}

void SplineDolly::validate()
{
    damping.position.x = std::clamp(damping.position.x, 0.0f, 20.0f);
    damping.position.y = std::clamp(damping.position.y, 0.0f, 20.0f);
    damping.position.z = std::clamp(damping.position.z, 0.0f, 20.0f);
    damping.angular = std::clamp(damping.angular, 0.0f, 20.0f);
    automaticDolly.validate();
}

bool SplineDolly::isValid() const
{
    return isAttached() && spline.spline != nullptr;
}

float SplineDolly::maxDampTime() const
{
    if (!damping.enabled)
        return 0.0f;
    return std::max(
        std::max(damping.position.x, damping.position.y),
        std::max(damping.position.z, damping.angular));
}

math::Quat SplineDolly::rotationAtSplinePoint(
    math::Quat splineRotation,
    math::Vec3 up,
    math::Quat defaultOrientation,
    bool& isDefault) const
{
    isDefault = false;
    switch (cameraRotation)
    {
    case RotationMode::Default:
        break;
    case RotationMode::Spline:
        return splineRotation;
    case RotationMode::SplineNoRoll:
        return math::lookRotation(
            math::rotate(splineRotation, math::Vec3::forward),
            up);
    case RotationMode::FollowTarget:
        if (const auto rotation = followTargetRotation())
            return *rotation;
        break;
    case RotationMode::FollowTargetNoRoll:
        if (const auto rotation = followTargetRotation())
        {
            return math::lookRotation(
                math::rotate(*rotation, math::Vec3::forward),
                up);
        }
        break;
    }

    isDefault = true;
    return math::lookRotation(
        math::rotate(defaultOrientation, math::Vec3::forward),
        up);
}

void SplineDolly::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached() || spline.spline == nullptr)
        return;

    VirtualCamera& camera = virtualCamera();
    const ISpline& track = *spline.spline;
    float maxPosition = 0.0f;
    float splinePosition = spline.standardizedPosition(&maxPosition);
    const math::Quat incomingOrientation = state.rawOrientation;

    if (deltaTime < 0.0f || !camera.previousStateIsValid)
    {
        previousSplinePosition_ = splinePosition;
        previousPosition_ = state.rawPosition;
        previousRotation_ = state.rawOrientation;
    }

    if (automaticDolly.enabled && automaticDolly.method)
    {
        splinePosition = automaticDolly.getSplinePosition(
            track,
            followTargetPosition(),
            splinePosition,
            spline.units,
            deltaTime);
    }

    const bool useDamping =
        damping.enabled && deltaTime >= 0.0f && camera.previousStateIsValid;
    if (useDamping)
    {
        float previous = previousSplinePosition_;
        if (track.closed()
            && std::abs(splinePosition - previous) > maxPosition * 0.5f)
        {
            previous += splinePosition > previous ? maxPosition : -maxPosition;
        }
        splinePosition = previous + math::standardDamp(
            splinePosition - previous,
            damping.position.z,
            deltaTime);
    }
    previousSplinePosition_ = spline.position = splinePosition;

    const float normalized = spline.normalizedPosition();
    math::Quat splineRotation = math::lookRotation(
        track.evaluateTangent(normalized),
        track.evaluateUp(normalized));
    if (splineRoll != nullptr)
        splineRotation = splineRoll->apply(splineRotation, normalized);
    const math::Vec3 offsetX =
        math::rotate(splineRotation, math::Vec3::right);
    const math::Vec3 offsetY =
        math::rotate(splineRotation, math::Vec3::up);
    const math::Vec3 offsetZ =
        math::rotate(splineRotation, math::Vec3::forward);

    math::Vec3 newPosition = track.evaluatePosition(normalized);
    newPosition += offsetX * splineOffset.x;
    newPosition += offsetY * splineOffset.y;
    newPosition += offsetZ * splineOffset.z;

    if (useDamping)
    {
        const math::Vec3 delta = previousPosition_ - newPosition;
        const math::Vec3 deltaY = offsetY * math::dot(delta, offsetY);
        const math::Vec3 deltaXZ = delta - deltaY;
        const math::Vec3 dampedXZ = math::standardDamp(
            deltaXZ,
            damping.position.x,
            deltaTime);
        const math::Vec3 dampedY = math::standardDamp(
            deltaY,
            damping.position.y,
            deltaTime);
        newPosition = previousPosition_ - (dampedXZ + dampedY);
    }
    state.rawPosition = previousPosition_ = newPosition;

    bool isDefault = false;
    math::Quat newRotation = rotationAtSplinePoint(
        splineRotation,
        state.referenceUp,
        incomingOrientation,
        isDefault);
    if (useDamping)
    {
        const float t = camera.detachedFollowTargetDamp(
            1.0f,
            damping.angular,
            deltaTime);
        newRotation = math::slerp(previousRotation_, newRotation, t);
    }
    previousRotation_ = newRotation;
    state.rawOrientation = newRotation;
    if (!isDefault)
        state.referenceUp = math::rotate(newRotation, math::Vec3::up);
}

} // namespace cinerig
