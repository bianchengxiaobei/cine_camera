#include <cinerig/impulse/external_impulse_listener.hpp>

#include <cinerig/impulse/impulse_manager.hpp>

namespace cinerig::impulse
{

ExternalImpulseListener::ExternalImpulseListener(ImpulseManager* manager)
    : impulseManager(manager)
{
}

void ExternalImpulseListener::update(float deltaTime)
{
    if (deltaTime < 0.0f || impulseManager == nullptr)
        return;

    IEntityWorld& world = impulseManager->entityWorld();
    if (targetEntity == 0)
    {
        if (hasAppliedWrite_)
            restoreApplied(world);
        clearAppliedState();
        clearResultState();
        return;
    }

    if (hasAppliedWrite_ && appliedEntity_ != 0 && appliedEntity_ != targetEntity)
    {
        restoreApplied(world);
        clearAppliedState();
    }

    if (!world.exists(targetEntity))
    {
        clearAppliedState();
        clearResultState();
        return;
    }

    Transform transform = world.transform(targetEntity);
    if (hasAppliedWrite_ && appliedEntity_ == targetEntity)
    {
        transform.position -= appliedPositionOffset_;
        transform.rotation = transform.rotation * math::inverse(appliedRotationOffset_);
    }

    math::Vec3 impulsePos{};
    math::Quat impulseRot = math::Quat::identity();
    const bool haveImpulse = impulseManager->getImpulseAt(
        transform.position,
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

    const math::Vec3 worldOffset = useLocalSpace
        ? math::rotate(transform.rotation, impulsePos)
        : impulsePos;

    if (hasAppliedWrite_ || haveImpulse || haveReaction)
    {
        transform.position += worldOffset;
        transform.rotation = transform.rotation * impulseRot;

        const bool wrote = world.trySetTransform(targetEntity, transform);
        if (wrote)
        {
            const bool hasOffset = worldOffset.x != 0.0f
                || worldOffset.y != 0.0f
                || worldOffset.z != 0.0f
                || impulseRot.x != 0.0f
                || impulseRot.y != 0.0f
                || impulseRot.z != 0.0f
                || impulseRot.w != 1.0f;
            hasAppliedWrite_ = hasOffset;
            appliedEntity_ = hasOffset ? targetEntity : 0;
            appliedPositionOffset_ = worldOffset;
            appliedRotationOffset_ = impulseRot;
        }
    }

    lastPositionOffset_ = worldOffset;
    lastRotationOffset_ = impulseRot;
    hasAppliedImpulse_ = haveImpulse || haveReaction;
}

void ExternalImpulseListener::reset()
{
    if (impulseManager != nullptr)
        restoreApplied(impulseManager->entityWorld());
    clearAppliedState();
    clearResultState();
    reaction.reset();
}

bool ExternalImpulseListener::hasAppliedImpulse() const noexcept
{
    return hasAppliedImpulse_;
}

math::Vec3 ExternalImpulseListener::lastPositionOffset() const noexcept
{
    return lastPositionOffset_;
}

math::Quat ExternalImpulseListener::lastRotationOffset() const noexcept
{
    return lastRotationOffset_;
}

void ExternalImpulseListener::restoreApplied(IEntityWorld& world)
{
    if (!hasAppliedWrite_ || appliedEntity_ == 0 || !world.exists(appliedEntity_))
        return;

    Transform transform = world.transform(appliedEntity_);
    transform.position -= appliedPositionOffset_;
    transform.rotation = transform.rotation * math::inverse(appliedRotationOffset_);
    world.trySetTransform(appliedEntity_, transform);
}

void ExternalImpulseListener::clearAppliedState() noexcept
{
    appliedEntity_ = 0;
    appliedPositionOffset_ = {};
    appliedRotationOffset_ = math::Quat::identity();
    hasAppliedWrite_ = false;
}

void ExternalImpulseListener::clearResultState() noexcept
{
    lastPositionOffset_ = {};
    lastRotationOffset_ = math::Quat::identity();
    hasAppliedImpulse_ = false;
}

} // namespace cinerig::impulse
