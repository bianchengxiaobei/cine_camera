#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <cinerig/core/camera_interface.hpp>
#include <cinerig/engine/camera_output.hpp>

namespace cinerig
{

class CameraBrain;
class Blend;

struct BlendEventParams
{
    ICinemachineMixer* origin = nullptr;
    Blend* blend = nullptr;
};

using CameraActivatedCallback = std::function<void(const ActivationEventParams&)>;
using CameraDeactivatedCallback = std::function<void(ICinemachineMixer*, ICinemachineCamera*)>;
using BlendCreatedCallback = std::function<void(const BlendEventParams&)>;
using BlendFinishedCallback = std::function<void(ICinemachineMixer*, ICinemachineCamera*)>;
using CameraUpdatedCallback = std::function<void(CameraBrain&)>;

class BrainEvents
{
public:
    void addCameraActivated(CameraActivatedCallback callback)
    {
        cameraActivated.push_back(std::move(callback));
    }

    void addCameraDeactivated(CameraDeactivatedCallback callback)
    {
        cameraDeactivated.push_back(std::move(callback));
    }

    void addBlendCreated(BlendCreatedCallback callback)
    {
        blendCreated.push_back(std::move(callback));
    }

    void addBlendFinished(BlendFinishedCallback callback)
    {
        blendFinished.push_back(std::move(callback));
    }

    void addCameraUpdated(CameraUpdatedCallback callback)
    {
        cameraUpdated.push_back(std::move(callback));
    }

    void clear()
    {
        cameraActivated.clear();
        cameraDeactivated.clear();
        blendCreated.clear();
        blendFinished.clear();
        cameraUpdated.clear();
    }

    void emitCameraActivated(const ActivationEventParams& event) const
    {
        for (const auto& callback : cameraActivated)
        {
            if (callback)
                callback(event);
        }
    }

    void emitCameraDeactivated(ICinemachineMixer* origin, ICinemachineCamera* camera) const
    {
        for (const auto& callback : cameraDeactivated)
        {
            if (callback)
                callback(origin, camera);
        }
    }

    void emitBlendCreated(const BlendEventParams& event) const
    {
        for (const auto& callback : blendCreated)
        {
            if (callback)
                callback(event);
        }
    }

    void emitBlendFinished(ICinemachineMixer* origin, ICinemachineCamera* camera) const
    {
        for (const auto& callback : blendFinished)
        {
            if (callback)
                callback(origin, camera);
        }
    }

    void emitCameraUpdated(CameraBrain& brain) const
    {
        for (const auto& callback : cameraUpdated)
        {
            if (callback)
                callback(brain);
        }
    }

    std::vector<CameraActivatedCallback> cameraActivated;
    std::vector<CameraDeactivatedCallback> cameraDeactivated;
    std::vector<BlendCreatedCallback> blendCreated;
    std::vector<BlendFinishedCallback> blendFinished;
    std::vector<CameraUpdatedCallback> cameraUpdated;
};

} // namespace cinerig
