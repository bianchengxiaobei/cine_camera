#include <cinerig/impulse/impulse_manager.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

#include <cinerig/math/damp.hpp>

namespace cinerig::impulse
{
namespace
{

constexpr float kMinSpeed = 1.0f;


float bezier1(float t, float p0, float p1, float p2, float p3)
{
    t = math::clamp01(t);
    const float inverse = 1.0f - t;
    return inverse * inverse * inverse * p0
        + 3.0f * inverse * inverse * t * p1
        + 3.0f * inverse * t * t * p2
        + t * t * t * p3;
}

float evaluateDissipationScale(float spread, float normalizedDistance)
{
    const float low = -0.8f;
    const float high = 0.8f;
    float bend = low + (high - low) * (1.0f - spread);
    bend = (1.0f - bend) * 0.5f;
    const float denominator =
        (((1.0f / math::clamp01(bend)) - 2.0f) * (1.0f - normalizedDistance)) + 1.0f;
    const float t = math::clamp01(normalizedDistance) / denominator;
    return 1.0f - bezier1(t, 0.0f, 0.0f, 1.0f, 1.0f);
}

float distance2D(math::Vec3 a, math::Vec3 b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    return std::sqrt(x * x + y * y);
}

} // namespace

EnvelopeDefinition EnvelopeDefinition::makeDefault()
{
    EnvelopeDefinition result;
    result.decayTime = 0.7f;
    result.sustainTime = 0.2f;
    result.scaleWithImpact = true;
    return result;
}

float EnvelopeDefinition::duration() const noexcept
{
    return holdForever ? -1.0f : attackTime + sustainTime + decayTime;
}

float EnvelopeDefinition::getValueAt(float offset) const
{
    if (offset < 0.0f)
        return 0.0f;

    if (offset < attackTime && attackTime > math::Epsilon)
    {
        if (attackShape.size() < 2)
            return math::standardDamp(1.0f, attackTime, offset);
        return attackShape.evaluate(offset / attackTime);
    }

    offset -= attackTime;
    if (holdForever || offset < sustainTime)
        return 1.0f;

    offset -= sustainTime;
    if (offset < decayTime && decayTime > math::Epsilon)
    {
        if (decayShape.size() < 2)
            return 1.0f - math::standardDamp(1.0f, decayTime, offset);
        return decayShape.evaluate(offset / decayTime);
    }

    return 0.0f;
}

void EnvelopeDefinition::changeStopTime(float offset, bool forceNoDecay)
{
    offset = std::max(0.0f, offset);
    if (offset < attackTime)
        attackTime = 0.0f;
    sustainTime = offset - attackTime;
    if (forceNoDecay)
        decayTime = 0.0f;
}

void EnvelopeDefinition::clear()
{
    attackShape = {};
    decayShape = {};
    attackTime = 0.0f;
    sustainTime = 0.0f;
    decayTime = 0.0f;
    holdForever = false;
}

void EnvelopeDefinition::validate()
{
    attackTime = std::max(0.0f, attackTime);
    sustainTime = std::max(0.0f, sustainTime);
    decayTime = std::max(0.0f, decayTime);
}

bool ImpulseEvent::expired(float currentTime) const
{
    const float envelopeDuration = envelope.duration();
    const float maxDistance = radius + dissipationDistance;
    const float arrivalTime =
        currentTime - maxDistance / std::max(kMinSpeed, propagationSpeed);
    return envelopeDuration > 0.0f && startTime + envelopeDuration <= arrivalTime;
}

void ImpulseEvent::cancel(float currentTime, bool forceNoDecay)
{
    envelope.holdForever = false;
    envelope.changeStopTime(currentTime - startTime, forceNoDecay);
}

float ImpulseEvent::distanceDecay(float distance) const
{
    const float safeRadius = std::max(radius, 0.0f);
    if (distance < safeRadius)
        return 1.0f;

    distance -= safeRadius;
    if (distance >= dissipationDistance)
        return 0.0f;

    if (customDissipation >= 0.0f)
    {
        const float normalized = dissipationDistance > math::Epsilon
            ? distance / dissipationDistance
            : 1.0f;
        return evaluateDissipationScale(customDissipation, normalized);
    }

    switch (dissipationMode)
    {
    case DissipationMode::SoftDecay:
        return 0.5f * (1.0f + std::cos(math::Pi * (distance / dissipationDistance)));
    case DissipationMode::ExponentialDecay:
        return 1.0f - math::standardDamp(1.0f, dissipationDistance, distance);
    default:
        return math::lerp(1.0f, 0.0f, distance / dissipationDistance);
    }
}

bool ImpulseEvent::getDecayedSignal(
    float currentTime,
    math::Vec3 listenerPosition,
    bool use2D,
    math::Vec3& pos,
    math::Quat& rot) const
{
    if (!signalSource)
    {
        pos = {};
        rot = math::Quat::identity();
        return false;
    }

    const float distance = use2D
        ? distance2D(listenerPosition, position)
        : (listenerPosition - position).length();
    const float time = currentTime - startTime
        - distance / std::max(kMinSpeed, propagationSpeed);
    const float scale = envelope.getValueAt(time) * distanceDecay(distance);
    if (scale == 0.0f)
    {
        pos = {};
        rot = math::Quat::identity();
        return false;
    }

    signalSource->getSignal(time, pos, rot);
    pos *= scale;
    rot = math::slerpUnclamped(math::Quat::identity(), rot, scale);

    if (directionMode == DirectionMode::RotateTowardSource && distance > math::Epsilon)
    {
        math::Quat direction = math::fromToRotation(
            math::Vec3::up,
            listenerPosition - position);
        if (radius > math::Epsilon)
        {
            const float t = math::clamp01(distance / radius);
            direction = math::slerp(
                direction,
                math::Quat::identity(),
                std::cos(math::Pi * t / 2.0f));
        }
        pos = math::rotate(direction, pos);
    }

    return true;
}

void ImpulseEvent::clear()
{
    startTime = 0.0f;
    envelope.clear();
    signalSource.reset();
    position = {};
    radius = 0.0f;
    directionMode = DirectionMode::Fixed;
    channel = 0;
    dissipationMode = DissipationMode::ExponentialDecay;
    dissipationDistance = 100.0f;
    customDissipation = -1.0f;
    propagationSpeed = 343.0f;
}

ImpulseManager::ImpulseManager(IEntityWorld& world) noexcept
    : world_(world)
{
}

float ImpulseManager::currentTime() const noexcept
{
    return world_.time();
}

std::unique_ptr<ImpulseEvent> ImpulseManager::takeExpiredEvent()
{
    if (expiredEvents_.empty())
        return nullptr;

    std::unique_ptr<ImpulseEvent> event = std::move(expiredEvents_.back());
    expiredEvents_.pop_back();
    return event;
}

std::unique_ptr<ImpulseEvent> ImpulseManager::newImpulseEvent()
{
    std::unique_ptr<ImpulseEvent> event = takeExpiredEvent();
    if (!event)
        event = std::make_unique<ImpulseEvent>();
    event->clear();
    return event;
}

void ImpulseManager::addImpulseEvent(std::unique_ptr<ImpulseEvent> event)
{
    if (!event)
        return;

    event->startTime = currentTime();
    activeEvents_.push_back(std::move(event));
}

void ImpulseManager::pruneExpired()
{
    const float now = currentTime();
    for (std::size_t index = activeEvents_.size(); index > 0; --index)
    {
        std::unique_ptr<ImpulseEvent>& event = activeEvents_[index - 1];
        if (event && !event->expired(now))
            continue;

        std::unique_ptr<ImpulseEvent> removed = std::move(event);
        activeEvents_.erase(activeEvents_.begin() + static_cast<std::ptrdiff_t>(index - 1));
        recycle(std::move(removed));
    }
}

void ImpulseManager::recycle(std::unique_ptr<ImpulseEvent> event)
{
    if (!event)
        return;
    event->clear();
    expiredEvents_.push_back(std::move(event));
}

bool ImpulseManager::getImpulseAt(
    math::Vec3 listenerLocation,
    bool distance2D,
    std::uint32_t channelMask,
    math::Vec3& pos,
    math::Quat& rot)
{
    pruneExpired();

    bool nontrivial = false;
    pos = {};
    rot = math::Quat::identity();
    for (const std::unique_ptr<ImpulseEvent>& event : activeEvents_)
    {
        if (!event || (event->channel & channelMask) == 0u)
            continue;

        math::Vec3 eventPos{};
        math::Quat eventRot{};
        if (event->getDecayedSignal(currentTime(), listenerLocation, distance2D, eventPos, eventRot))
        {
            nontrivial = true;
            pos += eventPos;
            rot = rot * eventRot;
        }
    }
    return nontrivial;
}

bool ImpulseManager::getStrongestImpulseAt(
    math::Vec3 listenerLocation,
    bool distance2D,
    std::uint32_t channelMask,
    math::Vec3& pos,
    math::Quat& rot)
{
    pruneExpired();

    bool nontrivial = false;
    float strongestMagnitude = 0.0f;
    math::Vec3 strongestPos{};
    math::Quat strongestRot = math::Quat::identity();
    const float now = currentTime();
    for (const std::unique_ptr<ImpulseEvent>& event : activeEvents_)
    {
        if (!event || (event->channel & channelMask) == 0u)
            continue;

        math::Vec3 eventPos{};
        math::Quat eventRot{};
        if (!event->getDecayedSignal(now, listenerLocation, distance2D, eventPos, eventRot))
            continue;

        nontrivial = true;
        const float magnitude = eventPos.sqrMagnitude();
        if (magnitude > strongestMagnitude)
        {
            strongestMagnitude = magnitude;
            strongestPos = eventPos;
            strongestRot = eventRot;
        }
    }
    pos = strongestPos;
    rot = strongestRot;
    return nontrivial;
}

void ImpulseManager::clear()
{
    for (std::unique_ptr<ImpulseEvent>& event : activeEvents_)
        recycle(std::move(event));
    activeEvents_.clear();
    expiredEvents_.clear();
}

} // namespace cinerig::impulse

