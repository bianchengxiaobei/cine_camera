#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/camera/camera_extension.hpp>
#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig::testing
{

inline std::string_view stageName(Stage stage)
{
    switch (stage)
    {
    case Stage::Body:
        return "Body";
    case Stage::Aim:
        return "Aim";
    case Stage::Noise:
        return "Noise";
    case Stage::Finalize:
        return "Finalize";
    }
    return "Unknown";
}

class RecordingComponent final : public CameraComponent
{
public:
    RecordingComponent(
        Stage componentStage,
        std::string componentName,
        std::vector<std::string>* executionLog = nullptr,
        bool appliesAfterAim = false,
        bool componentValid = true)
        : stage_(componentStage),
          name(std::move(componentName)),
          log(executionLog),
          bodyAppliesAfterAim_(appliesAfterAim),
          valid(componentValid)
    {
    }

    Stage stage() const override { return stage_; }
    bool isValid() const override { return valid; }
    bool bodyAppliesAfterAim() const override { return bodyAppliesAfterAim_; }

    void prePipelineMutate(CameraState&, float) override
    {
        previousAtPre = virtualCamera().previousStateIsValid;
        append("pre:" + name);
    }

    void mutateCameraState(CameraState& state, float) override
    {
        previousAtMutate = virtualCamera().previousStateIsValid;
        state.rawPosition += mutateOffset;
        append("mutate:" + name);
    }

    bool onTransitionFromCamera(
        const ICinemachineCamera*,
        math::Vec3,
        float) override
    {
        ++transitionCount;
        append("transition:" + name);
        return transitionResult;
    }

    void onTargetObjectWarped(EntityId target, math::Vec3 delta) override
    {
        warpedTarget = target;
        warpDelta = delta;
        append("warp:" + name);
    }

    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override
    {
        forcedPosition = pos;
        forcedRotation = rot;
        append("force:" + name);
    }

    float maxDampTime() const override { return dampTime; }

    Stage stage_;
    std::string name;
    std::vector<std::string>* log = nullptr;
    bool bodyAppliesAfterAim_ = false;
    bool valid = true;
    bool previousAtPre = false;
    bool previousAtMutate = false;
    bool transitionResult = false;
    int transitionCount = 0;
    float dampTime = 0.0f;
    math::Vec3 mutateOffset{};
    math::Vec3 forcedPosition{};
    math::Quat forcedRotation{};
    EntityId warpedTarget = 0;
    math::Vec3 warpDelta{};

private:
    void append(std::string entry)
    {
        if (log != nullptr)
            log->push_back(std::move(entry));
    }
};

class RecordingExtension final : public CameraExtension
{
public:
    RecordingExtension(
        std::string extensionName,
        std::vector<std::string>* executionLog = nullptr,
        bool forceTransition = false)
        : name(std::move(extensionName)),
          log(executionLog),
          forceTransition_(forceTransition)
    {
    }

    void prePipelineMutateCameraStateCallback(
        VirtualCamera& vcam,
        CameraState&,
        float) override
    {
        ++preCount;
        callbackCamera = &vcam;
        append("ext-pre:" + name);
    }

    void postPipelineStageCallback(
        VirtualCamera& vcam,
        Stage stage,
        CameraState&,
        float) override
    {
        ++postCount;
        lastStage = stage;
        callbackCamera = &vcam;
        append("ext-post:" + name + ":" + std::string(stageName(stage)));
    }

    void onTargetObjectWarped(EntityId target, math::Vec3 delta) override
    {
        warpedTarget = target;
        warpDelta = delta;
        append("ext-warp:" + name);
    }

    bool onTransitionFromCamera(
        const ICinemachineCamera*,
        math::Vec3,
        float) override
    {
        ++transitionCount;
        append("ext-transition:" + name);
        return forceTransition_;
    }

    void forceCameraPosition(math::Vec3 pos, math::Quat rot) override
    {
        forcedPosition = pos;
        forcedRotation = rot;
        append("ext-force:" + name);
    }

    float maxDampTime() const override { return dampTime; }

    std::string name;
    std::vector<std::string>* log = nullptr;
    bool forceTransition_ = false;
    int preCount = 0;
    int postCount = 0;
    int transitionCount = 0;
    Stage lastStage = Stage::Body;
    VirtualCamera* callbackCamera = nullptr;
    float dampTime = 0.0f;
    math::Vec3 forcedPosition{};
    math::Quat forcedRotation{};
    EntityId warpedTarget = 0;
    math::Vec3 warpDelta{};

private:
    void append(std::string entry)
    {
        if (log != nullptr)
            log->push_back(std::move(entry));
    }
};

} // namespace cinerig::testing
