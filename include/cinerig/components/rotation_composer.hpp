#pragma once

#include <cstdint>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/core/lookahead_settings.hpp>
#include <cinerig/core/modifier_interfaces.hpp>
#include <cinerig/core/screen_composer_settings.hpp>
#include <cinerig/math/geometry.hpp>
#include <cinerig/math/predictor.hpp>

namespace cinerig
{

class RotationComposer
    : public CameraComponent
    , public IModifiableComposition
{
public:
    ScreenComposerSettings composition;
    bool centerOnActivate = true;
    math::Vec3 targetOffset{0.0f, 0.0f, 0.0f};
    math::Vec2 damping{0.5f, 0.5f};
    LookaheadSettings lookahead;

    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override;
    void prePipelineMutate(CameraState& state, float deltaTime) override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    float maxDampTime() const override;

    ScreenComposerSettings getComposition() const override { return composition; }
    void setComposition(const ScreenComposerSettings& value) override { composition = value; }

private:
    struct FovCache
    {
        math::Rect fovSoftGuideRect{};
        math::Rect fovHardGuideRect{};
        math::Vec2 fov{};
        math::Vec2 halfFovRad{};
        math::Vec2 screenBounds{};
        math::Rect deadZoneRect{};
        math::Rect hardLimitsRect{};
        float orthographicSizeOverDistance = 0.0f;
        float aspect = 0.0f;
        bool orthographic = false;

        void update(
            const LensSettings& lens,
            const math::Rect& softGuide,
            const math::Rect& hardGuide,
            float targetDistance);
        math::Vec2 screenToAngle(math::Vec2 point) const;
        math::Vec3 directionFromScreen(math::Vec2 point) const;
    };

    math::Vec3 getLookAtPointAndSetTrackedPoint(
        math::Vec3 lookAt,
        math::Vec3 up,
        float deltaTime);
    void rotateToScreenBounds(
        CameraState& state,
        math::Rect screenRect,
        math::Vec3 trackedPoint,
        math::Quat& rigOrientation,
        math::Vec2 fov,
        float deltaTime) const;
    static bool clampVerticalBounds(
        math::Rect& rect,
        math::Vec3 direction,
        math::Vec3 up,
        float verticalFovRadians);
    float detachedLookAtDamp(float initial, float dampTime, float deltaTime) const;

    math::Vec3 trackedPoint_{};
    bool trackedPointValid = false;
    TargetRef previousLookAtTarget{};
    bool hasPreviousLookAtTarget = false;
    math::Vec3 cameraPositionPrevFrame{};
    math::Vec3 lookAtPrevFrame{};
    math::Vec2 screenOffsetPrevFrame{};
    math::Quat cameraOrientationPrevFrame{};
    math::PositionPredictor predictor;
    ScreenComposerSettings compositionLastFrame{};
    FovCache cache;
    bool historyValid = false;
};

} // namespace cinerig
