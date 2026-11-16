#pragma once

#include <cstdint>

#include <cinerig/core/signal_source.hpp>
#include <cinerig/impulse/impulse_manager.hpp>
#include <cinerig/math/curve.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig::impulse
{

class ImpulseDefinition
{
public:
    enum class ImpulseShape
    {
        Custom,
        Recoil,
        Bump,
        Explosion,
        Rumble,
    };

    enum class ImpulseType
    {
        Uniform,
        Dissipating,
        Propagating,
        Legacy,
    };

    enum class RepeatMode
    {
        Stretch,
        Loop,
    };

    std::uint32_t impulseChannel = 1;
    ImpulseShape impulseShape = ImpulseShape::Custom;
    math::Curve customImpulseShape{};
    float impulseDuration = 0.2f;
    ImpulseType impulseType = ImpulseType::Legacy;
    float dissipationRate = 0.0f;

    const ISignalSource6D* rawSignal = nullptr;
    float amplitudeGain = 1.0f;
    float frequencyGain = 1.0f;
    RepeatMode repeatMode = RepeatMode::Stretch;
    bool randomize = true;
    EnvelopeDefinition timeEnvelope = EnvelopeDefinition::makeDefault();
    float impactRadius = 100.0f;
    ImpulseEvent::DirectionMode directionMode = ImpulseEvent::DirectionMode::Fixed;
    ImpulseEvent::DissipationMode dissipationMode =
        ImpulseEvent::DissipationMode::ExponentialDecay;
    float dissipationDistance = 100.0f;
    float propagationSpeed = 343.0f;
    std::uint32_t seed = 1;

    void validate();

    void createEvent(
        ImpulseManager& manager,
        math::Vec3 position,
        math::Vec3 velocity);
    ImpulseEvent* createAndReturnEvent(
        ImpulseManager& manager,
        math::Vec3 position,
        math::Vec3 velocity);

private:
    ImpulseEvent* legacyCreateAndReturnEvent(
        ImpulseManager& manager,
        math::Vec3 position,
        math::Vec3 velocity);
};

} // namespace cinerig::impulse
