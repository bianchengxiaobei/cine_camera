#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

// Infers whether an entity moves on the fixed or late clock. The tracker is
// owned by CameraSystem and never uses global state.
class UpdateTracker
{
public:
    explicit UpdateTracker(IEntityWorld& world);

    void observe(EntityId entity);
    void record(UpdateClock clock);
    void reset();

    UpdateClock preferredUpdate(EntityId entity) const;
    std::size_t trackedTargetCount() const noexcept;

private:
    struct TargetStatus
    {
        math::Vec3 lastPosition{};
        std::uint64_t lastFixedFrame = 0;
        std::uint64_t windowStart = 0;
        int fixedMoves = 0;
        int lateMoves = 0;
        int windows = 0;
        UpdateClock preferred = UpdateClock::Late;
    };

    void recordTarget(
        EntityId entity, TargetStatus& status, UpdateClock clock,
        std::uint64_t currentFrame, std::uint64_t fixedFrame);

    IEntityWorld& world_;
    std::unordered_map<EntityId, TargetStatus> targets_;
};

}
