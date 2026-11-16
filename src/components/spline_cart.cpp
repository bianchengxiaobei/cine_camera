#include <cinerig/components/spline_cart.hpp>

#include <cinerig/math/quat.hpp>

namespace cinerig
{

void SplineCart::reset()
{
    spline = {nullptr, 0.0f, PathIndexUnit::Normalized};
    splineRoll = nullptr;
    updateMethod = UpdateMethod::Update;
    automaticDolly = {};
    trackingTarget = 0;
}

void SplineCart::validate()
{
    automaticDolly.validate();
}

bool SplineCart::update(
    IEntityWorld& world,
    EntityId cartEntity,
    float deltaTime)
{
    if (updateMethod != UpdateMethod::Update)
        return false;
    return updateCartPosition(world, cartEntity, deltaTime);
}

bool SplineCart::fixedUpdate(
    IEntityWorld& world,
    EntityId cartEntity,
    float deltaTime)
{
    if (updateMethod != UpdateMethod::FixedUpdate)
        return false;
    return updateCartPosition(world, cartEntity, deltaTime);
}

bool SplineCart::lateUpdate(
    IEntityWorld& world,
    EntityId cartEntity,
    float deltaTime)
{
    if (updateMethod != UpdateMethod::LateUpdate)
        return false;
    return updateCartPosition(world, cartEntity, deltaTime);
}

bool SplineCart::setCartPosition(
    IEntityWorld& world,
    EntityId cartEntity,
    float position)
{
    if (spline.spline == nullptr || cartEntity == 0 || !world.exists(cartEntity))
        return false;

    const ISpline& track = *spline.spline;
    spline.position = position;
    spline.position = spline.standardizedPosition();
    const float normalized = spline.normalizedPosition();
    const math::Vec3 tangent = track.evaluateTangent(normalized);
    const math::Vec3 up = track.evaluateUp(normalized);
    math::Quat rotation = math::lookRotation(tangent, up);
    if (splineRoll != nullptr)
        rotation = splineRoll->apply(rotation, normalized);

    Transform transform = world.transform(cartEntity);
    transform.position = track.evaluatePosition(normalized);
    transform.rotation = rotation;
    return world.trySetTransform(cartEntity, transform);
}

bool SplineCart::updateCartPosition(
    IEntityWorld& world,
    EntityId cartEntity,
    float deltaTime)
{
    if (spline.spline == nullptr)
        return false;

    float position = spline.position;
    if (automaticDolly.enabled && automaticDolly.method)
    {
        position = automaticDolly.getSplinePosition(
            *spline.spline,
            trackingTargetPosition(world),
            position,
            spline.units,
            deltaTime);
    }
    return setCartPosition(world, cartEntity, position);
}

std::optional<math::Vec3> SplineCart::trackingTargetPosition(
    const IEntityWorld& world) const
{
    if (trackingTarget == 0 || !world.exists(trackingTarget))
        return std::nullopt;
    return world.transform(trackingTarget).position;
}

} // namespace cinerig
