#include <cinerig/impulse/impulse_definition.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace cinerig::impulse
{
namespace
{

constexpr float kBigNumber = 9999999.0f;

float evaluateCurve(const math::Curve& curve, float time)
{
    if (curve.keyframes.empty())
        return 0.0f;
    if (curve.keyframes.size() == 1 || time <= curve.keyframes.front().time)
        return curve.keyframes.front().value;
    if (time >= curve.keyframes.back().time)
        return curve.keyframes.back().value;

    for (std::size_t index = 1; index < curve.keyframes.size(); ++index)
    {
        const math::Keyframe& next = curve.keyframes[index];
        if (time > next.time)
            continue;

        const math::Keyframe& previous = curve.keyframes[index - 1];
        const float duration = next.time - previous.time;
        if (duration <= math::Epsilon)
            return next.value;

        const float t = (time - previous.time) / duration;
        const float t2 = t * t;
        const float t3 = t2 * t;
        const float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        const float h10 = t3 - 2.0f * t2 + t;
        const float h01 = -2.0f * t3 + 3.0f * t2;
        const float h11 = t3 - t2;
        return h00 * previous.value
            + h10 * duration * previous.outTangent
            + h01 * next.value
            + h11 * duration * next.inTangent;
    }

    return curve.keyframes.back().value;
}

math::Curve standardCurve(ImpulseDefinition::ImpulseShape shape)
{
    switch (shape)
    {
    case ImpulseDefinition::ImpulseShape::Recoil:
        return math::Curve({
            {0.0f, 1.0f, -3.2f, -3.2f},
            {1.0f, 0.0f, 0.0f, 0.0f},
        });
    case ImpulseDefinition::ImpulseShape::Bump:
        return math::Curve({
            {0.0f, 0.0f, -4.9f, -4.9f},
            {0.2f, 0.0f, 8.25f, 8.25f},
            {1.0f, 0.0f, -0.25f, -0.25f},
        });
    case ImpulseDefinition::ImpulseShape::Explosion:
        return math::Curve({
            {0.0f, -1.4f, -7.9f, -7.9f},
            {0.27f, 0.78f, 23.4f, 23.4f},
            {0.54f, -0.12f, 22.6f, 22.6f},
            {0.75f, 0.042f, 9.23f, 9.23f},
            {0.9f, -0.02f, 5.8f, 5.8f},
            {0.95f, -0.006f, -3.0f, -3.0f},
            {1.0f, 0.0f, 0.0f, 0.0f},
        });
    case ImpulseDefinition::ImpulseShape::Rumble:
        return math::Curve({
            {0.0f, 0.0f, 0.0f, 0.0f},
            {0.1f, 0.25f, 0.0f, 0.0f},
            {0.2f, 0.0f, 0.0f, 0.0f},
            {0.3f, 0.75f, 0.0f, 0.0f},
            {0.4f, 0.0f, 0.0f, 0.0f},
            {0.5f, 1.0f, 0.0f, 0.0f},
            {0.6f, 0.0f, 0.0f, 0.0f},
            {0.7f, 0.75f, 0.0f, 0.0f},
            {0.8f, 0.0f, 0.0f, 0.0f},
            {0.9f, 0.25f, 0.0f, 0.0f},
            {1.0f, 0.0f, 0.0f, 0.0f},
        });
    default:
        return {};
    }
}

class ShapeSignalSource final : public ISignalSource6D
{
public:
    ShapeSignalSource(math::Curve curve, math::Vec3 velocity, float duration)
        : curve_(std::move(curve)),
          velocity_(velocity),
          duration_(duration)
    {
    }

    float signalDuration() const override { return duration_; }

    void getSignal(
        float timeSinceSignalStart,
        math::Vec3& pos,
        math::Quat& rot) const override
    {
        pos = velocity_ * evaluateCurve(
            curve_,
            timeSinceSignalStart / std::max(math::Epsilon, duration_));
        rot = math::Quat::identity();
    }

private:
    math::Curve curve_;
    math::Vec3 velocity_;
    float duration_;
};

class LegacySignalSource final : public ISignalSource6D
{
public:
    LegacySignalSource(
        const ISignalSource6D* rawSignal,
        math::Vec3 velocity,
        float amplitudeGain,
        float frequencyGain,
        ImpulseDefinition::RepeatMode repeatMode,
        float envelopeDuration,
        float startTimeOffset)
        : rawSignal_(rawSignal),
          velocity_(velocity),
          amplitudeGain_(amplitudeGain),
          frequencyGain_(frequencyGain),
          repeatMode_(repeatMode),
          envelopeDuration_(envelopeDuration),
          startTimeOffset_(startTimeOffset)
    {
    }

    float signalDuration() const override
    {
        return rawSignal_ == nullptr ? 0.0f : rawSignal_->signalDuration();
    }

    void getSignal(
        float timeSinceSignalStart,
        math::Vec3& pos,
        math::Quat& rot) const override
    {
        if (rawSignal_ == nullptr)
        {
            pos = {};
            rot = math::Quat::identity();
            return;
        }

        float time = startTimeOffset_ + timeSinceSignalStart * frequencyGain_;
        const float duration = signalDuration();
        if (duration > 0.0f)
        {
            if (repeatMode_ == ImpulseDefinition::RepeatMode::Loop)
                time = std::fmod(time, duration);
            else if (envelopeDuration_ > math::Epsilon)
                time *= envelopeDuration_ / duration;
        }

        rawSignal_->getSignal(time, pos, rot);
        float gain = velocity_.length() * amplitudeGain_;
        pos *= gain;
        pos = math::rotate(math::fromToRotation(math::Vec3::down, velocity_), pos);
        rot = math::slerpUnclamped(math::Quat::identity(), rot, gain);
    }

private:
    const ISignalSource6D* rawSignal_ = nullptr;
    math::Vec3 velocity_{};
    float amplitudeGain_ = 1.0f;
    float frequencyGain_ = 1.0f;
    ImpulseDefinition::RepeatMode repeatMode_ = ImpulseDefinition::RepeatMode::Stretch;
    float envelopeDuration_ = 0.0f;
    float startTimeOffset_ = 0.0f;
};

std::uint32_t splitMix32(std::uint32_t& state) noexcept
{
    state += 0x9e3779b9u;
    std::uint32_t value = state;
    value = (value ^ (value >> 16)) * 0x85ebca6bu;
    value = (value ^ (value >> 13)) * 0xc2b2ae35u;
    return value ^ (value >> 16);
}

float randomOffset(std::uint32_t& state) noexcept
{
    const float unit = static_cast<float>(splitMix32(state)) / 4294967296.0f;
    return unit * 2000.0f - 1000.0f;
}

} // namespace

void ImpulseDefinition::validate()
{
    impulseDuration = std::max(math::Epsilon, impulseDuration);
    dissipationDistance = std::max(math::Epsilon, dissipationDistance);
    dissipationRate = math::clamp01(dissipationRate);
    propagationSpeed = std::max(1.0f, propagationSpeed);
    impactRadius = std::max(0.0f, impactRadius);
    timeEnvelope.validate();
}

void ImpulseDefinition::createEvent(
    ImpulseManager& manager,
    math::Vec3 position,
    math::Vec3 velocity)
{
    (void)createAndReturnEvent(manager, position, velocity);
}

ImpulseEvent* ImpulseDefinition::createAndReturnEvent(
    ImpulseManager& manager,
    math::Vec3 position,
    math::Vec3 velocity)
{
    if (impulseType == ImpulseType::Legacy)
        return legacyCreateAndReturnEvent(manager, position, velocity);

    if ((impulseShape == ImpulseShape::Custom && customImpulseShape.empty())
        || std::abs(dissipationDistance) < math::Epsilon
        || std::abs(impulseDuration) < math::Epsilon)
    {
        return nullptr;
    }

    math::Curve curve = impulseShape == ImpulseShape::Custom
        ? customImpulseShape
        : standardCurve(impulseShape);

    std::unique_ptr<ImpulseEvent> event = manager.newImpulseEvent();
    event->envelope = {};
    event->envelope.sustainTime = impulseDuration;
    event->signalSource = std::make_unique<ShapeSignalSource>(
        std::move(curve),
        velocity,
        impulseDuration);
    event->position = position;
    event->radius = impulseType == ImpulseType::Uniform ? kBigNumber : 0.0f;
    event->channel = impulseChannel;
    event->directionMode = ImpulseEvent::DirectionMode::Fixed;
    event->dissipationDistance =
        impulseType == ImpulseType::Uniform ? 0.0f : dissipationDistance;
    event->propagationSpeed = impulseType == ImpulseType::Propagating
        ? std::max(1.0f, propagationSpeed)
        : kBigNumber;
    event->customDissipation = dissipationRate;

    ImpulseEvent* result = event.get();
    manager.addImpulseEvent(std::move(event));
    return result;
}

ImpulseEvent* ImpulseDefinition::legacyCreateAndReturnEvent(
    ImpulseManager& manager,
    math::Vec3 position,
    math::Vec3 velocity)
{
    if (rawSignal == nullptr
        || std::abs(timeEnvelope.duration()) < math::Epsilon)
    {
        return nullptr;
    }

    const float originalDuration = timeEnvelope.duration();
    std::unique_ptr<ImpulseEvent> event = manager.newImpulseEvent();
    event->envelope = timeEnvelope;
    if (timeEnvelope.scaleWithImpact)
        event->envelope.decayTime *= std::sqrt(velocity.length());

    float startTimeOffset = 0.0f;
    if (randomize && rawSignal->signalDuration() <= 0.0f)
        startTimeOffset = randomOffset(seed);

    event->signalSource = std::make_unique<LegacySignalSource>(
        rawSignal,
        velocity,
        amplitudeGain,
        frequencyGain,
        repeatMode,
        originalDuration,
        startTimeOffset);
    event->position = position;
    event->radius = impactRadius;
    event->channel = impulseChannel;
    event->directionMode = directionMode;
    event->dissipationMode = dissipationMode;
    event->dissipationDistance = dissipationDistance;
    event->propagationSpeed = propagationSpeed;
    event->customDissipation = -1.0f;

    ImpulseEvent* result = event.get();
    manager.addImpulseEvent(std::move(event));
    return result;
}

} // namespace cinerig::impulse
