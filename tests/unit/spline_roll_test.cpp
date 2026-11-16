#include "fake_entity_world.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/spline_cart.hpp>
#include <cinerig/components/spline_dolly.hpp>
#include <cinerig/components/spline_roll.hpp>
#include <cinerig/core/bezier_spline.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

SplineRoll makeRoll()
{
    SplineRoll roll;
    roll.easing = false;
    roll.roll = {
        {0.0f, -10.0f},
        {0.5f, 30.0f},
        {1.0f, 0.0f},
    };
    return roll;
}

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

CameraState makeState()
{
    CameraState state = CameraState::makeDefault();
    state.referenceUp = math::Vec3::up;
    return state;
}

} // namespace

TEST_CASE("SplineRoll defaults, reset, and empty application", "[unit][spline-roll]")
{
    SplineRoll roll;
    REQUIRE(roll.easing);
    REQUIRE(roll.empty());
    REQUIRE(roll.evaluate(0.5f) == Catch::Approx(0.0f));

    const math::Quat base = math::lookRotation(
        math::Vec3::right,
        math::Vec3::up);
    REQUIRE(math::approximately(roll.apply(base, 0.25f), base, 1e-6f));

    roll.easing = false;
    roll.roll = {{0.5f, 12.0f}};
    roll.reset();
    REQUIRE(roll.easing);
    REQUIRE(roll.empty());
}

TEST_CASE("SplineRoll interpolates linearly and clamps endpoints", "[unit][spline-roll]")
{
    SplineRoll roll = makeRoll();

    REQUIRE(roll.evaluate(-0.5f) == Catch::Approx(-10.0f));
    REQUIRE(roll.evaluate(0.0f) == Catch::Approx(-10.0f));
    REQUIRE(roll.evaluate(0.25f) == Catch::Approx(10.0f));
    REQUIRE(roll.evaluate(0.5f) == Catch::Approx(30.0f));
    REQUIRE(roll.evaluate(0.75f) == Catch::Approx(15.0f));
    REQUIRE(roll.evaluate(1.0f) == Catch::Approx(0.0f));
    REQUIRE(roll.evaluate(1.5f) == Catch::Approx(0.0f));
    REQUIRE(roll.evaluate(std::nanf("")) == Catch::Approx(-10.0f));
}

TEST_CASE("SplineRoll easing uses smoothstep between keys", "[unit][spline-roll]")
{
    SplineRoll roll = makeRoll();
    roll.easing = true;

    REQUIRE(roll.evaluate(0.0f) == Catch::Approx(-10.0f));
    REQUIRE(roll.evaluate(0.125f) == Catch::Approx(-3.75f).margin(1e-4f));
    REQUIRE(roll.evaluate(0.25f) == Catch::Approx(10.0f));
    REQUIRE(roll.evaluate(0.5f) == Catch::Approx(30.0f));
    REQUIRE(roll.evaluate(0.625f) == Catch::Approx(25.3125f).margin(1e-4f));
}

TEST_CASE("SplineRoll validation clamps and sorts keys", "[unit][spline-roll]")
{
    SplineRoll roll;
    roll.roll = {
        {1.5f, 20.0f},
        {-0.2f, 5.0f},
        {0.4f, std::nanf("")},
    };
    roll.validate();

    REQUIRE(roll.roll.size() == 3);
    REQUIRE(roll.roll[0].position == Catch::Approx(0.0f));
    REQUIRE(roll.roll[0].value == Catch::Approx(5.0f));
    REQUIRE(roll.roll[1].position == Catch::Approx(0.4f));
    REQUIRE(roll.roll[1].value == Catch::Approx(0.0f));
    REQUIRE(roll.roll[2].position == Catch::Approx(1.0f));
    REQUIRE(roll.roll[2].value == Catch::Approx(20.0f));
}

TEST_CASE("SplineRoll application preserves forward and changes up", "[unit][spline-roll]")
{
    SplineRoll roll;
    roll.easing = false;
    roll.roll = {{0.0f, 0.0f}, {1.0f, 90.0f}};

    const math::Quat base = math::lookRotation(
        math::Vec3::right,
        math::Vec3::up);
    const math::Quat rolled = roll.apply(base, 1.0f);

    REQUIRE(math::angle(base, rolled) == Catch::Approx(math::Pi * 0.5f).margin(1e-4f));
    REQUIRE(math::dot(
        math::rotate(rolled, math::Vec3::forward),
        math::Vec3::right) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::dot(
        math::rotate(rolled, math::Vec3::up),
        math::Vec3::up) == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("SplineDolly applies roll before offset axes and reference up", "[unit][spline-roll]")
{
    FakeEntityWorld world;
    CameraSystem system{world};
    const CameraId id = system.createCamera<VirtualCamera>("roll dolly");
    VirtualCamera* camera = system.camera(id);
    SplineDolly& dolly = camera->setBody<SplineDolly>();
    BezierSpline track = makeTrack();
    SplineRoll roll;
    roll.easing = false;
    roll.roll = {{0.0f, 0.0f}, {1.0f, 90.0f}};

    dolly.spline.spline = &track;
    dolly.spline.units = PathIndexUnit::Normalized;
    dolly.spline.position = 0.5f;
    dolly.splineRoll = &roll;
    dolly.splineOffset = {1.0f, 0.0f, 0.0f};
    dolly.cameraRotation = SplineDolly::RotationMode::Spline;
    camera->previousStateIsValid = false;

    const float normalized = 0.5f;
    const math::Quat base = math::lookRotation(
        track.evaluateTangent(normalized),
        track.evaluateUp(normalized));
    const math::Quat expectedRotation = roll.apply(base, normalized);
    const math::Vec3 expectedPosition =
        track.evaluatePosition(normalized)
        + math::rotate(expectedRotation, math::Vec3::right);

    CameraState state = makeState();
    dolly.mutateCameraState(state, 1.0f / 60.0f);

    REQUIRE(math::approximately(state.rawPosition, expectedPosition, 1e-4f));
    REQUIRE(math::approximately(state.rawOrientation, expectedRotation, 1e-4f));
    REQUIRE(math::approximately(
        state.referenceUp,
        math::rotate(expectedRotation, math::Vec3::up),
        1e-4f));
}

TEST_CASE("SplineCart writes the rolled frame", "[unit][spline-roll]")
{
    FakeEntityWorld world;
    const EntityId cartEntity = 1;
    world.add(
        cartEntity,
        Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    BezierSpline track = makeTrack();
    SplineRoll roll;
    roll.easing = false;
    roll.roll = {{0.0f, 0.0f}, {1.0f, 90.0f}};

    SplineCart cart;
    cart.spline.spline = &track;
    cart.spline.units = PathIndexUnit::Normalized;
    cart.splineRoll = &roll;

    REQUIRE(cart.setCartPosition(world, cartEntity, 0.5f));
    const math::Quat base = math::lookRotation(
        track.evaluateTangent(0.5f),
        track.evaluateUp(0.5f));
    REQUIRE(math::approximately(
        world.transform(cartEntity).rotation,
        roll.apply(base, 0.5f),
        1e-4f));
}
