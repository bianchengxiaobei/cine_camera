#pragma once

#include <optional>
#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/engine/physics_world.hpp>

namespace cinerig
{

class Confiner3D : public CameraExtension
{
public:
    EntityId boundingVolume = 0;
    float slowingDistance = 0.0f;

    void reset();
    void validate();

    bool isValid() const noexcept { return boundingVolume != 0; }
    bool cameraWasDisplaced(CameraId cameraId) const;
    float cameraDisplacementDistance(CameraId cameraId) const;

    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
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
        math::Vec3 previousCameraPosition{};
    };

    ExtraState& extraState(CameraId cameraId);
    const ExtraState* findExtraState(CameraId cameraId) const;

    std::optional<math::Vec3> confinePoint(
        const IPhysicsWorld& physics,
        math::Vec3 point) const;
    float distanceFromEdge(
        const IPhysicsWorld& physics,
        math::Vec3 point,
        math::Vec3 directionUnit,
        float maxDistance) const;

    std::unordered_map<CameraId, ExtraState> extraStates_;
};

} // namespace cinerig
