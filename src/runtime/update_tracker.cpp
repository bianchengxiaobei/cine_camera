#include <cinerig/runtime/update_tracker.hpp>

#include <algorithm>
#include <cmath>

namespace cinerig
{

namespace
{

constexpr int kWindowSize = 30;

bool samePosition(math::Vec3 a, math::Vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

}

UpdateTracker::UpdateTracker(IEntityWorld& world)
    : world_(world)
{
}

void UpdateTracker::observe(EntityId entity)
{
    if (entity == 0 || !world_.exists(entity))
        return;

    const auto [it, inserted] = targets_.try_emplace(entity);
    if (!inserted)
        return;

    it->second.lastPosition = world_.transform(entity).position;
    it->second.lastFixedFrame = world_.frame(UpdateClock::Fixed);
    it->second.windowStart = std::max(world_.frame(UpdateClock::Fixed), world_.frame(UpdateClock::Late));
}

void UpdateTracker::record(UpdateClock clock)
{
    const std::uint64_t fixedFrame = world_.frame(UpdateClock::Fixed);
    const std::uint64_t lateFrame = world_.frame(UpdateClock::Late);
    const std::uint64_t currentFrame = std::max(fixedFrame, lateFrame);

    for (auto& [entity, status] : targets_)
    {
        if (!world_.exists(entity))
            continue;
        recordTarget(entity, status, clock, currentFrame, fixedFrame);
    }

    for (auto it = targets_.begin(); it != targets_.end();)
    {
        if (!world_.exists(it->first))
            it = targets_.erase(it);
        else
            ++it;
    }
}

void UpdateTracker::recordTarget(
    EntityId entity, TargetStatus& status, UpdateClock clock,
    std::uint64_t currentFrame, std::uint64_t fixedFrame)
{
    const math::Vec3 position = world_.transform(entity).position;
    if (!samePosition(position, status.lastPosition))
    {
        if (clock == UpdateClock::Late)
        {
            ++status.lateMoves;
        }
        else if (status.lastFixedFrame != fixedFrame)
        {
            ++status.fixedMoves;
            status.lastFixedFrame = fixedFrame;
        }
        status.lastPosition = position;
    }

    UpdateClock choice = UpdateClock::Late;
    if (status.fixedMoves > 3 && status.lateMoves < status.fixedMoves / 3)
        choice = UpdateClock::Fixed;

    if (status.windows == 0)
        status.preferred = choice;

    if (currentFrame >= status.windowStart + kWindowSize)
    {
        status.preferred = choice;
        ++status.windows;
        status.windowStart = currentFrame;
        status.lateMoves = status.preferred == UpdateClock::Late ? 1 : 0;
        status.fixedMoves = status.preferred == UpdateClock::Fixed ? 1 : 0;
    }
}

void UpdateTracker::reset()
{
    targets_.clear();
}

UpdateClock UpdateTracker::preferredUpdate(EntityId entity) const
{
    const auto it = targets_.find(entity);
    return it == targets_.end() ? UpdateClock::Late : it->second.preferred;
}

std::size_t UpdateTracker::trackedTargetCount() const noexcept
{
    return targets_.size();
}

}
