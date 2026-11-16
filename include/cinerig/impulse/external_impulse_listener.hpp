#pragma once

#include <cstdint>

#include <cinerig/engine/entity_world.hpp>
#include <cinerig/impulse/impulse_listener.hpp>
#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig::impulse
{

class ExternalImpulseListener
{
public:
    explicit ExternalImpulseListener(ImpulseManager* manager = nullptr);

    ImpulseManager* impulseManager = nullptr;
    EntityId targetEntity = 0;
    std::uint32_t channelMask = 1u;
    float gain = 1.0f;
    bool use2DDistance = false;
    bool useLocalSpace = true;
    ImpulseListener::ImpulseReaction reaction{};

    void update(float deltaTime);
    void reset();

    bool hasAppliedImpulse() const noexcept;
    math::Vec3 lastPositionOffset() const noexcept;
    math::Quat lastRotationOffset() const noexcept;

private:
    void restoreApplied(IEntityWorld& world);
    void clearAppliedState() noexcept;
    void clearResultState() noexcept;

    EntityId appliedEntity_ = 0;
    math::Vec3 appliedPositionOffset_{};
    math::Quat appliedRotationOffset_ = math::Quat::identity();
    bool hasAppliedWrite_ = false;
    math::Vec3 lastPositionOffset_{};
    math::Quat lastRotationOffset_ = math::Quat::identity();
    bool hasAppliedImpulse_ = false;
};

} // namespace cinerig::impulse
