#pragma once

#include <vector>

#include <cinerig/brain/camera_blend_stack.hpp>

namespace cinerig
{

class BlendManager : public CameraBlendStack
{
public:
    ICinemachineCamera* activeVirtualCamera() const;
    const Blend* activeBlend() const;
    bool isBlending() const;
    const CameraState& cameraState() const;

    bool isLive(const ICinemachineCamera* camera) const;
    bool isLiveChild(const ICinemachineCamera* camera) const;

    void computeCurrentBlend();
    void resetRootFrame();
    ICinemachineCamera* processActiveCamera(
        ICinemachineMixer* mixer,
        math::Vec3 worldUp,
        float deltaTime,
        bool* wasCut = nullptr);

    void refreshCurrentCameraState(math::Vec3 worldUp, float deltaTime);
    void syncLiveFlags();
    void reset();

private:
    static ICinemachineCamera* deepActiveCamera(const Blend& blend);
    static void collectLiveCameras(
        const Blend& blend,
        std::vector<ICinemachineCamera*>& cameras);

    std::vector<ICinemachineCamera*> previousLiveCameras_;
    std::vector<ICinemachineCamera*> currentLiveCameras_;
    ICinemachineCamera* previousActiveCamera_ = nullptr;
    bool wasBlending_ = false;
    mutable CameraState cachedState_ = CameraState::makeDefault();
};

} // namespace cinerig

