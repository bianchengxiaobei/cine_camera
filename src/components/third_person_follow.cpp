#include <cinerig/components/third_person_follow.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

namespace
{

constexpr float kHeadRadius = 0.15f;

math::Vec3 detachedDamp(
    math::Vec3 initial,
    math::Vec3 dampTime,
    float deltaTime,
    float followTargetAttachment)
{
    const float attachment = math::clamp01(followTargetAttachment);
    const math::Vec3 effectiveDampTime = math::lerp(
        math::max(math::Vec3::one, dampTime),
        dampTime,
        attachment);
    const float effectiveDeltaTime = math::lerp(0.0f, deltaTime, attachment);
    return math::standardDamp(initial, effectiveDampTime, effectiveDeltaTime);
}

} // namespace

bool ThirdPersonFollow::isValid() const
{
    return followTargetPosition().has_value() && followTargetRotation().has_value();
}

void ThirdPersonFollow::validate()
{
    cameraSide = math::clamp(cameraSide, -1.0f, 1.0f);
    damping = math::max(damping, math::Vec3::zero);
    avoidObstacles.cameraRadius = std::max(0.001f, avoidObstacles.cameraRadius);
    avoidObstacles.dampingIntoCollision = std::max(0.0f, avoidObstacles.dampingIntoCollision);
    avoidObstacles.dampingFromCollision = std::max(0.0f, avoidObstacles.dampingFromCollision);
}

math::Quat ThirdPersonFollow::getHeading(math::Quat targetRotation, math::Vec3 up)
{
    const math::Vec3 targetForward = math::rotate(targetRotation, math::Vec3::forward);
    math::Vec3 planeForward = math::cross(
        up,
        math::cross(math::projectOnPlane(targetForward, up), up));
    if (math::almostZero(planeForward))
        planeForward = math::cross(math::rotate(targetRotation, math::Vec3::right), up);
    return math::lookRotation(planeForward, up);
}

void ThirdPersonFollow::getRawRigPositions(
    math::Vec3 root,
    math::Quat targetRotation,
    math::Quat heading,
    math::Vec3& shoulder,
    math::Vec3& hand) const
{
    math::Vec3 offset = shoulderOffset;
    offset.x = -offset.x + 2.0f * offset.x * cameraSide;
    offset.x += dampingCorrection_.x;
    offset.y += dampingCorrection_.y;
    shoulder = root + math::rotate(heading, offset);
    hand = shoulder + math::rotate(targetRotation, math::Vec3{0.0f, verticalArmLength, 0.0f});
}

math::Vec3 ThirdPersonFollow::resolveCollisions(
    math::Vec3 root,
    math::Vec3 tip,
    float deltaTime,
    float cameraRadius,
    float& collisionCorrection)
{
    if (!avoidObstacles.enabled || avoidObstacles.collisionFilter.bits == 0u)
        return tip;

    const math::Vec3 delta = tip - root;
    const float length = delta.length();
    if (length < math::Epsilon)
        return tip;

    const math::Vec3 direction = delta / length;
    const VirtualCamera* camera = virtualCameraOrNull();
    const IPhysicsWorld* physics = camera != nullptr ? camera->physicsWorld() : nullptr;
    if (physics == nullptr)
        return tip;

    const QueryFilter filter{
        avoidObstacles.collisionFilter,
        avoidObstacles.ignoreEntity,
    };
    const auto hit = physics->sphereCast(
        {root, direction},
        std::max(0.0f, cameraRadius),
        length,
        filter);
    if (hit.has_value())
        currentObstacle_ = hit->entity;

    float desiredCorrection = 0.0f;
    if (hit.has_value())
    {
        const math::Vec3 desiredResult = hit->point + hit->normal * cameraRadius;
        desiredCorrection = (desiredResult - tip).length();
    }

    collisionCorrection += deltaTime < 0.0f
        ? desiredCorrection - collisionCorrection
        : math::standardDamp(
            desiredCorrection - collisionCorrection,
            desiredCorrection > collisionCorrection
                ? avoidObstacles.dampingIntoCollision
                : avoidObstacles.dampingFromCollision,
            deltaTime);

    if (collisionCorrection > math::Epsilon)
        return tip - direction * collisionCorrection;
    return tip;
}

void ThirdPersonFollow::mutateCameraState(CameraState& state, float deltaTime)
{
    if (!isAttached())
        return;

    const auto targetPosition = followTargetPosition();
    const auto targetRotation = followTargetRotation();
    if (!targetPosition.has_value() || !targetRotation.has_value())
        return;

    VirtualCamera& camera = virtualCamera();
    if (!camera.previousStateIsValid)
        deltaTime = -1.0f;

    const math::Quat heading = getHeading(*targetRotation, state.referenceUp);
    if (deltaTime < 0.0f)
    {
        dampingCorrection_ = math::Vec3::zero;
        collisionCorrection_ = 0.0f;
    }
    else
    {
        dampingCorrection_ += math::rotate(
            math::inverse(heading),
            previousFollowTargetPosition_ - *targetPosition);
        dampingCorrection_ -= detachedDamp(
            dampingCorrection_,
            damping,
            deltaTime,
            camera.followTargetAttachment);
    }

    previousFollowTargetPosition_ = *targetPosition;

    math::Vec3 shoulder{};
    math::Vec3 hand{};
    getRawRigPositions(*targetPosition, *targetRotation, heading, shoulder, hand);

    const math::Vec3 targetForward = math::rotate(*targetRotation, math::Vec3::forward);
    math::Vec3 cameraPosition =
        hand - targetForward * (cameraDistance - dampingCorrection_.z);

    currentObstacle_ = 0;
    if (avoidObstacles.enabled)
    {
        float dummyCorrection = 0.0f;
        const math::Vec3 collidedHand = resolveCollisions(
            *targetPosition,
            hand,
            -1.0f,
            avoidObstacles.cameraRadius * 1.05f,
            dummyCorrection);
        cameraPosition = resolveCollisions(
            collidedHand,
            cameraPosition,
            deltaTime,
            avoidObstacles.cameraRadius,
            collisionCorrection_);
    }

    state.rawPosition = cameraPosition;
    state.rawOrientation = *targetRotation;

    if (!state.hasLookAt()
        || (cameraPosition - state.referenceLookAt).sqrMagnitude() < kHeadRadius * kHeadRadius)
    {
        state.blendHint |= BlendHint::IgnoreLookAtTarget;
        if (!state.hasLookAt() || math::almostZero(cameraPosition - state.referenceLookAt))
            state.referenceLookAt = *targetPosition + targetForward * 0.01f;
    }
}

void ThirdPersonFollow::getRigPositions(
    math::Vec3& root,
    math::Vec3& shoulder,
    math::Vec3& hand)
{
    root = previousFollowTargetPosition_;
    shoulder = root;
    hand = root;
    if (!isAttached())
        return;

    const auto targetRotation = followTargetRotation();
    if (!targetRotation.has_value())
        return;

    const math::Vec3 up = virtualCamera().state().referenceUp;
    const math::Quat heading = getHeading(*targetRotation, up);
    getRawRigPositions(root, *targetRotation, heading, shoulder, hand);

    if (avoidObstacles.enabled)
    {
        float dummyCorrection = 0.0f;
        hand = resolveCollisions(
            root,
            hand,
            -1.0f,
            avoidObstacles.cameraRadius * 1.05f,
            dummyCorrection);
    }
}

void ThirdPersonFollow::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    if (!isAttached())
        return;
    if (virtualCamera().isEntityTarget(virtualCamera().effectiveFollowTarget(), target))
        previousFollowTargetPosition_ += positionDelta;
}

float ThirdPersonFollow::maxDampTime() const
{
    float result = std::max(damping.x, std::max(damping.y, damping.z));
    if (avoidObstacles.enabled)
    {
        result = std::max(
            result,
            std::max(
                avoidObstacles.dampingIntoCollision,
                avoidObstacles.dampingFromCollision));
    }
    return result;
}

EntityId ThirdPersonFollow::currentObstacle() const noexcept
{
    return currentObstacle_;
}

float ThirdPersonFollow::getNormalizedModifierValue()
{
    const auto rotation = followTargetRotation();
    if (!rotation.has_value())
        return 0.0f;

    const math::Vec3 up = isAttached()
        ? virtualCamera().state().referenceUp
        : math::Vec3::up;
    const math::Vec3 targetUp = math::rotate(*rotation, math::Vec3::up);
    const math::Vec3 targetRight = math::rotate(*rotation, math::Vec3::right);
    const float angle = math::signedAngle(targetUp, up, targetRight);
    return math::clamp(angle, -math::Pi * 0.5f, math::Pi * 0.5f) / (-math::Pi * 0.5f);
}

} // namespace cinerig
