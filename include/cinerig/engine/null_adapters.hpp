#pragma once

#include <cinerig/engine/animation_state.hpp>
#include <cinerig/engine/camera_output.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/engine/input_source.hpp>
#include <cinerig/engine/physics_world.hpp>
#include <cinerig/engine/post_processor.hpp>
#include <cinerig/engine/spline.hpp>

namespace cinerig
{

// Safe no-op adapters for optional engine services. They make it possible to
// wire the runtime without null checks.

struct NullPhysicsWorld final : IPhysicsWorld
{
    std::optional<RaycastHit> raycast(
        const Ray&, float, const QueryFilter&) const override
    {
        return std::nullopt;
    }

    std::optional<SphereCastHit> sphereCast(
        const Ray&, float, float, const QueryFilter&) const override
    {
        return std::nullopt;
    }

    std::vector<OverlapHit> overlapSphere(
        math::Vec3, float, const QueryFilter&) const override
    {
        return {};
    }

    std::optional<Bounds> bounds(EntityId) const override
    {
        return std::nullopt;
    }
};

struct NullInputSource final : IInputSource
{
    float axis(std::string_view) const override { return 0.0f; }
    math::Vec2 axis2D(std::string_view) const override { return {}; }
};

struct NullPostProcessor final : IPostProcessor
{
    void apply(const AppliedCamera&) override {}
};

}
