#pragma once

#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/runtime/update_tracker.hpp>
#include <cinerig/runtime/virtual_camera_registry.hpp>

namespace cinerig
{

enum class UpdateMethod
{
    Fixed,
    Late,
    Smart,
    Manual,
};

class UpdateManager
{
public:
    struct CameraUpdateStatus
    {
        std::int64_t lastUpdateFrame = 0;
        std::int64_t lastUpdateFixedFrame = 0;
        UpdateClock lastUpdateMode = UpdateClock::Late;
    };

    UpdateManager(
        VirtualCameraRegistry& registry,
        IEntityWorld& world,
        UpdateTracker& tracker);

    void updateAll(
        OutputChannels channelMask,
        math::Vec3 worldUp,
        float deltaTime,
        UpdateClock clock,
        bool smart);

    // Updates one camera, applying the "once per clock frame" guard. Useful
    // for Brain and for tests that drive a single camera.
    void updateVirtualCamera(
        VirtualCamera& camera,
        math::Vec3 worldUp,
        float deltaTime,
        UpdateClock clock,
        bool smart);

    void forget(const VirtualCamera* camera);
    void reset();

    const CameraUpdateStatus* status(const VirtualCamera* camera) const;
    bool unitTestMode() const noexcept;
    void setUnitTestMode(bool enabled) noexcept;

private:
    bool shouldUpdateOnSmartClock(const VirtualCamera& camera, UpdateClock clock) const;
    void refreshUpdateLevels();
    void advanceRoundRobin(std::uint64_t lateFrame);
    bool isRoundRobinSelected(std::size_t level, std::size_t index) const;

    VirtualCameraRegistry& registry_;
    IEntityWorld& world_;
    UpdateTracker& tracker_;
    std::vector<std::vector<VirtualCamera*>> updateLevels_;
    std::unordered_map<const VirtualCamera*, CameraUpdateStatus> statuses_;
    std::size_t roundRobinLevel_ = 0;
    std::size_t roundRobinIndex_ = 0;
    std::uint64_t lastRoundRobinFrame_ = std::numeric_limits<std::uint64_t>::max();
    bool unitTestMode_ = false;
};

}
