#include <cinerig/components/shot_quality_evaluator.hpp>

#include <algorithm>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

void ShotQualityEvaluator::reset()
{
    occlusionLayers = LayerMask{1u};
    ignoreEntity = 0;
    minimumDistanceFromTarget = 0.2f;
    cameraRadius = 0.0f;
    distanceEvaluation = DistanceEvaluationSettings{};
}

void ShotQualityEvaluator::validate()
{
    cameraRadius = std::max(0.0f, cameraRadius);
    minimumDistanceFromTarget = std::max(0.01f, minimumDistanceFromTarget);
    distanceEvaluation.nearLimit =
        std::max(0.1f, distanceEvaluation.nearLimit);
    distanceEvaluation.farLimit = std::max(
        distanceEvaluation.nearLimit,
        distanceEvaluation.farLimit);
    distanceEvaluation.optimalDistance = math::clamp(
        distanceEvaluation.optimalDistance,
        distanceEvaluation.nearLimit,
        distanceEvaluation.farLimit);
}

void ShotQualityEvaluator::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)deltaTime;

    if (stage != Stage::Finalize || !state.hasLookAt())
        return;

    const IPhysicsWorld* physics = vcam.physicsWorld();
    const bool targetObscured = state.isTargetOffscreen()
        || (physics != nullptr && occlusionLayers.bits != 0u
            && isTargetObscured(state, *physics));
    if (targetObscured)
        state.shotQuality *= 0.2f;

    if (!distanceEvaluation.enabled
        || distanceEvaluation.optimalDistance <= 0.0f)
    {
        return;
    }

    float nearnessBoost = 0.0f;
    float distance =
        (state.referenceLookAt - state.finalPosition()).length();
    if (distance <= distanceEvaluation.optimalDistance)
    {
        if (distance >= distanceEvaluation.nearLimit)
        {
            const float range = distanceEvaluation.optimalDistance
                - distanceEvaluation.nearLimit;
            nearnessBoost = range > math::Epsilon
                ? distanceEvaluation.maxQualityBoost
                    * (distance - distanceEvaluation.nearLimit) / range
                : distanceEvaluation.maxQualityBoost;
        }
    }
    else
    {
        distance -= distanceEvaluation.optimalDistance;
        if (distance < distanceEvaluation.farLimit)
        {
            nearnessBoost = distanceEvaluation.maxQualityBoost
                * (1.0f - distance / distanceEvaluation.farLimit);
        }
    }
    state.shotQuality *= 1.0f + nearnessBoost;
}

bool ShotQualityEvaluator::isTargetObscured(
    const CameraState& state,
    const IPhysicsWorld& physics) const
{
    const math::Vec3 position = state.correctedPosition();
    math::Vec3 direction = state.referenceLookAt - position;
    const float distance = direction.length();
    if (distance < std::max(minimumDistanceFromTarget, math::Epsilon))
        return true;

    direction = math::normalize(direction);
    const QueryFilter filter{occlusionLayers, ignoreEntity};
    return physics.sphereCast(
        Ray{position, direction},
        cameraRadius,
        distance - minimumDistanceFromTarget,
        filter).has_value();
}

} // namespace cinerig
