#pragma once

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/core/shot_quality.hpp>
#include <cinerig/engine/physics_world.hpp>

namespace cinerig
{

class ShotQualityEvaluator : public CameraExtension, public IShotQualityEvaluator
{
public:
    struct DistanceEvaluationSettings
    {
        bool enabled = false;
        float optimalDistance = 10.0f;
        float nearLimit = 5.0f;
        float farLimit = 30.0f;
        float maxQualityBoost = 0.2f;
    };

    LayerMask occlusionLayers{1u};
    EntityId ignoreEntity = 0;
    float minimumDistanceFromTarget = 0.2f;
    float cameraRadius = 0.0f;
    DistanceEvaluationSettings distanceEvaluation{};

    void reset();
    void validate();

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    bool isTargetObscured(
        const CameraState& state,
        const IPhysicsWorld& physics) const;
};

} // namespace cinerig
