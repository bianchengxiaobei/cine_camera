#include <cinerig/impulse/impulse_listener.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/impulse/impulse_manager.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig::impulse
{
namespace
{

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

void ImpulseListener::ImpulseReaction::reSeed()
{
    std::uint32_t state = seed;
    noiseOffsets = {
        randomOffset(state),
        randomOffset(state),
        randomOffset(state),
    };
    seed = state;
}

bool ImpulseListener::ImpulseReaction::getReaction(
    float currentTime,
    float deltaTime,
    math::Vec3 impulsePos,
    math::Vec3& pos,
    math::Quat& rot)
{
    if (!initialized_)
    {
        initialized_ = true;
        currentAmount_ = 0.0f;
        currentDamping_ = 0.0f;
        currentTime_ = currentTime * frequencyGain;
        if (math::almostZero(noiseOffsets))
            reSeed();
    }

    pos = {};
    rot = math::Quat::identity();
    const float squaredMagnitude = impulsePos.sqrMagnitude();
    if (!secondaryNoise.has_value()
        || (squaredMagnitude < 0.001f && currentAmount_ < 0.0001f))
    {
        return false;
    }

    currentTime_ += deltaTime * frequencyGain;
    currentAmount_ = std::max(currentAmount_, std::sqrt(squaredMagnitude));
    currentDamping_ = std::max(
        currentDamping_,
        std::max(1.0f, std::sqrt(currentAmount_)) * duration);

    const float reactionGain = currentAmount_ * amplitudeGain;
    const math::Vec3 positionNoise =
        secondaryNoise->getCombinedPosition(currentTime_, noiseOffsets) * reactionGain;
    pos = {
        positionNoise.x,
        positionNoise.y,
        -positionNoise.z,
    };

    const math::Vec3 orientationNoise =
        secondaryNoise->getCombinedOrientation(currentTime_, noiseOffsets) * reactionGain;
    rot = math::quatFromEuler({
        -orientationNoise.x * math::DegToRad,
        -orientationNoise.y * math::DegToRad,
        orientationNoise.z * math::DegToRad,
    });

    currentAmount_ -= math::standardDamp(currentAmount_, currentDamping_, deltaTime);
    currentDamping_ -= math::standardDamp(currentDamping_, currentDamping_, deltaTime);
    return true;
}

ImpulseListener::ImpulseListener(ImpulseManager* manager)
    : impulseManager(manager)
{
}

void ImpulseListener::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    (void)vcam;
    if (stage != applyAfter || deltaTime < 0.0f || impulseManager == nullptr)
        return;

    math::Vec3 impulsePos{};
    math::Quat impulseRot = math::Quat::identity();
    const bool haveImpulse = signalCombinationMode == SignalCombinationMode::Additive
        ? impulseManager->getImpulseAt(
            state.finalPosition(),
            use2DDistance,
            channelMask,
            impulsePos,
            impulseRot)
        : impulseManager->getStrongestImpulseAt(
            state.finalPosition(),
            use2DDistance,
            channelMask,
            impulsePos,
            impulseRot);

    math::Vec3 reactionPos{};
    math::Quat reactionRot = math::Quat::identity();
    const bool haveReaction = reaction.getReaction(
        impulseManager->currentTime(),
        deltaTime,
        impulsePos,
        reactionPos,
        reactionRot);

    if (haveImpulse)
    {
        impulseRot = math::slerpUnclamped(math::Quat::identity(), impulseRot, gain);
        impulsePos *= gain;
    }
    if (haveReaction)
    {
        impulsePos += reactionPos;
        impulseRot = impulseRot * reactionRot;
    }
    if (!haveImpulse && !haveReaction)
        return;

    if (useCameraSpace)
        impulsePos = math::rotate(state.rawOrientation, impulsePos);
    state.positionCorrection += impulsePos;
    state.orientationCorrection = state.orientationCorrection * impulseRot;
}

} // namespace cinerig::impulse
