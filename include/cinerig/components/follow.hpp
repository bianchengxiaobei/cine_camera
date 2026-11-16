#pragma once

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/tracker.hpp>

namespace cinerig
{

class Follow : public CameraComponent
{
public:
    math::Vec3 followOffset{0.0f, 0.0f, -10.0f};
    TrackerSettings trackerSettings{};

    Stage stage() const override { return Stage::Body; }
    bool isValid() const override;
    void mutateCameraState(CameraState& state, float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    float maxDampTime() const override;

    math::Vec3 effectiveOffset() const;

private:
    TrackerTarget trackingTarget() const;

    Tracker targetTracker_;
};

} // namespace cinerig
