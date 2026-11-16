#pragma once

#include <cstdint>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/core/camera_interface.hpp>
#include <cinerig/core/camera_state.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class VirtualCamera;

class CameraExtension
{
public:
    virtual ~CameraExtension() = default;

    VirtualCamera* virtualCamera() const noexcept { return owner_; }
    bool isAttached() const noexcept { return owner_ != nullptr; }
    bool isEnabled() const noexcept { return enabled_; }
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    virtual void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam,
        CameraState& state,
        float deltaTime)
    {
        (void)vcam;
        (void)state;
        (void)deltaTime;
    }

    virtual void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState& state,
        float deltaTime)
    {
        (void)vcam;
        (void)stage;
        (void)state;
        (void)deltaTime;
    }

    virtual void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
    {
        (void)target;
        (void)positionDelta;
    }

    virtual bool onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime)
    {
        (void)from;
        (void)worldUp;
        (void)deltaTime;
        return false;
    }

    virtual void forceCameraPosition(math::Vec3 pos, math::Quat rot)
    {
        (void)pos;
        (void)rot;
    }

    virtual float maxDampTime() const { return 0.0f; }

private:
    friend class VirtualCamera;

    void setVirtualCamera(VirtualCamera* camera) noexcept { owner_ = camera; }

    VirtualCamera* owner_ = nullptr;
    bool enabled_ = true;
};

} // namespace cinerig
