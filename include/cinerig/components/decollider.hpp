#pragma once

#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/engine/physics_world.hpp>

namespace cinerig
{

class Decollider : public CameraExtension
{
public:
    struct FollowTargetSettings
    {
        bool enabled = false;
        float yOffset = 0.0f;
    };

    struct DecollisionSettings
    {
        bool enabled = false;
        LayerMask obstacleLayers{1u};
        FollowTargetSettings useFollowTarget{};
        float damping = 0.5f;
        float smoothingTime = 0.0f;
    };

    struct TerrainSettings
    {
        bool enabled = true;
        LayerMask terrainLayers{1u};
        float maximumRaycast = 10.0f;
        float damping = 0.5f;
    };

    float cameraRadius = 0.4f;
    DecollisionSettings decollision{};
    TerrainSettings terrainResolution{};

    void reset();
    void validate();

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;
    float maxDampTime() const override;

private:
    struct ExtraState
    {
        float previousTerrainDisplacement = 0.0f;
        float previousDistanceFromTarget = 0.0f;
        math::Vec3 previousDecollisionDisplacement{};
        math::Vec3 previousCorrectedCameraPosition{};
        float smoothedDistance = 0.0f;
        float smoothingStartTime = 0.0f;
    };

    ExtraState& extraState(CameraId cameraId);
    static float updateDistanceSmoothing(
        ExtraState& extra,
        float distance,
        float smoothingTime,
        bool haveDisplacement,
        float now);

    float resolveTerrain(
        const IPhysicsWorld& physics,
        const ExtraState& extra,
        math::Vec3 cameraPosition,
        math::Vec3 up,
        float deltaTime) const;
    math::Vec3 decollideCamera(
        const IPhysicsWorld& physics,
        math::Vec3 cameraPosition,
        math::Vec3 targetPoint) const;
    math::Vec3 applySmoothingAndDamping(
        math::Vec3 displacement,
        math::Vec3 targetPoint,
        math::Vec3 oldCameraPosition,
        ExtraState& extra,
        float deltaTime,
        float now) const;

    std::unordered_map<CameraId, ExtraState> extraStates_;
};

} // namespace cinerig

