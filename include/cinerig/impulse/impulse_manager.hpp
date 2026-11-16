#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <cinerig/core/signal_source.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/curve.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig::impulse
{

struct EnvelopeDefinition
{
    math::Curve attackShape;
    math::Curve decayShape;
    float attackTime = 0.0f;
    float sustainTime = 0.0f;
    float decayTime = 0.0f;
    bool scaleWithImpact = false;
    bool holdForever = false;

    static EnvelopeDefinition makeDefault();
    float duration() const noexcept;
    float getValueAt(float offset) const;
    void changeStopTime(float offset, bool forceNoDecay);
    void clear();
    void validate();
};

class ImpulseEvent
{
public:
    enum class DirectionMode
    {
        Fixed,
        RotateTowardSource,
    };

    enum class DissipationMode
    {
        LinearDecay,
        SoftDecay,
        ExponentialDecay,
    };

    float startTime = 0.0f;
    EnvelopeDefinition envelope{};
    std::unique_ptr<ISignalSource6D> signalSource;
    math::Vec3 position{};
    float radius = 0.0f;
    DirectionMode directionMode = DirectionMode::Fixed;
    std::uint32_t channel = 0;
    DissipationMode dissipationMode = DissipationMode::ExponentialDecay;
    float dissipationDistance = 100.0f;
    float customDissipation = -1.0f;
    float propagationSpeed = 343.0f;

    bool expired(float currentTime) const;
    void cancel(float currentTime, bool forceNoDecay);
    float distanceDecay(float distance) const;
    bool getDecayedSignal(
        float currentTime,
        math::Vec3 listenerPosition,
        bool use2D,
        math::Vec3& pos,
        math::Quat& rot) const;
    void clear();
};

class ImpulseManager
{
public:
    explicit ImpulseManager(IEntityWorld& world) noexcept;

    ImpulseManager(const ImpulseManager&) = delete;
    ImpulseManager& operator=(const ImpulseManager&) = delete;

    IEntityWorld& entityWorld() const noexcept { return world_; }
    float currentTime() const noexcept;

    std::unique_ptr<ImpulseEvent> newImpulseEvent();
    void addImpulseEvent(std::unique_ptr<ImpulseEvent> event);

    bool getImpulseAt(
        math::Vec3 listenerLocation,
        bool distance2D,
        std::uint32_t channelMask,
        math::Vec3& pos,
        math::Quat& rot);
    bool getStrongestImpulseAt(
        math::Vec3 listenerLocation,
        bool distance2D,
        std::uint32_t channelMask,
        math::Vec3& pos,
        math::Quat& rot);

    void clear();
    std::size_t activeEventCount() const noexcept { return activeEvents_.size(); }
    std::size_t expiredEventCount() const noexcept { return expiredEvents_.size(); }

private:
    void pruneExpired();
    std::unique_ptr<ImpulseEvent> takeExpiredEvent();
    void recycle(std::unique_ptr<ImpulseEvent> event);

    IEntityWorld& world_;
    std::vector<std::unique_ptr<ImpulseEvent>> activeEvents_;
    std::vector<std::unique_ptr<ImpulseEvent>> expiredEvents_;
};

} // namespace cinerig::impulse

