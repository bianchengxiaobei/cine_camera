#include <cinerig/brain/blend_manager.hpp>

#include <cinerig/camera/virtual_camera.hpp>

#include <algorithm>
#include <memory>
#include <utility>

namespace cinerig
{

ICinemachineCamera* BlendManager::activeVirtualCamera() const
{
    return deepActiveCamera(currentBlend());
}

const Blend* BlendManager::activeBlend() const
{
    const Blend& blend = currentBlend();
    if (blend.camA == nullptr || blend.isComplete())
        return nullptr;
    return &blend;
}

bool BlendManager::isBlending() const
{
    return activeBlend() != nullptr;
}

const CameraState& BlendManager::cameraState() const
{
    return cachedState_;
}

bool BlendManager::isLive(const ICinemachineCamera* camera) const
{
    return currentBlend().uses(camera);
}

bool BlendManager::isLiveChild(const ICinemachineCamera* camera) const
{
    return isLive(camera);
}

void BlendManager::computeCurrentBlend()
{
    CameraBlendStack::computeCurrentBlend();
    cachedState_ = currentBlend().state();
}

void BlendManager::resetRootFrame()
{
    CameraBlendStack::resetRootFrame();
    cachedState_ = CameraState::makeDefault();
}

ICinemachineCamera* BlendManager::processActiveCamera(
    ICinemachineMixer* mixer,
    math::Vec3 worldUp,
    float deltaTime,
    bool* wasCut)
{
    if (wasCut != nullptr)
        *wasCut = false;

    for (ICinemachineCamera* camera : previousLiveCameras_)
    {
        if (camera != nullptr && !isLive(camera) && events_ != nullptr)
            events_->emitCameraDeactivated(mixer, camera);
    }

    ICinemachineCamera* incomingCamera = activeVirtualCamera();
    if (incomingCamera != nullptr && incomingCamera->isValid())
    {
        ICinemachineCamera* outgoingCamera = previousActiveCamera_;
        if (outgoingCamera != nullptr && !outgoingCamera->isValid())
            outgoingCamera = nullptr;

        if (incomingCamera == outgoingCamera)
        {
            if (wasBlending_ && currentBlend().camA == nullptr && events_ != nullptr)
                events_->emitBlendFinished(mixer, incomingCamera);
        }
        else
        {
            ICinemachineCamera* eventOutgoing = outgoingCamera;
            std::unique_ptr<NestedBlendSource> activeBlendSource;
            if (isBlending())
            {
                activeBlendSource =
                    std::make_unique<NestedBlendSource>(&currentBlend());
                eventOutgoing = activeBlendSource.get();
            }

            ActivationEventParams event;
            event.origin = mixer;
            event.outgoingCamera = eventOutgoing;
            event.incomingCamera = incomingCamera;
            event.isCut = !isBlending();
            if (wasCut != nullptr)
                *wasCut = event.isCut;
            event.worldUp = worldUp;
            event.deltaTime = deltaTime;

            incomingCamera->onCameraActivated(event);
            if (mixer != nullptr)
                mixer->onCameraActivated(event);
            if (events_ != nullptr)
                events_->emitCameraActivated(event);

            incomingCamera->updateCameraState(worldUp, deltaTime);
        }
    }

    syncLiveFlags();
    previousLiveCameras_.clear();
    collectLiveCameras(currentBlend(), previousLiveCameras_);
    previousActiveCamera_ = deepActiveCamera(currentBlend());
    wasBlending_ = currentBlend().camA != nullptr;
    cachedState_ = currentBlend().state();
    return incomingCamera;
}

void BlendManager::refreshCurrentCameraState(math::Vec3 worldUp, float deltaTime)
{
    currentBlend().updateCameraState(worldUp, deltaTime);
}

void BlendManager::syncLiveFlags()
{
    for (ICinemachineCamera* camera : previousLiveCameras_)
    {
        if (camera != nullptr && !isLive(camera))
        {
            if (auto* virtualCamera = dynamic_cast<VirtualCamera*>(camera))
                virtualCamera->setLive(false);
        }
    }

    currentLiveCameras_.clear();
    collectLiveCameras(currentBlend(), currentLiveCameras_);
    for (ICinemachineCamera* camera : currentLiveCameras_)
    {
        if (auto* virtualCamera = dynamic_cast<VirtualCamera*>(camera))
            virtualCamera->setLive(true);
    }
}

void BlendManager::reset()
{
    for (ICinemachineCamera* camera : previousLiveCameras_)
    {
        if (auto* virtualCamera = dynamic_cast<VirtualCamera*>(camera))
            virtualCamera->setLive(false);
    }
    CameraBlendStack::reset();
    previousLiveCameras_.clear();
    currentLiveCameras_.clear();
    previousActiveCamera_ = nullptr;
    wasBlending_ = false;
    cachedState_ = CameraState::makeDefault();
}

ICinemachineCamera* BlendManager::deepActiveCamera(const Blend& blend)
{
    ICinemachineCamera* camera = blend.camB;
    while (auto* nested = dynamic_cast<NestedBlendSource*>(camera))
    {
        if (nested->blend() == nullptr)
            return nullptr;
        camera = nested->blend()->camB;
    }
    return camera != nullptr && camera->isValid() ? camera : nullptr;
}

void BlendManager::collectLiveCameras(
    const Blend& blend,
    std::vector<ICinemachineCamera*>& cameras)
{
    auto add = [&cameras](ICinemachineCamera* camera)
    {
        if (camera != nullptr
            && std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
        {
            cameras.push_back(camera);
        }
    };

    if (auto* nested = dynamic_cast<NestedBlendSource*>(blend.camA))
    {
        if (nested->blend() != nullptr)
            collectLiveCameras(*nested->blend(), cameras);
    }
    else
    {
        add(blend.camA);
    }

    if (auto* nested = dynamic_cast<NestedBlendSource*>(blend.camB))
    {
        if (nested->blend() != nullptr)
            collectLiveCameras(*nested->blend(), cameras);
    }
    else
    {
        add(blend.camB);
    }
}

} // namespace cinerig

