#pragma once

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/modifier_interfaces.hpp>
#include <cinerig/engine/physics_world.hpp>

namespace cinerig
{

class ThirdPersonFollow
    : public CameraComponent
    , public IModifierValueSource
    , public IModifiablePositionDamping
    , public IModifiableDistance
{
public:
    struct ObstacleSettings
    {
        bool enabled = false;
        LayerMask collisionFilter{1u};
        EntityId ignoreEntity = 0;
        float cameraRadius = 0.2f;
        float dampingIntoCollision = 0.0f;
        float dampingFromCollision = 0.5f;
    };

    math::Vec3 damping{0.1f, 0.5f, 0.3f};
    math::Vec3 shoulderOffset{0.5f, -0.4f, 0.0f};
    float verticalArmLength = 0.4f;
    float cameraSide = 1.0f;
    float cameraDistance = 2.0f;
    ObstacleSettings avoidObstacles{};

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    float maxDampTime() const override;

    void validate();
    static math::Quat getHeading(math::Quat targetRotation, math::Vec3 up);
    void getRigPositions(
        math::Vec3& root,
        math::Vec3& shoulder,
        math::Vec3& hand);
    EntityId currentObstacle() const noexcept;

    float getNormalizedModifierValue() override;
    math::Vec3 getPositionDamping() const override { return damping; }
    void setPositionDamping(math::Vec3 value) override { damping = value; }
    float getDistance() const override { return cameraDistance; }
    void setDistance(float value) override { cameraDistance = value; }

private:
    math::Vec3 resolveCollisions(
        math::Vec3 root,
        math::Vec3 tip,
        float deltaTime,
        float cameraRadius,
        float& collisionCorrection);
    void getRawRigPositions(
        math::Vec3 root,
        math::Quat targetRotation,
        math::Quat heading,
        math::Vec3& shoulder,
        math::Vec3& hand) const;

    math::Vec3 previousFollowTargetPosition_{};
    math::Vec3 dampingCorrection_{};
    float collisionCorrection_ = 0.0f;
    EntityId currentObstacle_ = 0;
};

} // namespace cinerig
