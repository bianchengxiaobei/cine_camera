#pragma once

#include <unordered_map>

#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/core/camera_state.hpp>
#include <cinerig/math/geometry.hpp>
#include <cinerig/math/vec.hpp>
#include <cinerig/targets/target_group.hpp>

namespace cinerig
{

class VirtualCamera;

namespace targets
{

class GroupFraming : public CameraExtension
{
public:
    enum class FramingMode
    {
        Horizontal,
        Vertical,
        HorizontalAndVertical,
    };

    enum class SizeAdjustmentMode
    {
        ZoomOnly,
        DollyOnly,
        DollyThenZoom,
    };

    enum class LateralAdjustmentMode
    {
        ChangePosition,
        ChangeRotation,
    };

    static constexpr float kMinimumGroupSize = 0.01f;

    FramingMode framingMode = FramingMode::HorizontalAndVertical;
    float framingSize = 0.8f;
    math::Vec2 centerOffset{};
    float damping = 2.0f;
    SizeAdjustmentMode sizeAdjustment = SizeAdjustmentMode::DollyThenZoom;
    LateralAdjustmentMode lateralAdjustment = LateralAdjustmentMode::ChangePosition;
    math::Vec2 fovRange{1.0f, 100.0f};
    math::Vec2 dollyRange{-100.0f, 100.0f};
    math::Vec2 orthoSizeRange{1.0f, 1000.0f};

    math::Bounds groupBounds{};
    math::Vec3 groupBoundsPosition{};
    math::Quat groupBoundsRotation{};

    void validate();
    void reset(VirtualCamera& vcam);
    float maxDampTime() const override;

    void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam,
        CameraState& state,
        float deltaTime) override;

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime) override;

private:
    struct CameraCache
    {
        math::Vec3 positionAdjustment{};
        math::Vec2 rotationAdjustment{};
        float fovAdjustment = 0.0f;
        int bodyCallsThisFrame = 0;

        void reset()
        {
            positionAdjustment = {};
            rotationAdjustment = {};
            fovAdjustment = 0.0f;
        }
    };

    float frameHeight(math::Vec2 boundsSize, float aspect) const;
    void perspectiveFraming(
        VirtualCamera& vcam,
        const TargetGroup& group,
        CameraCache& cache,
        CameraState& state,
        float deltaTime);
    void orthoFraming(
        VirtualCamera& vcam,
        const TargetGroup& group,
        CameraCache& cache,
        CameraState& state,
        float deltaTime);
    void adjustSize(
        const TargetGroup& group,
        float aspect,
        math::Vec3& camPos,
        math::Quat& camRot,
        float& fov,
        float& dollyAmount);
    void computeCameraViewGroupBounds(
        const TargetGroup& group,
        math::Vec3& camPos,
        math::Quat& camRot,
        bool moveCamera);

    std::unordered_map<const VirtualCamera*, CameraCache> caches_;
};

} // namespace targets
} // namespace cinerig

