#pragma once

#include <cstdint>
#include <optional>

#include <cinerig/core/camera_interface.hpp>
#include <cinerig/core/camera_state.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class VirtualCamera;

enum class Stage
{
    Body = 0,
    Aim,
    Noise,
    Finalize,
};

inline constexpr int kStageCount = 4;

class CameraComponent
{
public:
    virtual ~CameraComponent() = default;

    VirtualCamera& virtualCamera() const;
    VirtualCamera* virtualCameraOrNull() const noexcept;
    bool isAttached() const noexcept;

    std::optional<math::Vec3> followTargetPosition() const;
    std::optional<math::Quat> followTargetRotation() const;
    std::optional<math::Vec3> lookAtTargetPosition() const;
    std::optional<math::Quat> lookAtTargetRotation() const;

    virtual Stage stage() const = 0;
    virtual bool isValid() const = 0;
    virtual bool bodyAppliesAfterAim() const { return false; }

    virtual void prePipelineMutate(CameraState& state, float deltaTime)
    {
        (void)state;
        (void)deltaTime;
    }

    virtual void mutateCameraState(CameraState& state, float deltaTime) = 0;

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

    virtual void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
    {
        (void)target;
        (void)positionDelta;
    }

    virtual void forceCameraPosition(math::Vec3 pos, math::Quat rot)
    {
        (void)pos;
        (void)rot;
    }

    virtual float maxDampTime() const { return 0.0f; }

private:
    friend class Pipeline;

    void setVirtualCamera(VirtualCamera* camera) noexcept { owner_ = camera; }

    VirtualCamera* owner_ = nullptr;
};

} // namespace cinerig
