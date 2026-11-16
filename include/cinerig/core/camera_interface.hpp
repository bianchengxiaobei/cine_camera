#pragma once

#include <string_view>

#include <cinerig/core/camera_state.hpp>

namespace cinerig
{

class ICinemachineMixer;

struct ActivationEventParams;

class ICinemachineCamera
{
public:
    virtual ~ICinemachineCamera() = default;

    virtual std::string_view name() const = 0;
    virtual const CameraState& state() const = 0;
    virtual bool isValid() const = 0;
    virtual ICinemachineMixer* parentCamera() const { return nullptr; }
    virtual void updateCameraState(math::Vec3 worldUp, float deltaTime)
    {
        (void)worldUp;
        (void)deltaTime;
    }
    virtual void onCameraActivated(const ActivationEventParams& event)
    {
        (void)event;
    }
};

class ICinemachineMixer : public virtual ICinemachineCamera
{
public:
    ~ICinemachineMixer() override = default;

    virtual bool isLiveChild(
        const ICinemachineCamera* child,
        bool dominantChildOnly = false) const = 0;
};

struct ActivationEventParams
{
    ICinemachineMixer* origin = nullptr;
    ICinemachineCamera* outgoingCamera = nullptr;
    ICinemachineCamera* incomingCamera = nullptr;
    bool isCut = false;
    math::Vec3 worldUp{};
    float deltaTime = 0.0f;
};

}
