#pragma once

#include <cstdint>
#include <string_view>

#include <cinerig/brain/blend_lookup.hpp>
#include <cinerig/brain/blend_manager.hpp>
#include <cinerig/brain/brain_events.hpp>
#include <cinerig/brain/camera_override_stack.hpp>
#include <cinerig/core/camera_interface.hpp>
#include <cinerig/engine/camera_output.hpp>
#include <cinerig/engine/post_processor.hpp>
#include <cinerig/runtime/camera_system.hpp>

namespace cinerig
{

class CameraBrain final : public ICinemachineMixer, public ICameraOverrideStack
{
public:
    CameraBrain(
        CameraSystem& system,
        ICameraOutput& output,
        const BrainConfig& config);

    void update(float deltaTime);
    void fixedUpdate(float fixedDeltaTime);
    void manualUpdate(float deltaTime, std::uint64_t frame);
    void prepareLiveCamera();
    void processFrame(UpdateClock clock, float deltaTime);

    ICinemachineCamera* activeVirtualCamera() const;
    bool isBlending() const;
    const Blend* activeBlend() const;
    const CameraState& state() const override;

    int setCameraOverride(
        int overrideId,
        int priority,
        ICinemachineCamera* camA,
        ICinemachineCamera* camB,
        float weightB,
        float deltaTime) override;
    void releaseCameraOverride(int overrideId) override;
    math::Vec3 defaultWorldUp() const override;

    void resetState();
    void onCameraDestroyed(const ICinemachineCamera* camera);
    void applyToOutput();

    std::string_view name() const override;
    bool isValid() const override;
    ICinemachineMixer* parentCamera() const override;
    bool isLiveChild(
        const ICinemachineCamera* child,
        bool dominantChildOnly = false) const override;

    BrainConfig& config() noexcept;
    const BrainConfig& config() const noexcept;
    void setConfig(const BrainConfig& config);

    BlendLookup& blendLookup() noexcept;
    const BlendLookup& blendLookup() const noexcept;
    void setBlendOverride(BlendLookup::BlendOverride override);
    void setCustomBlender(CameraBlendStack::CustomBlenderFunction customBlender);

    void setPostProcessor(IPostProcessor* processor) noexcept;
    IPostProcessor* postProcessor() const noexcept;

    BlendManager& blendManager() noexcept;
    const BlendManager& blendManager() const noexcept;

    BrainEvents& events() noexcept;
    const BrainEvents& events() const noexcept;

private:
    void updateInternal(UpdateClock clock, float deltaTime);
    void processFrameInternal(UpdateClock clock, float effectiveDeltaTime);
    ICinemachineCamera* topCameraFromPriorityQueue() const;
    math::Vec3 effectiveWorldUp() const;
    float effectiveDeltaTime(UpdateClock clock, float deltaTime) const;
    void applyLensInheritance(CameraState& state) const;

    CameraSystem& system_;
    ICameraOutput& output_;
    BrainConfig config_;
    BrainEvents events_;
    BlendLookup blendLookup_;
    BlendDefinition lastConfigBlend_{BlendStyle::EaseInOut, 2.0f};
    BlendManager blendManager_;
    IPostProcessor* postProcessor_ = nullptr;
    CameraState currentState_ = CameraState::makeDefault();
    std::uint64_t lastUpdateFrame_ = 0;
};

} // namespace cinerig

