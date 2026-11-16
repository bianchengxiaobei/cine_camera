#include "fake_entity_world.hpp"

#include <cmath>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/spline_cart.hpp>
#include <cinerig/core/bezier_spline.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BezierSpline makeTrack()
{
    return BezierSpline::linear(
        {
            {7.0f, 1.0f, -6.0f},
            {13.0f, 1.0f, -6.0f},
            {13.0f, 1.0f, 1.0f},
            {7.0f, 1.0f, 1.0f},
        },
        true);
}

struct CartFixture
{
    CartFixture()
    {
        world.add(
            cartEntity,
            Transform{
                {0.0f, 0.0f, 0.0f},
                math::Quat::identity(),
                {2.0f, 3.0f, 4.0f}});
        world.add(
            targetEntity,
            Transform{
                {0.0f, 0.0f, 0.0f},
                math::Quat::identity(),
                math::Vec3::one});
        cart.spline.spline = &track;
        cart.trackingTarget = targetEntity;
    }

    void setTarget(math::Vec3 position)
    {
        world.set(
            targetEntity,
            Transform{position, math::Quat::identity(), math::Vec3::one});
    }

    FakeEntityWorld world;
    BezierSpline track = makeTrack();
    SplineCart cart;
    EntityId cartEntity = 1;
    EntityId targetEntity = 2;
};

} // namespace

TEST_CASE("SplineCart exposes original defaults and reset", "[unit][spline-cart]")
{
    SplineCart cart;
    REQUIRE_FALSE(cart.isValid());
    REQUIRE(cart.spline.spline == nullptr);
    REQUIRE(cart.spline.position == Catch::Approx(0.0f));
    REQUIRE(cart.spline.units == PathIndexUnit::Normalized);
    REQUIRE(cart.splineRoll == nullptr);
    REQUIRE(cart.updateMethod == SplineCart::UpdateMethod::Update);
    REQUIRE_FALSE(cart.automaticDolly.enabled);
    REQUIRE(cart.automaticDolly.method == nullptr);
    REQUIRE(cart.trackingTarget == 0);

    auto nearest = std::make_shared<SplineAutoDolly::NearestPointToTarget>();
    nearest->searchResolution = 0;
    nearest->searchIteration = 0;
    cart.automaticDolly.method = nearest;
    cart.validate();
    REQUIRE(nearest->searchResolution == 1);
    REQUIRE(nearest->searchIteration == 1);

    cart.spline.units = PathIndexUnit::Distance;
    cart.spline.position = 12.0f;
    cart.updateMethod = SplineCart::UpdateMethod::LateUpdate;
    cart.automaticDolly.enabled = true;
    cart.trackingTarget = 7;
    cart.reset();
    REQUIRE(cart.spline.spline == nullptr);
    REQUIRE(cart.spline.position == Catch::Approx(0.0f));
    REQUIRE(cart.spline.units == PathIndexUnit::Normalized);
    REQUIRE(cart.splineRoll == nullptr);
    REQUIRE(cart.updateMethod == SplineCart::UpdateMethod::Update);
    REQUIRE_FALSE(cart.automaticDolly.enabled);
    REQUIRE(cart.automaticDolly.method == nullptr);
    REQUIRE(cart.trackingTarget == 0);
}

TEST_CASE("SplineCart writes original positions and preserves scale", "[unit][spline-cart]")
{
    CartFixture fixture;
    fixture.cart.spline.units = PathIndexUnit::Distance;

    REQUIRE(fixture.cart.setCartPosition(fixture.world, fixture.cartEntity, 0.0f));
    Transform transform = fixture.world.transform(fixture.cartEntity);
    REQUIRE(math::approximately(transform.position, {7.0f, 1.0f, -6.0f}, 1e-4f));
    REQUIRE(math::approximately(transform.scale, {2.0f, 3.0f, 4.0f}));
    REQUIRE(math::dot(
        math::rotate(transform.rotation, math::Vec3::forward),
        math::Vec3::right) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::dot(
        math::rotate(transform.rotation, math::Vec3::up),
        math::Vec3::up) == Catch::Approx(1.0f).margin(1e-4f));

    REQUIRE(fixture.cart.setCartPosition(fixture.world, fixture.cartEntity, 10.0f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {13.0f, 1.0f, -2.0f},
        1e-4f));

    fixture.cart.spline.units = PathIndexUnit::Normalized;
    REQUIRE(fixture.cart.setCartPosition(fixture.world, fixture.cartEntity, 1.0f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {7.0f, 1.0f, -6.0f},
        1e-4f));

    fixture.cart.spline.position = 0.5f;
    REQUIRE(fixture.cart.setCartPosition(fixture.world, fixture.cartEntity, 0.5f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {13.0f, 1.0f, 1.0f},
        1e-4f));
}

TEST_CASE("SplineCart update methods are gated by UpdateMethod", "[unit][spline-cart]")
{
    CartFixture fixture;
    fixture.cart.spline.position = 0.0f;

    REQUIRE(fixture.cart.update(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE_FALSE(fixture.cart.fixedUpdate(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE_FALSE(fixture.cart.lateUpdate(fixture.world, fixture.cartEntity, 1.0f / 60.0f));

    fixture.cart.updateMethod = SplineCart::UpdateMethod::FixedUpdate;
    fixture.cart.spline.position = 0.5f;
    REQUIRE_FALSE(fixture.cart.update(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE(fixture.cart.fixedUpdate(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {13.0f, 1.0f, 1.0f},
        1e-4f));

    fixture.cart.updateMethod = SplineCart::UpdateMethod::LateUpdate;
    fixture.cart.spline.position = 0.0f;
    REQUIRE_FALSE(fixture.cart.update(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE_FALSE(fixture.cart.fixedUpdate(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE(fixture.cart.lateUpdate(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {7.0f, 1.0f, -6.0f},
        1e-4f));
}

TEST_CASE("SplineCart fixed speed moves in the selected units", "[unit][spline-cart]")
{
    CartFixture fixture;
    fixture.cart.spline.units = PathIndexUnit::Normalized;
    fixture.cart.automaticDolly.enabled = true;
    auto speed = std::make_shared<SplineAutoDolly::FixedSpeed>();
    speed->speed = 0.1f;
    fixture.cart.automaticDolly.method = speed;

    REQUIRE(fixture.cart.update(fixture.world, fixture.cartEntity, 0.5f));
    REQUIRE(fixture.cart.spline.position == Catch::Approx(0.05f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {8.3f, 1.0f, -6.0f},
        1e-3f));

    REQUIRE(fixture.cart.update(fixture.world, fixture.cartEntity, 0.0f));
    REQUIRE(fixture.cart.spline.position == Catch::Approx(0.05f));
    REQUIRE(fixture.cart.update(fixture.world, fixture.cartEntity, -0.5f));
    REQUIRE(fixture.cart.spline.position == Catch::Approx(0.05f));
}

TEST_CASE("SplineCart nearest-point auto-dolly uses its tracking target", "[unit][spline-cart]")
{
    CartFixture fixture;
    fixture.cart.spline.units = PathIndexUnit::Distance;
    fixture.cart.automaticDolly.enabled = true;
    auto nearest = std::make_shared<SplineAutoDolly::NearestPointToTarget>();
    nearest->searchResolution = 32;
    nearest->searchIteration = 20;
    nearest->positionOffset = 1.0f;
    fixture.cart.automaticDolly.method = nearest;
    fixture.setTarget({12.0f, 1.0f, -6.0f});

    REQUIRE(fixture.cart.update(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE(fixture.cart.spline.position == Catch::Approx(6.0f).margin(1e-2f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {13.0f, 1.0f, -6.0f},
        1e-2f));

    fixture.world.remove(fixture.targetEntity);
    fixture.cart.spline.position = 4.0f;
    REQUIRE(fixture.cart.update(fixture.world, fixture.cartEntity, 1.0f / 60.0f));
    REQUIRE(fixture.cart.spline.position == Catch::Approx(4.0f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        {11.0f, 1.0f, -6.0f},
        1e-3f));
}

TEST_CASE("SplineCart handles missing entities and read-only worlds", "[unit][spline-cart]")
{
    CartFixture fixture;
    fixture.cart.spline.units = PathIndexUnit::Normalized;
    fixture.cart.spline.position = 0.5f;

    fixture.world.setWritable(fixture.cartEntity, false);
    const Transform before = fixture.world.transform(fixture.cartEntity);
    REQUIRE_FALSE(fixture.cart.setCartPosition(
        fixture.world,
        fixture.cartEntity,
        0.0f));
    REQUIRE(math::approximately(
        fixture.world.transform(fixture.cartEntity).position,
        before.position,
        1e-5f));

    fixture.world.setWritable(fixture.cartEntity, true);
    REQUIRE_FALSE(fixture.cart.setCartPosition(fixture.world, 999, 0.0f));
    fixture.cart.spline.spline = nullptr;
    REQUIRE_FALSE(fixture.cart.setCartPosition(
        fixture.world,
        fixture.cartEntity,
        0.0f));
    REQUIRE_FALSE(fixture.cart.update(
        fixture.world,
        fixture.cartEntity,
        1.0f / 60.0f));
}
