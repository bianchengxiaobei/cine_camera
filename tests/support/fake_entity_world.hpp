#pragma once

#include <cstdint>
#include <unordered_map>

#include <cinerig/engine/entity_world.hpp>

namespace cinerig::testing
{

// Deterministic IEntityWorld implementation for scenario tests. It owns a
// simple entity table and a manually controlled clock; it never reads system
// time. Writable entities are enabled by default.
class FakeEntityWorld final : public IEntityWorld
{
public:
    FakeEntityWorld();

    void add(EntityId id, const Transform& transform);
    void set(EntityId id, const Transform& transform);
    void remove(EntityId id);

    void setDeltaTime(UpdateClock clock, float delta);
    void setUnscaledDeltaTime(UpdateClock clock, float delta);
    void setFrame(UpdateClock clock, std::uint64_t frame);
    void setTime(float time);
    void setUnscaledTime(float time);
    void setWorldUp(math::Vec3 up);
    void setViewportSize(math::Vec2 size);
    void setLastChangeClock(EntityId id, UpdateClock clock);
    void setWritable(EntityId id, bool writable);
    void setDefaultWritable(bool writable);
    void reset();

    bool exists(EntityId id) const override;
    Transform transform(EntityId id) const override;
    bool trySetTransform(EntityId id, const Transform& transform) override;
    UpdateClock lastChangeClock(EntityId id) const override;
    float deltaTime(UpdateClock clock) const override;
    float fixedDeltaTime() const override;
    float time() const override;
    float unscaledDeltaTime(UpdateClock clock) const override;
    float unscaledTime() const override;
    std::uint64_t frame(UpdateClock clock) const override;
    math::Vec3 worldUp() const override;
    math::Vec2 viewportSize() const override;

    const std::unordered_map<EntityId, Transform>& entities() const noexcept;

private:
    std::unordered_map<EntityId, Transform> entities_;
    std::unordered_map<EntityId, UpdateClock> changedClock_;
    std::unordered_map<EntityId, bool> writable_;
    float fixedDelta_ = 1.0f / 60.0f;
    float lateDelta_ = 1.0f / 60.0f;
    float unscaledFixedDelta_ = 0.0f;
    float unscaledLateDelta_ = 0.0f;
    float time_ = 0.0f;
    float unscaledTime_ = 0.0f;
    bool hasUnscaledFixedDelta_ = false;
    bool hasUnscaledLateDelta_ = false;
    bool hasUnscaledTime_ = false;
    std::uint64_t fixedFrame_ = 0;
    std::uint64_t lateFrame_ = 0;
    math::Vec3 worldUp_ = math::Vec3::up;
    math::Vec2 viewport_{1920.0f, 1080.0f};
    bool defaultWritable_ = true;
};

}
