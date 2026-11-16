#include <cinerig/targets/group_framing.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/targets/target_group.hpp>

namespace cinerig::targets
{

namespace
{

bool isOrthographic(const LensSettings& lens)
{
    return lens.modeOverride == LensSettings::OverrideMode::Orthographic
        || (lens.modeOverride == LensSettings::OverrideMode::None && lens.orthographic);
}

math::Bounds toDepthBounds(math::Bounds bounds)
{
    bounds.center.z = -bounds.center.z;
    return bounds;
}

float safeAspect(float aspect)
{
    return aspect > math::Epsilon ? aspect : 1.0f;
}

} // namespace

void GroupFraming::validate()
{
    framingSize = std::max(kMinimumGroupSize, framingSize);
    damping = std::max(0.0f, damping);
    dollyRange.y = std::max(dollyRange.x, dollyRange.y);
    fovRange.y = math::clamp(fovRange.y, 1.0f, 179.0f);
    fovRange.x = math::clamp(fovRange.x, 1.0f, fovRange.y);
    orthoSizeRange.x = std::max(0.01f, orthoSizeRange.x);
    orthoSizeRange.y = std::max(orthoSizeRange.x, orthoSizeRange.y);
}

void GroupFraming::reset(VirtualCamera& vcam)
{
    caches_.erase(&vcam);
}

float GroupFraming::maxDampTime() const
{
    return damping;
}

void GroupFraming::prePipelineMutateCameraStateCallback(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime)
{
    (void)state;
    (void)deltaTime;
    caches_[&vcam].bodyCallsThisFrame = 0;
}

void GroupFraming::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    CameraCache& cache = caches_[&vcam];
    const CameraComponent* body = vcam.component(Stage::Body);
    const bool applyOnBody = body != nullptr && body->bodyAppliesAfterAim();
    const Stage framingStage = applyOnBody ? Stage::Body : Stage::Aim;
    if (stage != framingStage)
        return;

    if (applyOnBody && cache.bodyCallsThisFrame++ == 0)
        return;

    const TargetGroup* group = dynamic_cast<const TargetGroup*>(
        vcam.lookAtTargetAsGroup());
    if (group == nullptr)
        group = dynamic_cast<const TargetGroup*>(vcam.followTargetAsGroup());
    if (group == nullptr || group->isEmpty())
        return;

    if (!vcam.previousStateIsValid)
        cache.reset();

    if (isOrthographic(state.lens))
        orthoFraming(vcam, *group, cache, state, deltaTime);
    else
        perspectiveFraming(vcam, *group, cache, state, deltaTime);
}

float GroupFraming::frameHeight(math::Vec2 boundsSize, float aspect) const
{
    const float safe = safeAspect(aspect);
    float height = 0.0f;
    switch (framingMode)
    {
    case FramingMode::Horizontal:
        height = std::max(math::Epsilon, boundsSize.x) / safe;
        break;
    case FramingMode::Vertical:
        height = std::max(math::Epsilon, boundsSize.y);
        break;
    case FramingMode::HorizontalAndVertical:
    default:
        height = std::max(
            std::max(math::Epsilon, boundsSize.x) / safe,
            std::max(math::Epsilon, boundsSize.y));
        break;
    }
    return std::max(height, kMinimumGroupSize);
}

void GroupFraming::perspectiveFraming(
    VirtualCamera& vcam,
    const TargetGroup& group,
    CameraCache& cache,
    CameraState& state,
    float deltaTime)
{
    const float dampTime = vcam.previousStateIsValid && deltaTime >= 0.0f
        ? damping
        : 0.0f;
    const math::Vec3 statePos = state.correctedPosition();
    const math::Quat stateRot = state.correctedOrientation();

    math::Vec3 camPos = statePos;
    math::Quat camRot = stateRot;
    const math::Vec3 up = math::rotate(camRot, math::Vec3::up);
    float fov = state.lens.fieldOfView;

    const bool canDollyOut = sizeAdjustment != SizeAdjustmentMode::ZoomOnly;
    const math::Bounds naiveBounds = toDepthBounds(group.viewSpaceBoundingBox(
        camPos,
        camRot,
        canDollyOut));

    const bool moveCamera = lateralAdjustment == LateralAdjustmentMode::ChangePosition;
    if (!moveCamera)
    {
        const math::Vec3 forward = math::rotate(
            camRot,
            math::Vec3{
                naiveBounds.center.x,
                naiveBounds.center.y,
                -naiveBounds.center.z});
        if (!math::almostZero(math::cross(forward, up)))
            camRot = math::lookRotation(forward, up);
    }

    constexpr float slush = 5.0f;
    const math::Vec2 effectiveDollyRange = canDollyOut
        ? dollyRange
        : math::Vec2::zero;
    float dollyAmount = math::clamp(
        std::min(0.0f, naiveBounds.center.z) - naiveBounds.extents.z - slush,
        effectiveDollyRange.x,
        effectiveDollyRange.y);
    camPos += math::rotate(camRot, math::Vec3{0.0f, 0.0f, -dollyAmount});

    computeCameraViewGroupBounds(group, camPos, camRot, moveCamera);
    adjustSize(
        group,
        safeAspect(state.lens.aspect),
        camPos,
        camRot,
        fov,
        dollyAmount);

    LensSettings lens = state.lens;
    const float deltaFov = fov - lens.fieldOfView;
    cache.fovAdjustment += vcam.detachedFollowTargetDamp(
        deltaFov - cache.fovAdjustment,
        dampTime,
        deltaTime);
    lens.fieldOfView += cache.fovAdjustment;
    state.lens = lens;

    const math::Vec2 deltaRot = math::getCameraRotationToTarget(
        stateRot,
        math::rotate(camRot, math::Vec3::forward),
        up);
    cache.rotationAdjustment.x += vcam.detachedFollowTargetDamp(
        deltaRot.x - cache.rotationAdjustment.x,
        dampTime,
        deltaTime);
    cache.rotationAdjustment.y += vcam.detachedFollowTargetDamp(
        deltaRot.y - cache.rotationAdjustment.y,
        dampTime,
        deltaTime);
    state.orientationCorrection = state.orientationCorrection
        * math::applyCameraRotation(
            math::Quat::identity(),
            cache.rotationAdjustment,
            up);
    const math::Quat correctedRot = state.correctedOrientation();

    const math::Vec3 deltaPos = math::rotate(
        math::inverse(correctedRot),
        camPos - statePos);
    cache.positionAdjustment += vcam.detachedFollowTargetDamp(
        deltaPos - cache.positionAdjustment,
        dampTime,
        deltaTime);
    state.positionCorrection += math::rotate(
        correctedRot,
        cache.positionAdjustment);

    if (std::abs(centerOffset.x) > 0.01f || std::abs(centerOffset.y) > 0.01f)
    {
        const float halfFov = 0.5f * state.lens.fieldOfView * math::DegToRad;
        if (moveCamera)
        {
            const float d = groupBounds.center.z - groupBounds.extents.z;
            state.positionCorrection -= math::rotate(
                correctedRot,
                math::Vec3{
                    centerOffset.x * std::tan(halfFov) * safeAspect(state.lens.aspect) * d,
                    centerOffset.y * std::tan(halfFov) * d,
                    0.0f});
        }
        else
        {
            const math::Vec2 rotation{
                -centerOffset.y * halfFov,
                centerOffset.x * halfFov / safeAspect(state.lens.aspect),
            };
            state.orientationCorrection = state.orientationCorrection
                * math::applyCameraRotation(
                    math::Quat::identity(),
                    rotation,
                    state.referenceUp);
        }
    }
}

void GroupFraming::orthoFraming(
    VirtualCamera& vcam,
    const TargetGroup& group,
    CameraCache& cache,
    CameraState& state,
    float deltaTime)
{
    const float dampTime = vcam.previousStateIsValid && deltaTime >= 0.0f
        ? damping
        : 0.0f;
    const math::Vec3 statePos = state.correctedPosition();
    const math::Quat stateRot = state.correctedOrientation();
    const float aspect = safeAspect(state.lens.aspect);

    groupBoundsPosition = statePos;
    groupBoundsRotation = stateRot;
    groupBounds = toDepthBounds(group.viewSpaceBoundingBox(
        statePos,
        stateRot,
        true));

    math::Vec3 camPos = groupBounds.center;
    camPos.z = std::min(0.0f, camPos.z - groupBounds.extents.z);

    LensSettings lens = state.lens;
    const math::Vec2 scaledSize = math::Vec2{groupBounds.size().x, groupBounds.size().y}
        / std::max(framingSize, kMinimumGroupSize);
    float targetHeight = frameHeight(scaledSize, aspect) * 0.5f;
    targetHeight = math::clamp(
        targetHeight,
        orthoSizeRange.x,
        orthoSizeRange.y);

    const float deltaFov = targetHeight - lens.orthographicSize;
    cache.fovAdjustment += vcam.detachedFollowTargetDamp(
        deltaFov - cache.fovAdjustment,
        dampTime,
        deltaTime);
    lens.orthographicSize += cache.fovAdjustment;

    camPos.x -= centerOffset.x * lens.orthographicSize / aspect;
    camPos.y -= centerOffset.y * lens.orthographicSize;
    const math::Vec3 desiredLocal{camPos.x, camPos.y, -camPos.z};
    cache.positionAdjustment += vcam.detachedFollowTargetDamp(
        desiredLocal - cache.positionAdjustment,
        dampTime,
        deltaTime);
    state.positionCorrection += math::rotate(stateRot, cache.positionAdjustment);
    state.lens = lens;
}

void GroupFraming::adjustSize(
    const TargetGroup& group,
    float aspect,
    math::Vec3& camPos,
    math::Quat& camRot,
    float& fov,
    float& dollyAmount)
{
    if (sizeAdjustment != SizeAdjustmentMode::ZoomOnly)
    {
        const math::Vec2 scaledSize = math::Vec2{groupBounds.size().x, groupBounds.size().y}
        / std::max(framingSize, kMinimumGroupSize);
        const float height = frameHeight(scaledSize, aspect);
        const float currentDistance = groupBounds.center.z - groupBounds.extents.z;
        const float desiredDistance =
            height / (2.0f * std::tan(fov * math::DegToRad * 0.5f));

        float dolly = currentDistance - desiredDistance;
        dolly = math::clamp(
            dolly + dollyAmount,
            dollyRange.x,
            dollyRange.y) - dollyAmount;
        dollyAmount += dolly;

        camPos += math::rotate(camRot, math::Vec3{0.0f, 0.0f, -dolly});
        computeCameraViewGroupBounds(group, camPos, camRot, true);
    }

    if (sizeAdjustment != SizeAdjustmentMode::DollyOnly)
    {
        const math::Vec2 scaledSize = math::Vec2{groupBounds.size().x, groupBounds.size().y}
        / std::max(framingSize, kMinimumGroupSize);
        const float height = frameHeight(scaledSize, aspect);
        const float distance = groupBounds.center.z - groupBounds.extents.z;
        if (distance > math::Epsilon)
            fov = 2.0f * std::atan(height / (2.0f * distance)) * math::RadToDeg;
        fov = math::clamp(fov, fovRange.x, fovRange.y);
    }
}

void GroupFraming::computeCameraViewGroupBounds(
    const TargetGroup& group,
    math::Vec3& camPos,
    math::Quat& camRot,
    bool moveCamera)
{
    groupBoundsPosition = camPos;
    groupBoundsRotation = camRot;

    if (moveCamera)
    {
        const math::Bounds viewBounds = toDepthBounds(group.viewSpaceBoundingBox(
            camPos,
            camRot,
            false));
        camPos += math::rotate(
            camRot,
            math::Vec3{viewBounds.center.x, viewBounds.center.y, 0.0f});
        groupBoundsPosition = camPos;
    }

    math::Vec2 minAngles{};
    math::Vec2 maxAngles{};
    math::Vec2 zRange{};
    group.viewSpaceAngularBounds(camPos, camRot, minAngles, maxAngles, zRange);
    const math::Vec2 shift = (minAngles + maxAngles) * (0.5f * math::DegToRad);
    const math::Quat adjustment = math::applyCameraRotation(
        math::Quat::identity(),
        shift,
        math::Vec3::up);

    if (moveCamera)
    {
        const math::Vec3 direction = math::rotate(adjustment, math::Vec3::forward);
        if (std::abs(direction.z) > math::Epsilon)
        {
            const float t = -zRange.x / direction.z;
            camPos += math::rotate(
                camRot,
                math::Vec3{direction.x * t, direction.y * t, 0.0f});
        }
        group.viewSpaceAngularBounds(camPos, camRot, minAngles, maxAngles, zRange);
    }
    else
    {
        camRot = camRot * adjustment;
        minAngles -= shift;
        maxAngles -= shift;
    }

    const float limit = 89.5f * math::DegToRad;
    const float halfVertical = std::min(
        std::max(std::abs(minAngles.x), std::abs(maxAngles.x)) * math::DegToRad,
        limit);
    const float halfHorizontal = std::min(
        std::max(std::abs(minAngles.y), std::abs(maxAngles.y)) * math::DegToRad,
        limit);

    groupBounds.center = {0.0f, 0.0f, (zRange.x + zRange.y) * 0.5f};
    groupBounds.extents = {
        std::tan(halfHorizontal) * zRange.x,
        std::tan(halfVertical) * zRange.x,
        (zRange.y - zRange.x) * 0.5f,
    };
    groupBoundsPosition = camPos;
    groupBoundsRotation = camRot;
}

} // namespace cinerig::targets

