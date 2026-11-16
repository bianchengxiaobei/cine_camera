#pragma once

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/core/lookahead_settings.hpp>
#include <cinerig/core/modifier_interfaces.hpp>
#include <cinerig/core/screen_composer_settings.hpp>
#include <cinerig/math/geometry.hpp>
#include <cinerig/math/predictor.hpp>

namespace cinerig
{

class PositionComposer
    : public CameraComponent
    , public IModifiablePositionDamping
    , public IModifiableComposition
    , public IModifiableDistance
{
public:
    float cameraDistance = 10.0f;
    float deadZoneDepth = 0.0f;
    ScreenComposerSettings composition;
    bool centerOnActivate = true;
    math::Vec3 targetOffset{0.0f, 0.0f, 0.0f};
    math::Vec3 damping{1.0f, 1.0f, 1.0f};
    LookaheadSettings lookahead;

    Stage stage() const override { return Stage::Body; }
    bool bodyAppliesAfterAim() const override { return true; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    bool onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    float maxDampTime() const override;

    math::Vec3 getPositionDamping() const override { return damping; }
    void setPositionDamping(math::Vec3 value) override { damping = value; }
    ScreenComposerSettings getComposition() const override { return composition; }
    void setComposition(const ScreenComposerSettings& value) override { composition = value; }
    float getDistance() const override { return cameraDistance; }
    void setDistance(float value) override { cameraDistance = value; }

private:
    static constexpr float kMinimumCameraDistance = 0.01f;

    static math::Rect screenToOrtho(
        const math::Rect& screenRect,
        float orthoSize,
        float aspect);
    static math::Vec3 orthoOffsetToScreenBounds(
        math::Vec3 targetPosition,
        const math::Rect& screenRect);
    math::Vec3 detachedFollowTargetDamp(
        math::Vec3 initial,
        math::Vec3 dampTime,
        float deltaTime) const;

    math::PositionPredictor predictor;
    math::Vec3 previousCameraPosition{};
    math::Quat previousRotation{};
    ScreenComposerSettings previousComposition{};
    float previousDesiredDistance = 10.0f;
    bool inheritingPosition = false;
    bool historyValid = false;
    TargetRef previousFollowTarget{};
    bool hasPreviousFollowTarget = false;
};

} // namespace cinerig
