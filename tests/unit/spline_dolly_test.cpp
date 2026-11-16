#include "fake_entity_world.hpp"

#include <cmath>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/spline_dolly.hpp>
#include <cinerig/core/bezier_spline.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/runtime/camera_system.hpp>

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

CameraState makeState()
{
    CameraState state = CameraState::makeDefault();
    state.referenceUp = math::Vec3::up;
    state.rawOrientation = math::Quat::identity();
    return state;
}

float forwardDot(const CameraState& state, math::Vec3 direction)
{
    return math::dot(
        math::rotate(state.rawOrientation, math::Vec3::forward),
        math::normalize(direction));
}

float upDot(const CameraState& state, math::Vec3 direction)
{
    return math::dot(
        math::rotate(state.rawOrientation, math::Vec3::up),
        math::normalize(direction));
}

struct SplineDollyFixture
{
    SplineDollyFixture()
    {
        world.add(
            target,
            Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("spline dolly");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        dolly = &camera->setBody<SplineDolly>();
        dolly->spline.spline = &track;
    }

    CameraState mutate(CameraState state, float deltaTime = 1.0f / 60.0f)
    {
        dolly->mutateCameraState(state, deltaTime);
        return state;
    }

    void setTarget(math::Vec3 position, math::Quat rotation = math::Quat::identity())
    {
        world.set(target, Transform{position, rotation, math::Vec3::one});
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    BezierSpline track = makeTrack();
    VirtualCamera* camera = nullptr;
    SplineDolly* dolly = nullptr;
    EntityId target = 1;
};

} // namespace

TEST_CASE("SplineDolly exposes original defaults and validation", "[unit][spline-dolly]")
{
    SplineDolly dolly;
    REQUIRE(dolly.stage() == Stage::Body);
    REQUIRE_FALSE(dolly.isValid());
    REQUIRE(dolly.spline.spline == nullptr);
    REQUIRE(dolly.spline.position == Catch::Approx(0.0f));
    REQUIRE(dolly.spline.units == PathIndexUnit::Normalized);
    REQUIRE(dolly.splineRoll == nullptr);
    REQUIRE(math::approximately(dolly.splineOffset, math::Vec3::zero));
    REQUIRE(dolly.cameraRotation == SplineDolly::RotationMode::Default);
    REQUIRE_FALSE(dolly.damping.enabled);
    REQUIRE(math::approximately(dolly.damping.position, math::Vec3::zero));
    REQUIRE(dolly.damping.angular == Catch::Approx(0.0f));
    REQUIRE_FALSE(dolly.automaticDolly.enabled);
    REQUIRE(dolly.automaticDolly.method == nullptr);
    REQUIRE_FALSE(dolly.automaticDolly.requiresTrackingTarget());
    REQUIRE(dolly.maxDampTime() == Catch::Approx(0.0f));

    dolly.damping.enabled = true;
    dolly.damping.position = {-1.0f, 25.0f, 7.0f};
    dolly.damping.angular = -2.0f;
    auto nearest = std::make_shared<SplineAutoDolly::NearestPointToTarget>();
    nearest->searchResolution = 0;
    nearest->searchIteration = 0;
    dolly.automaticDolly.method = nearest;
    dolly.validate();
    REQUIRE(math::approximately(dolly.damping.position, {0.0f, 20.0f, 7.0f}));
    REQUIRE(dolly.damping.angular == Catch::Approx(0.0f));
    REQUIRE(nearest->searchResolution == 1);
    REQUIRE(nearest->searchIteration == 1);
    REQUIRE(dolly.maxDampTime() == Catch::Approx(20.0f));
    REQUIRE(dolly.automaticDolly.requiresTrackingTarget());

    dolly.spline.units = PathIndexUnit::Distance;
    dolly.spline.position = 5.0f;
    dolly.splineOffset = {1.0f, 2.0f, 3.0f};
    dolly.cameraRotation = SplineDolly::RotationMode::Spline;
    dolly.reset();
    REQUIRE(dolly.spline.spline == nullptr);
    REQUIRE(dolly.spline.units == PathIndexUnit::Normalized);
    REQUIRE(dolly.spline.position == Catch::Approx(0.0f));
    REQUIRE(dolly.splineRoll == nullptr);
    REQUIRE(math::approximately(dolly.splineOffset, math::Vec3::zero));
    REQUIRE(dolly.cameraRotation == SplineDolly::RotationMode::Default);
    REQUIRE_FALSE(dolly.damping.enabled);
    REQUIRE_FALSE(dolly.automaticDolly.enabled);
    REQUIRE(dolly.automaticDolly.method == nullptr);
}

TEST_CASE("SplineDolly evaluates original distance positions", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.dolly->spline.units = PathIndexUnit::Distance;
    fixture.camera->previousStateIsValid = false;

    const auto positionAt = [&](float distance) {
        fixture.dolly->spline.position = distance;
        return fixture.mutate(makeState()).rawPosition;
    };

    REQUIRE(math::approximately(positionAt(0.0f), {7.0f, 1.0f, -6.0f}, 1e-4f));
    REQUIRE(math::approximately(positionAt(0.5f), {7.5f, 1.0f, -6.0f}, 1e-4f));
    REQUIRE(math::approximately(positionAt(6.0f), {13.0f, 1.0f, -6.0f}, 1e-4f));
    REQUIRE(math::approximately(positionAt(10.0f), {13.0f, 1.0f, -2.0f}, 1e-4f));
    REQUIRE(math::approximately(positionAt(15.0f), {11.0f, 1.0f, 1.0f}, 1e-4f));
    REQUIRE(math::approximately(positionAt(20.0f), {7.0f, 1.0f, 0.0f}, 1e-4f));
    REQUIRE(math::approximately(positionAt(26.0f), {7.0f, 1.0f, -6.0f}, 1e-4f));
}

TEST_CASE("SplineDolly evaluates normalized and knot positions", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.camera->previousStateIsValid = false;

    const auto positionAt = [&](float position, PathIndexUnit units) {
        fixture.dolly->spline.units = units;
        fixture.dolly->spline.position = position;
        return fixture.mutate(makeState()).rawPosition;
    };

    REQUIRE(math::approximately(
        positionAt(0.125f, PathIndexUnit::Normalized),
        {10.25f, 1.0f, -6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(0.5f, PathIndexUnit::Normalized),
        {13.0f, 1.0f, 1.0f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(0.75f, PathIndexUnit::Normalized),
        {7.0f, 1.0f, 0.5f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(1.0f, PathIndexUnit::Normalized),
        {7.0f, 1.0f, -6.0f},
        1e-4f));

    REQUIRE(math::approximately(
        positionAt(0.125f, PathIndexUnit::Knot),
        {7.75f, 1.0f, -6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(0.5f, PathIndexUnit::Knot),
        {10.0f, 1.0f, -6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(1.0f, PathIndexUnit::Knot),
        {13.0f, 1.0f, -6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(1.5f, PathIndexUnit::Knot),
        {13.0f, 1.0f, -2.5f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(1.75f, PathIndexUnit::Knot),
        {13.0f, 1.0f, -0.75f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(3.0f, PathIndexUnit::Knot),
        {7.0f, 1.0f, 1.0f},
        1e-4f));
    REQUIRE(math::approximately(
        positionAt(3.5f, PathIndexUnit::Knot),
        {7.0f, 1.0f, -2.5f},
        1e-4f));
}

TEST_CASE("SplineDolly fixed speed auto-dolly advances in position units", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.camera->previousStateIsValid = false;
    fixture.dolly->spline.units = PathIndexUnit::Normalized;
    fixture.dolly->automaticDolly.enabled = true;
    auto speed = std::make_shared<SplineAutoDolly::FixedSpeed>();
    speed->speed = 0.1f;
    fixture.dolly->automaticDolly.method = speed;

    CameraState state = fixture.mutate(makeState(), 0.5f);
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(0.05f));
    REQUIRE(math::approximately(state.rawPosition, {8.3f, 1.0f, -6.0f}, 1e-3f));

    state = fixture.mutate(state, 0.0f);
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(0.05f));

    state = fixture.mutate(state, -0.5f);
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(0.05f));
}

TEST_CASE("SplineDolly nearest-point auto-dolly uses target, offset, and units", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.camera->previousStateIsValid = false;
    fixture.dolly->spline.units = PathIndexUnit::Distance;
    fixture.dolly->automaticDolly.enabled = true;
    auto nearest = std::make_shared<SplineAutoDolly::NearestPointToTarget>();
    nearest->searchResolution = 32;
    nearest->searchIteration = 20;
    nearest->positionOffset = 1.0f;
    fixture.dolly->automaticDolly.method = nearest;

    fixture.setTarget({12.0f, 1.0f, -6.0f});
    CameraState state = fixture.mutate(makeState());
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(6.0f).margin(1e-2f));
    REQUIRE(math::approximately(state.rawPosition, {13.0f, 1.0f, -6.0f}, 1e-2f));

    fixture.world.remove(fixture.target);
    fixture.dolly->spline.position = 4.0f;
    state = fixture.mutate(state);
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(4.0f));
    REQUIRE(math::approximately(state.rawPosition, {11.0f, 1.0f, -6.0f}, 1e-3f));
}

TEST_CASE("SplineDolly applies offsets in spline-local axes", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.camera->previousStateIsValid = false;
    fixture.dolly->spline.units = PathIndexUnit::Normalized;
    fixture.dolly->spline.position = 0.0f;
    fixture.dolly->splineOffset = {2.0f, 3.0f, 4.0f};

    const CameraState state = fixture.mutate(makeState());
    REQUIRE(math::approximately(state.rawPosition, {11.0f, 4.0f, -4.0f}, 1e-4f));
}

TEST_CASE("SplineDolly rotation modes use spline, target, or incoming forward", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.dolly->spline.position = 0.0f;

    const auto rotate = [&](SplineDolly::RotationMode mode, CameraState state) {
        fixture.dolly->cameraRotation = mode;
        fixture.camera->previousStateIsValid = false;
        return fixture.mutate(state);
    };

    CameraState incoming = makeState();
    incoming.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, 0.6f);
    const math::Vec3 incomingForward =
        math::rotate(incoming.rawOrientation, math::Vec3::forward);
    CameraState state = rotate(SplineDolly::RotationMode::Default, incoming);
    REQUIRE(forwardDot(state, incomingForward) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(upDot(state, math::Vec3::up) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::up));

    state = rotate(SplineDolly::RotationMode::Spline, makeState());
    REQUIRE(forwardDot(state, math::Vec3::right) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(upDot(state, math::Vec3::up) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::up));

    state = makeState();
    state.referenceUp = math::Vec3::forward;
    state = rotate(SplineDolly::RotationMode::SplineNoRoll, state);
    REQUIRE(forwardDot(state, math::Vec3::right) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(upDot(state, math::Vec3::forward) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::forward));

    fixture.setTarget({0.0f, 0.0f, 0.0f});
    state = rotate(SplineDolly::RotationMode::FollowTarget, makeState());
    REQUIRE(math::approximately(state.rawOrientation, math::Quat::identity(), 1e-5f));
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::up));

    const math::Quat targetRotation =
        math::quatFromAxisAngle(math::Vec3::up, 0.4f);
    fixture.setTarget({0.0f, 0.0f, 0.0f}, targetRotation);
    state = rotate(SplineDolly::RotationMode::FollowTargetNoRoll, makeState());
    REQUIRE(math::approximately(state.rawOrientation, targetRotation, 1e-5f));
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::up));

    fixture.world.remove(fixture.target);
    state = makeState();
    state.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, 0.6f);
    state.referenceUp = math::Vec3::forward;
    state = rotate(SplineDolly::RotationMode::FollowTarget, state);
    REQUIRE(forwardDot(state, incomingForward) == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::forward));
}

TEST_CASE("SplineDolly damping snaps, follows, and uses the closed shortest path", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.dolly->spline.units = PathIndexUnit::Distance;
    fixture.dolly->damping.enabled = true;
    fixture.dolly->damping.position.z = 1.0f;
    fixture.camera->previousStateIsValid = false;

    CameraState state = fixture.mutate(makeState());
    fixture.camera->previousStateIsValid = true;

    fixture.dolly->spline.position = 10.0f;
    state = fixture.mutate(state);
    const float expected = math::standardDamp(10.0f, 1.0f, 1.0f / 60.0f);
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(expected).margin(1e-4f));
    REQUIRE(state.rawPosition.x == Catch::Approx(7.0f + expected).margin(1e-3f));

    for (int frame = 0; frame < 180; ++frame)
    {
        fixture.dolly->spline.position = 10.0f;
        state = fixture.mutate(state);
    }
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(10.0f).margin(1e-2f));

    fixture.dolly->spline.position = 24.0f;
    state = fixture.mutate(state, -1.0f);
    REQUIRE(fixture.dolly->spline.position == Catch::Approx(24.0f));
    REQUIRE(math::approximately(state.rawPosition, {7.0f, 1.0f, -4.0f}, 1e-3f));

    fixture.dolly->spline.position = 25.0f;
    fixture.camera->previousStateIsValid = false;
    state = fixture.mutate(makeState());
    fixture.camera->previousStateIsValid = true;
    fixture.dolly->spline.position = 1.0f;
    state = fixture.mutate(state);
    REQUIRE(fixture.dolly->spline.position < -0.5f);
    REQUIRE(fixture.dolly->spline.position > -1.0f);
    REQUIRE(state.rawPosition.z < -4.5f);
}

TEST_CASE("SplineDolly damps spline-local X and Y offset motion", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.dolly->spline.position = 0.0f;
    fixture.dolly->damping.enabled = true;
    fixture.dolly->damping.position.x = 1.0f;
    fixture.dolly->damping.position.y = 1.0f;
    fixture.camera->previousStateIsValid = false;

    CameraState state = fixture.mutate(makeState());
    fixture.camera->previousStateIsValid = true;
    fixture.dolly->splineOffset = {3.0f, 0.0f, 0.0f};
    state = fixture.mutate(state);
    REQUIRE(state.rawPosition.z > -6.0f);
    REQUIRE(state.rawPosition.z < -3.0f);

    fixture.dolly->splineOffset = {0.0f, 3.0f, 0.0f};
    state = fixture.mutate(state);
    REQUIRE(state.rawPosition.y > 1.0f);
    REQUIRE(state.rawPosition.y < 4.0f);
}

TEST_CASE("SplineDolly damps angular motion between spline frames", "[unit][spline-dolly]")
{
    SplineDollyFixture fixture;
    fixture.dolly->spline.units = PathIndexUnit::Distance;
    fixture.dolly->cameraRotation = SplineDolly::RotationMode::Spline;
    fixture.dolly->damping.enabled = true;
    fixture.dolly->damping.angular = 1.0f;
    fixture.camera->previousStateIsValid = false;

    CameraState state = fixture.mutate(makeState());
    const math::Vec3 startForward =
        math::rotate(state.rawOrientation, math::Vec3::forward);
    fixture.camera->previousStateIsValid = true;
    fixture.dolly->spline.position = 10.0f;
    state = fixture.mutate(state);

    const math::Vec3 finalForward =
        math::rotate(state.rawOrientation, math::Vec3::forward);
    REQUIRE(math::dot(startForward, math::Vec3::right)
        == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(math::dot(finalForward, math::Vec3::back) > 0.0f);
    REQUIRE(math::dot(finalForward, math::Vec3::back) < 0.4f);
    REQUIRE(math::approximately(state.referenceUp, math::Vec3::up));
}
