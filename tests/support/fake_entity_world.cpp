#include "fake_entity_world.hpp"

namespace cinerig::testing
{

FakeEntityWorld::FakeEntityWorld() = default;

void FakeEntityWorld::add(EntityId id, const Transform& transform)
{
    entities_[id] = transform;
}

void FakeEntityWorld::set(EntityId id, const Transform& transform)
{
    entities_[id] = transform;
}

void FakeEntityWorld::remove(EntityId id)
{
    entities_.erase(id);
    changedClock_.erase(id);
    writable_.erase(id);
}

void FakeEntityWorld::setDeltaTime(UpdateClock clock, float delta)
{
    if (clock == UpdateClock::Fixed)
        fixedDelta_ = delta;
    else
        lateDelta_ = delta;
}

void FakeEntityWorld::setFrame(UpdateClock clock, std::uint64_t frame)
{
    if (clock == UpdateClock::Fixed)
        fixedFrame_ = frame;
    else
        lateFrame_ = frame;
}

void FakeEntityWorld::setTime(float time)
{
    time_ = time;
}

void FakeEntityWorld::setWorldUp(math::Vec3 up)
{
    worldUp_ = up;
}

void FakeEntityWorld::setViewportSize(math::Vec2 size)
{
    viewport_ = size;
}

void FakeEntityWorld::setLastChangeClock(EntityId id, UpdateClock clock)
{
    changedClock_[id] = clock;
}

void FakeEntityWorld::setWritable(EntityId id, bool writable)
{
    writable_[id] = writable;
}

void FakeEntityWorld::setDefaultWritable(bool writable)
{
    defaultWritable_ = writable;
}

void FakeEntityWorld::reset()
{
    entities_.clear();
    changedClock_.clear();
    writable_.clear();
    fixedDelta_ = 1.0f / 60.0f;
    lateDelta_ = 1.0f / 60.0f;
    time_ = 0.0f;
    fixedFrame_ = 0;
    lateFrame_ = 0;
    worldUp_ = math::Vec3::up;
    viewport_ = {1920.0f, 1080.0f};
    defaultWritable_ = true;
}

bool FakeEntityWorld::exists(EntityId id) const
{
    return entities_.find(id) != entities_.end();
}

Transform FakeEntityWorld::transform(EntityId id) const
{
    const auto it = entities_.find(id);
    return it == entities_.end() ? Transform{} : it->second;
}

bool FakeEntityWorld::trySetTransform(EntityId id, const Transform& transform)
{
    const auto it = entities_.find(id);
    if (it == entities_.end())
        return false;

    const auto writable = writable_.find(id);
    const bool canWrite = writable == writable_.end() ? defaultWritable_ : writable->second;
    if (!canWrite)
        return false;

    it->second = transform;
    changedClock_[id] = UpdateClock::Late;
    return true;
}

UpdateClock FakeEntityWorld::lastChangeClock(EntityId id) const
{
    const auto it = changedClock_.find(id);
    return it == changedClock_.end() ? UpdateClock::Late : it->second;
}

float FakeEntityWorld::deltaTime(UpdateClock clock) const
{
    return clock == UpdateClock::Fixed ? fixedDelta_ : lateDelta_;
}

float FakeEntityWorld::fixedDeltaTime() const
{
    return fixedDelta_;
}

float FakeEntityWorld::time() const
{
    return time_;
}

std::uint64_t FakeEntityWorld::frame(UpdateClock clock) const
{
    return clock == UpdateClock::Fixed ? fixedFrame_ : lateFrame_;
}

math::Vec3 FakeEntityWorld::worldUp() const
{
    return worldUp_;
}

math::Vec2 FakeEntityWorld::viewportSize() const
{
    return viewport_;
}

const std::unordered_map<EntityId, Transform>& FakeEntityWorld::entities() const noexcept
{
    return entities_;
}

}
