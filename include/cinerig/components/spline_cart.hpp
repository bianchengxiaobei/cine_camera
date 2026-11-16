#pragma once

#include <optional>

#include <cinerig/components/spline_roll.hpp>
#include <cinerig/core/spline_auto_dolly.hpp>
#include <cinerig/core/spline_settings.hpp>
#include <cinerig/engine/entity_world.hpp>

namespace cinerig
{

// Moves an entity transform along a spline. This is a caller-updated helper,
// not a camera component; each update method is gated by UpdateMethod.
class SplineCart
{
public:
    enum class UpdateMethod
    {
        Update = 0,
        FixedUpdate,
        LateUpdate,
    };

    SplineSettings spline{nullptr, 0.0f, PathIndexUnit::Normalized};
    const SplineRoll* splineRoll = nullptr;
    UpdateMethod updateMethod = UpdateMethod::Update;
    SplineAutoDolly automaticDolly{};
    EntityId trackingTarget = 0;

    void reset();
    void validate();
    bool isValid() const noexcept { return spline.spline != nullptr; }

    bool update(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);
    bool fixedUpdate(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);
    bool lateUpdate(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);

    bool setCartPosition(
        IEntityWorld& world,
        EntityId cartEntity,
        float position);

private:
    bool updateCartPosition(
        IEntityWorld& world,
        EntityId cartEntity,
        float deltaTime);
    std::optional<math::Vec3> trackingTargetPosition(
        const IEntityWorld& world) const;
};

} // namespace cinerig
