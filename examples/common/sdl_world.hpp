#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <cinerig/engine/entity_world.hpp>

namespace cinerig::examples
{

// Minimal host entity database for the examples. It owns no SDL state, so it
// can be used by tests and headless tools without creating a window.
class SdlWorld final : public IEntityWorld
{
public:
    SdlWorld() = default;

    EntityId createEntity(Transform transform = {});
    void destroyEntity(EntityId id);
    void setTransform(EntityId id, Transform transform);

    bool exists(EntityId id) const override;
    Transform transform(EntityId id) const override;
    bool trySetTransform(EntityId id, const Transform& transform) override;
    UpdateClock lastChangeClock(EntityId id) const override;

    float deltaTime(UpdateClock clock) const override;
    float fixedDeltaTime() const override;
    float time() const override;
    std::uint64_t frame(UpdateClock clock) const override;
    math::Vec3 worldUp() const override;
    math::Vec2 viewportSize() const override;

    void setDeltaTime(UpdateClock clock, float deltaTime);
    void setTime(float seconds);
    void advanceTime(float deltaTime);
    void setFrame(UpdateClock clock, std::uint64_t frameIndex);
    void advanceFrame(UpdateClock clock);
    void setWorldUp(math::Vec3 up);
    void setViewportSize(math::Vec2 size);
    void setCurrentClock(UpdateClock clock);
    UpdateClock currentClock() const noexcept;
    std::size_t entityCount() const noexcept;

private:
    void markChanged(EntityId id);

    std::unordered_map<EntityId, Transform> transforms_;
    std::unordered_map<EntityId, UpdateClock> lastChangeClock_;
    EntityId nextEntityId_ = 1;
    float fixedDeltaTime_ = 1.0f / 60.0f;
    float lateDeltaTime_ = 0.0f;
    float time_ = 0.0f;
    std::uint64_t fixedFrame_ = 0;
    std::uint64_t lateFrame_ = 0;
    math::Vec3 worldUp_ = math::Vec3::up;
    math::Vec2 viewport_{1280.0f, 720.0f};
    UpdateClock currentClock_ = UpdateClock::Late;
};

}
