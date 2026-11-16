#pragma once

#include <unordered_map>
#include <vector>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/camera/camera_lens_cache_invalidator.hpp>
#include <cinerig/components/confiner_oven.hpp>
#include <cinerig/core/lens_settings.hpp>
#include <cinerig/math/matrix.hpp>

namespace cinerig
{

class Confiner2D : public CameraExtension, public ICameraLensCacheInvalidator
{
public:
    struct OversizeWindowSettings
    {
        bool enabled = false;
        float maxWindowSize = 0.0f;
        float padding = 0.0f;
    };

    std::vector<std::vector<math::Vec2>> boundingShape;
    math::Vec2 boundingShapeOffset{};
    math::Mat4 boundingShapeToWorld = math::Mat4::identity();
    float damping = 0.5f;
    float slowingDistance = 5.0f;
    OversizeWindowSettings oversizeWindow{};

    void reset();
    void validate();

    bool isValid() const noexcept { return !boundingShape.empty(); }
    bool boundingShapeIsBaked() const;
    bool bakeBoundingShape(VirtualCamera& vcam, float maxTimeInSeconds);
    float bakeProgress() const;
    ConfinerOven::BakingState bakingState() const;

    void invalidateBoundingShapeCache();
    void invalidateLensCache() override;

    bool cameraWasDisplaced(CameraId cameraId) const;
    float cameraDisplacementDistance(CameraId cameraId) const;
    std::vector<std::vector<math::Vec2>> bakedSolutionPaths(
        CameraId cameraId) const;

    static float calculateHalfFrustumHeight(
        const LensSettings& lens,
        float cameraPositionLocalZ);

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
        ConfinerOven::BakedSolution bakedSolution{};
        float frustumHeight = 0.0f;
        math::Vec3 previousDisplacement{};
        math::Vec3 dampedDisplacement{};
        math::Vec3 previousCameraPosition{};
    };

    struct ShapeCache
    {
        bool valid = false;
        ConfinerOven oven{};
        math::Mat4 bakedToWorld = math::Mat4::identity();
        math::Mat4 deltaWorldToBaked = math::Mat4::identity();
        math::Mat4 deltaBakedToWorld = math::Mat4::identity();
        float aspectRatio = 1.0f;
        float deltaScale = 1.0f;
        OversizeWindowSettings oversize{};

        void invalidate();
    };

    bool validateShapeCache(float aspectRatio, bool& changed);
    void calculateDelta();
    bool deltaIsUniform() const;
    bool oversizeMatches(const ShapeCache& cache) const;

    ExtraState& extraState(CameraId cameraId);
    const ExtraState* findExtraState(CameraId cameraId) const;

    math::Vec3 confinePoint(
        math::Vec3 point,
        math::Vec3 forward,
        const ExtraState& extra) const;
    float distanceFromEdge(
        math::Vec3 point,
        math::Vec3 directionUnit,
        float maxDistance,
        math::Vec3 forward,
        const ExtraState& extra) const;

    ShapeCache shapeCache_{};
    std::unordered_map<CameraId, ExtraState> extraStates_;
};

} // namespace cinerig
