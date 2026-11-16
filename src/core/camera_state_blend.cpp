#include <cinerig/core/camera_state_blend.hpp>

#include <cinerig/math/geometry.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace cinerig
{

namespace
{

bool isNoPoint(math::Vec3 point) noexcept
{
    return std::isnan(point.x) || std::isnan(point.y) || std::isnan(point.z);
}

math::Vec3 applyPositionBlendHint(
    math::Vec3 positionA,
    BlendHint hintA,
    math::Vec3 positionB,
    BlendHint hintB,
    math::Vec3 original,
    math::Vec3 blended)
{
    if (!hasBlendHint(hintA | hintB, BlendHint::NoPosition))
        return blended;
    if (hasBlendHint(hintA & hintB, BlendHint::NoPosition))
        return original;
    if (hasBlendHint(hintA, BlendHint::NoPosition))
        return positionB;
    return positionA;
}

math::Quat applyRotationBlendHint(
    math::Quat rotationA,
    BlendHint hintA,
    math::Quat rotationB,
    BlendHint hintB,
    math::Quat original,
    math::Quat blended)
{
    if (!hasBlendHint(hintA | hintB, BlendHint::NoOrientation))
        return blended;
    if (hasBlendHint(hintA & hintB, BlendHint::NoOrientation))
        return original;
    if (hasBlendHint(hintA, BlendHint::NoOrientation))
        return rotationB;
    return rotationA;
}

float interpolateFov(float fovA, float fovB, float distanceA, float distanceB, float t)
{
    const float heightA = distanceA * 2.0f * std::tan(fovA * math::DegToRad * 0.5f);
    const float heightB = distanceB * 2.0f * std::tan(fovB * math::DegToRad * 0.5f);
    const float height = math::lerp(heightA, heightB, t);
    const float distance = math::lerp(distanceA, distanceB, t);
    float fov = 179.0f;
    if (distance > math::Epsilon)
        fov = 2.0f * std::atan(height / (2.0f * distance)) * math::RadToDeg;
    return math::clamp(fov, std::min(fovA, fovB), std::max(fovA, fovB));
}

math::Vec3 interpolatePosition(
    math::Vec3 positionA,
    math::Vec3 pivotA,
    math::Vec3 positionB,
    math::Vec3 pivotB,
    float t,
    BlendHint hints,
    math::Vec3 up)
{
    if (!isNoPoint(pivotA) && !isNoPoint(pivotB))
    {
        if (hasBlendHint(hints, BlendHint::CylindricalPositionBlend))
        {
            const math::Vec3 a = math::projectOnPlane(positionA - pivotA, up);
            const math::Vec3 b = math::projectOnPlane(positionB - pivotB, up);
            const math::Vec3 c = math::slerp(a, b, t);
            positionA = (positionA - a) + c;
            positionB = (positionB - b) + c;
        }
        else if (hasBlendHint(hints, BlendHint::SphericalPositionBlend))
        {
            const math::Vec3 c = math::slerp(positionA - pivotA, positionB - pivotB, t);
            positionA = pivotA + c;
            positionB = pivotB + c;
        }
    }
    return math::lerp(positionA, positionB, t);
}

void lerpLensFields(LensSettings& destination, const LensSettings& source, float t)
{
    destination.farClipPlane = math::lerp(destination.farClipPlane, source.farClipPlane, t);
    destination.nearClipPlane = math::lerp(destination.nearClipPlane, source.nearClipPlane, t);
    destination.fieldOfView = math::lerp(destination.fieldOfView, source.fieldOfView, t);
    destination.orthographicSize = math::lerp(destination.orthographicSize, source.orthographicSize, t);
    destination.dutch = math::lerp(destination.dutch, source.dutch, t);
    destination.physical.sensorSize =
        math::lerp(destination.physical.sensorSize, source.physical.sensorSize, t);
    destination.physical.lensShift =
        math::lerp(destination.physical.lensShift, source.physical.lensShift, t);
    destination.physical.focusDistance =
        math::lerp(destination.physical.focusDistance, source.physical.focusDistance, t);
    destination.physical.iso = static_cast<int>(std::lround(
        math::lerp(static_cast<float>(destination.physical.iso),
                   static_cast<float>(source.physical.iso), t)));
    destination.physical.shutterSpeed =
        math::lerp(destination.physical.shutterSpeed, source.physical.shutterSpeed, t);
    destination.physical.aperture =
        math::lerp(destination.physical.aperture, source.physical.aperture, t);
    destination.physical.bladeCount = static_cast<int>(std::lround(
        math::lerp(static_cast<float>(destination.physical.bladeCount),
                   static_cast<float>(source.physical.bladeCount), t)));
    destination.physical.curvature =
        math::lerp(destination.physical.curvature, source.physical.curvature, t);
    destination.physical.barrelClipping =
        math::lerp(destination.physical.barrelClipping, source.physical.barrelClipping, t);
    destination.physical.anamorphism =
        math::lerp(destination.physical.anamorphism, source.physical.anamorphism, t);
}

}

bool CameraState::isTargetOffscreen() const
{
    if (!hasLookAt())
        return false;

    math::Vec3 direction = referenceLookAt - correctedPosition();
    direction = math::rotate(math::inverse(correctedOrientation()), direction);

    if (lens.orthographic)
    {
        if (std::abs(direction.y) > lens.orthographicSize)
            return true;
        if (std::abs(direction.x) > lens.orthographicSize * lens.aspect)
            return true;
        return false;
    }

    float fov = lens.fieldOfView * math::DegToRad * 0.5f;
    float angle = math::angle(
        math::projectOnPlane(direction, math::Vec3::right),
        math::Vec3::forward);
    if (angle > fov)
        return true;

    fov = std::atan(std::tan(fov) * lens.aspect);
    angle = math::angle(
        math::projectOnPlane(direction, math::Vec3::up),
        math::Vec3::forward);
    return angle > fov;
}

LensSettings LensSettings::lerp(const LensSettings& a, const LensSettings& b, float t)
{
    t = math::clamp01(t);
    if (t < 0.5f)
    {
        LensSettings result = a;
        lerpLensFields(result, b, t);
        return result;
    }

    LensSettings result = b;
    lerpLensFields(result, a, 1.0f - t);
    return result;
}

CameraState lerp(const CameraState& a, const CameraState& b, float t)
{
    t = math::clamp01(t);
    float adjustedT = t;

    const BlendHint hintA = a.blendHint;
    const BlendHint hintB = b.blendHint;
    const BlendHint eitherHint = hintA | hintB;
    const BlendHint bothHint = hintA & hintB;

    CameraState result = CameraState::makeDefault();

    if (hasBlendHint(bothHint, BlendHint::NoPosition))
        result.blendHint |= BlendHint::NoPosition;
    if (hasBlendHint(bothHint, BlendHint::NoOrientation))
        result.blendHint |= BlendHint::NoOrientation;
    if (hasBlendHint(bothHint, BlendHint::NoLens))
        result.blendHint |= BlendHint::NoLens;
    if (hasBlendHint(eitherHint, BlendHint::SphericalPositionBlend))
        result.blendHint |= BlendHint::SphericalPositionBlend;
    if (hasBlendHint(eitherHint, BlendHint::CylindricalPositionBlend))
        result.blendHint |= BlendHint::CylindricalPositionBlend;
    if (hasBlendHint(eitherHint, BlendHint::FreezeWhenBlendingOut))
        result.blendHint |= BlendHint::FreezeWhenBlendingOut;

    if (!hasBlendHint(eitherHint, BlendHint::NoLens))
    {
        result.lens = LensSettings::lerp(a.lens, b.lens, t);
    }
    else if (!hasBlendHint(bothHint, BlendHint::NoLens))
    {
        if (hasBlendHint(hintA, BlendHint::NoLens))
            result.lens = b.lens;
        else
            result.lens = a.lens;
    }

    result.referenceUp = math::slerp(a.referenceUp, b.referenceUp, t);
    result.shotQuality = math::lerp(a.shotQuality, b.shotQuality, t);

    result.positionCorrection = applyPositionBlendHint(
        a.positionCorrection,
        hintA,
        b.positionCorrection,
        hintB,
        result.positionCorrection,
        math::lerp(a.positionCorrection, b.positionCorrection, t));

    result.orientationCorrection = applyRotationBlendHint(
        a.orientationCorrection,
        hintA,
        b.orientationCorrection,
        hintB,
        result.orientationCorrection,
        math::slerp(a.orientationCorrection, b.orientationCorrection, t));

    if (!a.hasLookAt() || !b.hasLookAt() || hasBlendHint(eitherHint, BlendHint::IgnoreLookAtTarget))
    {
        result.referenceLookAt = CameraState::kNoPoint;
    }
    else
    {
        const float fovA = a.lens.fieldOfView;
        const float fovB = b.lens.fieldOfView;
        if (!hasBlendHint(eitherHint, BlendHint::NoLens)
            && !result.lens.orthographic
            && !math::approximately(fovA, fovB))
        {
            LensSettings lens = result.lens;
            lens.fieldOfView = interpolateFov(
                fovA,
                fovB,
                std::max((a.referenceLookAt - a.correctedPosition()).length(), a.lens.nearClipPlane),
                std::max((b.referenceLookAt - b.correctedPosition()).length(), b.lens.nearClipPlane),
                t);
            result.lens = lens;
            adjustedT = std::abs((lens.fieldOfView - fovA) / (fovB - fovA));
        }

        result.referenceLookAt = math::lerp(a.referenceLookAt, b.referenceLookAt, adjustedT);
    }

    result.rawPosition = applyPositionBlendHint(
        a.rawPosition,
        hintA,
        b.rawPosition,
        hintB,
        result.rawPosition,
        interpolatePosition(
            a.rawPosition,
            a.referenceLookAt,
            b.rawPosition,
            b.referenceLookAt,
            t,
            result.blendHint,
            result.referenceUp));

    if (result.hasLookAt() && hasBlendHint(eitherHint, BlendHint::ScreenSpaceAimWhenTargetsDiffer))
    {
        result.referenceLookAt = result.rawPosition + math::slerp(
            a.referenceLookAt - result.rawPosition,
            b.referenceLookAt - result.rawPosition,
            adjustedT);
    }

    math::Quat newOrientation = result.rawOrientation;
    if (!hasBlendHint(eitherHint, BlendHint::NoOrientation))
    {
        math::Vec3 targetDirection{};
        if (result.hasLookAt())
        {
            const float orientationAngle = math::angle(a.rawOrientation, b.rawOrientation);
            if (orientationAngle > math::Epsilon)
                targetDirection = result.referenceLookAt - result.correctedPosition();
        }

        if (math::almostZero(targetDirection) || hasBlendHint(eitherHint, BlendHint::IgnoreLookAtTarget))
        {
            newOrientation = math::slerp(a.rawOrientation, b.rawOrientation, t);
        }
        else
        {
            math::Vec3 up = result.referenceUp;
            targetDirection = math::normalize(targetDirection);
            if (math::almostZero(math::cross(targetDirection, up)))
            {
                newOrientation = math::slerp(a.rawOrientation, b.rawOrientation, t);
                up = math::rotate(newOrientation, math::Vec3::up);
            }

            newOrientation = math::lookRotation(targetDirection, up);
            const math::Vec2 deltaA = -math::getCameraRotationToTarget(
                a.rawOrientation,
                a.referenceLookAt - a.correctedPosition(),
                up);
            const math::Vec2 deltaB = -math::getCameraRotationToTarget(
                b.rawOrientation,
                b.referenceLookAt - b.correctedPosition(),
                up);
            newOrientation = math::applyCameraRotation(
                newOrientation,
                math::lerp(deltaA, deltaB, adjustedT),
                up);
        }
    }

    result.rawOrientation = applyRotationBlendHint(
        a.rawOrientation,
        hintA,
        b.rawOrientation,
        hintB,
        result.rawOrientation,
        newOrientation);

    for (std::size_t i = 0; i < a.numCustomBlendables(); ++i)
    {
        CustomBlendable blendable = a.customBlendable(i);
        blendable.weight *= (1.0f - t);
        if (blendable.weight > 0.0f)
            result.addCustomBlendable(std::move(blendable));
    }
    for (std::size_t i = 0; i < b.numCustomBlendables(); ++i)
    {
        CustomBlendable blendable = b.customBlendable(i);
        blendable.weight *= t;
        if (blendable.weight > 0.0f)
            result.addCustomBlendable(std::move(blendable));
    }

    return result;
}

CameraState CameraState::lerp(const CameraState& a, const CameraState& b, float t)
{
    return cinerig::lerp(a, b, t);
}

}
