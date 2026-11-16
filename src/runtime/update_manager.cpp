#include <cinerig/runtime/update_manager.hpp>

#include <algorithm>

namespace cinerig
{

UpdateManager::UpdateManager(
    VirtualCameraRegistry& registry,
    IEntityWorld& world,
    UpdateTracker& tracker)
    : registry_(registry), world_(world), tracker_(tracker)
{
}

void UpdateManager::updateAll(
    OutputChannels channelMask,
    math::Vec3 worldUp,
    float deltaTime,
    UpdateClock clock,
    bool smart)
{
    advanceRoundRobin(world_.frame(UpdateClock::Late));
    refreshUpdateLevels();

    const auto& levels = updateLevels_;
    for (std::size_t level = levels.size(); level-- > 0;)
    {
        const auto& cameras = levels[level];
        for (std::size_t index = cameras.size(); index-- > 0;)
        {
            VirtualCamera* camera = cameras[index];
            if (camera == nullptr || !camera->isEnabled())
                continue;

            if (!hasAnyChannel(channelMask, camera->outputChannel))
                continue;

            const bool standby =
                camera->isLive()
                || camera->standbyUpdate == StandbyUpdateMode::Always
                || (camera->standbyUpdate == StandbyUpdateMode::RoundRobin
                    && isRoundRobinSelected(level, index));

            if (standby)
                updateVirtualCamera(*camera, worldUp, deltaTime, clock, smart);
        }
    }
}

void UpdateManager::updateVirtualCamera(
    VirtualCamera& camera,
    math::Vec3 worldUp,
    float deltaTime,
    UpdateClock clock,
    bool smart)
{
    if (smart && !shouldUpdateOnSmartClock(camera, clock))
        return;

    const std::int64_t frame = static_cast<std::int64_t>(world_.frame(clock));
    const std::int64_t fixedFrame = static_cast<std::int64_t>(world_.frame(UpdateClock::Fixed));

    auto [it, inserted] = statuses_.try_emplace(&camera);
    CameraUpdateStatus& status = it->second;
    if (inserted)
    {
        status.lastUpdateFrame = frame + 2;
        status.lastUpdateFixedFrame = fixedFrame + 2;
        status.lastUpdateMode = UpdateClock::Late;
    }

    const std::int64_t frameDelta = clock == UpdateClock::Late
        ? frame - status.lastUpdateFrame
        : fixedFrame - status.lastUpdateFixedFrame;

    if (deltaTime >= 0.0f)
    {
        if (frameDelta == 0 && status.lastUpdateMode == clock)
            return;
        if (!unitTestMode_ && frameDelta > 0)
            deltaTime *= static_cast<float>(frameDelta);
    }

    camera.internalUpdateCameraState(worldUp, deltaTime);

    status.lastUpdateFrame = frame;
    status.lastUpdateFixedFrame = fixedFrame;
    status.lastUpdateMode = clock;
}

void UpdateManager::forget(const VirtualCamera* camera)
{
    statuses_.erase(camera);
}

void UpdateManager::reset()
{
    statuses_.clear();
    updateLevels_.clear();
    roundRobinLevel_ = 0;
    roundRobinIndex_ = 0;
    lastRoundRobinFrame_ = std::numeric_limits<std::uint64_t>::max();
}

const UpdateManager::CameraUpdateStatus* UpdateManager::status(const VirtualCamera* camera) const
{
    const auto it = statuses_.find(camera);
    return it == statuses_.end() ? nullptr : &it->second;
}

bool UpdateManager::unitTestMode() const noexcept
{
    return unitTestMode_;
}

void UpdateManager::setUnitTestMode(bool enabled) noexcept
{
    unitTestMode_ = enabled;
}

bool UpdateManager::shouldUpdateOnSmartClock(const VirtualCamera& camera, UpdateClock clock) const
{
    if (camera.updateTargetEntity == 0)
        return clock == UpdateClock::Late;

    UpdateClock preferred = tracker_.preferredUpdate(camera.updateTargetEntity);
    if (world_.lastChangeClock(camera.updateTargetEntity) == UpdateClock::Fixed)
        preferred = UpdateClock::Fixed;
    return preferred == clock;
}

void UpdateManager::refreshUpdateLevels()
{
    const auto& levels = registry_.camerasByLevel();
    updateLevels_.resize(levels.size());
    for (std::size_t level = 0; level < levels.size(); ++level)
    {
        updateLevels_[level].assign(levels[level].begin(), levels[level].end());
    }
}

void UpdateManager::advanceRoundRobin(std::uint64_t lateFrame)
{
    if (lateFrame == lastRoundRobinFrame_)
        return;
    lastRoundRobinFrame_ = lateFrame;

    const auto& levels = registry_.camerasByLevel();
    if (levels.empty())
    {
        roundRobinLevel_ = 0;
        roundRobinIndex_ = 0;
        return;
    }

    if (roundRobinLevel_ >= levels.size())
        roundRobinLevel_ = 0;

    if (!levels[roundRobinLevel_].empty())
        ++roundRobinIndex_;

    while (roundRobinLevel_ < levels.size() && roundRobinIndex_ >= levels[roundRobinLevel_].size())
    {
        roundRobinIndex_ = 0;
        ++roundRobinLevel_;
    }

    if (roundRobinLevel_ >= levels.size())
    {
        roundRobinLevel_ = 0;
        roundRobinIndex_ = 0;
    }
}

bool UpdateManager::isRoundRobinSelected(std::size_t level, std::size_t index) const
{
    return roundRobinLevel_ == level && roundRobinIndex_ == index;
}

}
