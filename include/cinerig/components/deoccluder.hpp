#pragma once

#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/engine/physics_world.hpp>

namespace cinerig
{

class Deoccluder : public CameraExtension
{
public:
    struct FollowTargetSettings
    {
        bool enabled = false;
        float yOffset = 0.0f;
    };

    enum class ResolutionStrategy
    {
        PullCameraForward,
        PreserveCameraHeight,
        PreserveCameraDistance,
    };

    struct ObstacleAvoidance
    {
        bool enabled = true;
        float distanceLimit = 0.0f;
        float minimumOcclusionTime = 0.0f;
        float cameraRadius = 0.4f;
        FollowTargetSettings useFollowTarget{};
        ResolutionStrategy strategy = ResolutionStrategy::PullCameraForward;
        int maximumEffort = 4;
        float smoothingTime = 0.0f;
        float damping = 0.4f;
        float dampingWhenOccluded = 0.2f;
    };

    struct QualityEvaluation
    {
        bool enabled = false;
        float optimalDistance = 10.0f;
        float nearLimit = 5.0f;
        float farLimit = 30.0f;
        float maxQualityBoost = 0.2f;
    };

    LayerMask collideAgainst{1u};
    EntityId ignoreEntity = 0;
    LayerMask transparentLayers{0u};
    float minimumDistanceFromTarget = 0.3f;
    ObstacleAvoidance avoidObstacles{};
    QualityEvaluation qualityEvaluation{};

    void reset();
    void validate();

    bool isTargetObscured(CameraId cameraId) const;
    bool cameraWasDisplaced(CameraId cameraId) const;
    float cameraDisplacementDistance(CameraId cameraId) const;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
    float maxDampTime() const override;

private:
    struct ExtraState
    {
        math::Vec3 previousDisplacement{};
        bool targetObscured = false;
        float occlusionStartTime = 0.0f;
        math::Vec3 previousCameraOffset{};
        math::Vec3 previousCameraPosition{};
        float previousDampTime = 0.0f;
        bool stateIsValid = false;
        float smoothedDistance = 0.0f;
        float smoothedTime = 0.0f;
    };

    ExtraState& extraState(CameraId cameraId);
    const ExtraState* findExtraState(CameraId cameraId) const;
    static void resetDistanceSmoothing(
        ExtraState& extra, float smoothingTime, float now);
    static float applyDistanceSmoothing(
        const ExtraState& extra, float distance, float smoothingTime, float now);
    static void updateDistanceSmoothing(ExtraState& extra, float distance, float now);

    math::Vec3 preserveLineOfSight(
        CameraState& state,
        const IPhysicsWorld& physics,
        math::Vec3 targetPoint);
    bool pullCameraInFrontOfNearestObstacle(
        const IPhysicsWorld& physics,
        math::Vec3 cameraPosition,
        math::Vec3 targetPoint,
        math::Vec3& newPosition,
        SphereCastHit& hit) const;
    math::Vec3 pushCameraBack(
        const IPhysicsWorld& physics,
        math::Vec3 currentPosition,
        math::Vec3 pushDirection,
        const SphereCastHit& obstacle,
        math::Vec3 targetPoint,
        math::Plane startPlane,
        float targetDistance,
        int iterations) const;
    bool getWalkingDirection(
        const IPhysicsWorld& physics,
        math::Vec3 position,
        math::Vec3 pushDirection,
        const SphereCastHit& obstacle,
        math::Vec3& direction) const;
    float getPushBackDistance(
        const Ray& ray,
        math::Plane startPlane,
        float targetDistance,
        math::Vec3 targetPoint) const;
    math::Vec3 respectCameraRadius(
        const IPhysicsWorld& physics,
        math::Vec3 cameraPosition,
        math::Vec3 targetPoint) const;
    bool isTargetObscured(
        const CameraState& state,
        const IPhysicsWorld& physics) const;

    std::unordered_map<CameraId, ExtraState> extraStates_;
};

} // namespace cinerig


