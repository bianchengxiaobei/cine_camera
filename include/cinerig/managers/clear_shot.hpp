#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include <cinerig/camera/virtual_camera_manager_base.hpp>

namespace cinerig::managers
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Manager camera that selects the enabled child with the best shot quality,
// with priority tie-breaking and optional activation timing/randomization.
class ClearShot final : public VirtualCameraManagerBase
{
public:
    float activateAfter = 0.0f;
    float minDuration = 0.0f;
    bool randomizeChoice = false;
    std::uint32_t randomSeed = 1;

    explicit ClearShot(std::string name = "ClearShot");

    void reset();
    void validate();
    void resetRandomization();

    const std::vector<VirtualCamera*>& randomizedChildOrder() const noexcept
    {
        return randomizedChildren_;
    }

    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
    void invalidateCameraCache() override;

private:
    void clearSelectionState();

    float activationTime_ = 0.0f;
    float pendingActivationTime_ = 0.0f;
    VirtualCamera* pendingCamera_ = nullptr;
    bool randomizeNow_ = false;
    std::vector<VirtualCamera*> randomizedChildren_;
    std::mt19937 random_;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace cinerig::managers
