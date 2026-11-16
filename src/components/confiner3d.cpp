#include <cinerig/components/confiner3d.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig
{

void Confiner3D::reset()
{
    boundingVolume = 0;
    slowingDistance = 0.0f;
    extraStates_.clear();
}

void Confiner3D::validate()
{
    slowingDistance = std::max(0.0f, slowingDistance);
}

bool Confiner3D::cameraWasDisplaced(CameraId cameraId) const
{
    return cameraDisplacementDistance(cameraId) > 0.0f;
}

float Confiner3D::cameraDisplacementDistance(CameraId cameraId) const
{
    const ExtraState* extra = findExtraState(cameraId);
    return extra == nullptr ? 0.0f : extra->previousDisplacement.length();
}

void Confiner3D::onTargetObjectWarped(
    EntityId target,
    math::Vec3 positionDelta)
{
    const VirtualCamera* camera = virtualCamera();
    if (camera == nullptr
        || !camera->isEntityTarget(camera->effectiveFollowTarget(), target))
    {
        return;
    }

    for (auto& [cameraId, extra] : extraStates_)
    {
        (void)cameraId;
        extra.previousCameraPosition += positionDelta;
    }
}

float Confiner3D::maxDampTime() const
{
    return slowingDistance * 0.2f;
}

Confiner3D::ExtraState& Confiner3D::extraState(CameraId cameraId)
{
    return extraStates_[cameraId];
}

const Confiner3D::ExtraState* Confiner3D::findExtraState(CameraId cameraId) const
{
    const auto iterator = extraStates_.find(cameraId);
    return iterator == extraStates_.end() ? nullptr : &iterator->second;
}

std::optional<math::Vec3> Confiner3D::confinePoint(
    const IPhysicsWorld& physics,
    math::Vec3 point) const
{
    if (!isValid())
        return std::nullopt;
    return physics.closestPoint(boundingVolume, point);
}

float Confiner3D::distanceFromEdge(
    const IPhysicsWorld& physics,
    math::Vec3 point,
    math::Vec3 directionUnit,
    float maxDistance) const
{
    const math::Vec3 edgePoint = point + directionUnit * maxDistance;
    const auto confined = confinePoint(physics, edgePoint);
    if (!confined.has_value())
        return maxDistance;
    return maxDistance - (*confined - edgePoint).length();
}

void Confiner3D::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    if (stage != Stage::Body || !isValid())
        return;

    const IPhysicsWorld* physics = vcam.physicsWorld();
    if (physics == nullptr)
        return;

    ExtraState& extra = extraState(vcam.cameraId());
    const math::Vec3 cameraPosition = state.correctedPosition();
    const auto confined = confinePoint(*physics, cameraPosition);
    if (!confined.has_value())
        return;

    math::Vec3 newPosition = *confined;
    if (slowingDistance > math::Epsilon
        && deltaTime >= 0.0f
        && vcam.previousStateIsValid)
    {
        const math::Vec3 previousPosition = extra.previousCameraPosition;
        math::Vec3 direction = newPosition - previousPosition;
        const float speed = direction.length();
        if (speed > math::Epsilon)
        {
            direction /= speed;
            const float distance = distanceFromEdge(
                *physics,
                previousPosition,
                direction,
                slowingDistance);
            const float t = distance / slowingDistance;
            newPosition = math::lerp(
                previousPosition,
                newPosition,
                t * t * t + 0.05f);
        }
    }

    const math::Vec3 displacement = newPosition - cameraPosition;
    state.positionCorrection += displacement;
    extra.previousCameraPosition = state.correctedPosition();
    extra.previousDisplacement = displacement;
}

} // namespace cinerig
