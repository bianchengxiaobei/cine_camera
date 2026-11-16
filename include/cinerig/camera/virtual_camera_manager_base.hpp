#pragma once

#include <string>
#include <vector>

#include <cinerig/brain/blend_lookup.hpp>
#include <cinerig/brain/blend_manager.hpp>
#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/core/camera_interface.hpp>

namespace cinerig
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// A virtual camera that owns a set of child cameras, chooses one active child
// according to a derived-class policy, and blends between child states.
//
// Children are registered explicitly instead of being discovered through an
// engine transform hierarchy. This keeps the core independent of scene graph
// ownership while preserving the manager semantics of the Unity implementation.
class VirtualCameraManagerBase : public VirtualCamera, public ICinemachineMixer
{
public:
    struct DefaultTargetSettings
    {
        bool enabled = false;
        CameraTarget target{};
    };

    explicit VirtualCameraManagerBase(std::string name = "VirtualCameraManager");
    ~VirtualCameraManagerBase() override;

    VirtualCameraManagerBase(const VirtualCameraManagerBase&) = delete;
    VirtualCameraManagerBase& operator=(const VirtualCameraManagerBase&) = delete;

    DefaultTargetSettings defaultTarget{};
    BlendDefinition defaultBlend{BlendStyle::EaseInOut, 0.5f};
    BlendLookupTable customBlends{};

    bool addChild(VirtualCamera& child);
    bool removeChild(VirtualCamera& child);

    const std::vector<VirtualCamera*>& childCameras() const noexcept;
    virtual VirtualCamera* liveChild() const;
    bool isBlending() const;
    const Blend* activeBlend() const;

    virtual void invalidateCameraCache();

    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override;
    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;
    void onTargetObjectWarped(EntityId target, math::Vec3 positionDelta) override;
    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override;
    void onCameraActivated(const ActivationEventParams& event) override;

    TargetRef effectiveFollowTarget() const override;
    TargetRef effectiveLookAtTarget() const override;

    bool isLiveChild(
        const ICinemachineCamera* child,
        bool dominantChildOnly = false) const override;

protected:
    virtual VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) = 0;

    virtual BlendDefinition lookupBlend(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming) const;

    void setLiveChild(VirtualCamera* camera, math::Vec3 worldUp, float deltaTime);
    void resetLiveChild();
    void finalizeCameraState(float deltaTime);
    virtual void updateCameraCache();

    bool wouldCreateCycle(const VirtualCamera& child) const;

private:
    bool containsChild(const VirtualCamera* child) const noexcept;
    void propagatePreviousStateInvalid();
    void pruneDetachedChildren();

    BlendManager blendManager_;
    std::vector<VirtualCamera*> childCameras_;
    const ICinemachineCamera* transitioningFrom_ = nullptr;
    bool cacheValid_ = true;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace cinerig
