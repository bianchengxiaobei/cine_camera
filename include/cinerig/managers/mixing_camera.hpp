#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <cinerig/camera/virtual_camera_manager_base.hpp>

namespace cinerig::managers
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Manager camera that takes the weighted average of up to eight child camera
// states. Children are explicitly registered through VirtualCameraManagerBase.
class MixingCamera final : public VirtualCameraManagerBase
{
public:
    static constexpr std::size_t MaxCameras = 8;

    std::array<float, MaxCameras> weights{
        0.5f, 0.5f, 0.5f, 0.5f,
        0.5f, 0.5f, 0.5f, 0.5f,
    };

    explicit MixingCamera(std::string name = "MixingCamera");

    void reset();
    void validate();

    bool setWeight(int index, float weight) noexcept;
    float getWeight(int index) const noexcept;
    bool setWeight(const VirtualCamera& child, float weight) noexcept;
    float getWeight(const VirtualCamera& child) const noexcept;

    float liveChildPercent() const noexcept { return liveChildPercent_; }

    VirtualCamera* liveChild() const override;
    bool isLiveChild(
        const ICinemachineCamera* child,
        bool dominantChildOnly = false) const override;

    void internalUpdateCameraState(math::Vec3 worldUp, float deltaTime) override;
    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
    void invalidateCameraCache() override;

private:
    void clearLiveChildren();

    VirtualCamera* mixLiveChild_ = nullptr;
    std::vector<VirtualCamera*> liveChildren_;
    float liveChildPercent_ = 0.0f;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace cinerig::managers
