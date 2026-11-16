#include "sdl_world.hpp"

#include <algorithm>

namespace cinerig::examples
{

EntityId SdlWorld::createEntity(Transform transform)
{
    const EntityId id = nextEntityId_++;
    transforms_.emplace(id, transform);
    markChanged(id);
    return id;
}

void SdlWorld::destroyEntity(EntityId id)
{
    transforms_.erase(id);
    lastChangeClock_.erase(id);
}

void SdlWorld::setTransform(EntityId id, Transform transform)
{
    const auto iterator = transforms_.find(id);
    if (iterator == transforms_.end())
        return;
    iterator->second = transform;
    markChanged(id);
}

bool SdlWorld::exists(EntityId id) const
{
    return transforms_.find(id) != transforms_.end();
}

Transform SdlWorld::transform(EntityId id) const
{
    const auto iterator = transforms_.find(id);
    return iterator == transforms_.end() ? Transform{} : iterator->second;
}

bool SdlWorld::trySetTransform(EntityId id, const Transform& transform)
{
    const auto iterator = transforms_.find(id);
    if (iterator == transforms_.end())
        return false;
    iterator->second = transform;
    markChanged(id);
    return true;
}

UpdateClock SdlWorld::lastChangeClock(EntityId id) const
{
    const auto iterator = lastChangeClock_.find(id);
    return iterator == lastChangeClock_.end() ? UpdateClock::Late : iterator->second;
}

float SdlWorld::deltaTime(UpdateClock clock) const
{
    return clock == UpdateClock::Fixed ? fixedDeltaTime_ : lateDeltaTime_;
}

float SdlWorld::fixedDeltaTime() const
{
    return fixedDeltaTime_;
}

float SdlWorld::time() const
{
    return time_;
}

std::uint64_t SdlWorld::frame(UpdateClock clock) const
{
    return clock == UpdateClock::Fixed ? fixedFrame_ : lateFrame_;
}

math::Vec3 SdlWorld::worldUp() const
{
    return worldUp_;
}

math::Vec2 SdlWorld::viewportSize() const
{
    return viewport_;
}

void SdlWorld::setDeltaTime(UpdateClock clock, float deltaTime)
{
    if (clock == UpdateClock::Fixed)
        fixedDeltaTime_ = deltaTime;
    else
        lateDeltaTime_ = deltaTime;
    currentClock_ = clock;
}

void SdlWorld::setTime(float seconds)
{
    time_ = std::max(0.0f, seconds);
}

void SdlWorld::advanceTime(float deltaTime)
{
    time_ += std::max(0.0f, deltaTime);
}

void SdlWorld::setFrame(UpdateClock clock, std::uint64_t frameIndex)
{
    if (clock == UpdateClock::Fixed)
        fixedFrame_ = frameIndex;
    else
        lateFrame_ = frameIndex;
}

void SdlWorld::advanceFrame(UpdateClock clock)
{
    if (clock == UpdateClock::Fixed)
        ++fixedFrame_;
    else
        ++lateFrame_;
}

void SdlWorld::setWorldUp(math::Vec3 up)
{
    if (!math::almostZero(up))
        worldUp_ = math::normalize(up);
}

void SdlWorld::setViewportSize(math::Vec2 size)
{
    viewport_ = {std::max(1.0f, size.x), std::max(1.0f, size.y)};
}

void SdlWorld::setCurrentClock(UpdateClock clock)
{
    currentClock_ = clock;
}

UpdateClock SdlWorld::currentClock() const noexcept
{
    return currentClock_;
}

std::size_t SdlWorld::entityCount() const noexcept
{
    return transforms_.size();
}

void SdlWorld::markChanged(EntityId id)
{
    lastChangeClock_[id] = currentClock_;
}

}
