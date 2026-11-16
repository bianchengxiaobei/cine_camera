#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/third_person_follow.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct ThirdPersonFixture
{
    ThirdPersonFixture()
    {
        world.add(target, Transform{});
        const CameraId id = system.createCamera<VirtualCamera>("third person");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        follow = &camera->setBody<ThirdPersonFollow>();
    }

    void setTarget(
        math::Vec3 position,
        math::Quat rotation = math::Quat::identity())
    {
        world.set(target, Transform{position, rotation, math::Vec3::one});
    }

    void step(float deltaTime = 1.0f / 60.0f)
    {
        time += deltaTime;
        world.setTime(time);
        camera->updateCameraState(math::Vec3::up, deltaTime);
    }

    FakeEntityWorld world;
    FakePhysicsWorld physics;
    CameraSystem system{world, &physics};
    VirtualCamera* camera = nullptr;
    ThirdPersonFollow* follow = nullptr;
    EntityId target = 1;
    float time = 0.0f;
};

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

} // namespace

TEST_CASE("ThirdPersonFollow exposes the original rig defaults", "[unit][third-person-follow]")
{
    ThirdPersonFollow follow;
    REQUIRE(follow.damping.x == Catch::Approx(0.1f));
    REQUIRE(follow.damping.y == Catch::Approx(0.5f));
    REQUIRE(follow.damping.z == Catch::Approx(0.3f));
    REQUIRE(follow.shoulderOffset.x == Catch::Approx(0.5f));
    REQUIRE(follow.shoulderOffset.y == Catch::Approx(-0.4f));
    REQUIRE(follow.shoulderOffset.z == Catch::Approx(0.0f));
    REQUIRE(follow.verticalArmLength == Catch::Approx(0.4f));
    REQUIRE(follow.cameraSide == Catch::Approx(1.0f));
    REQUIRE(follow.cameraDistance == Catch::Approx(2.0f));
    REQUIRE_FALSE(follow.avoidObstacles.enabled);
    REQUIRE(follow.avoidObstacles.collisionFilter.bits == 1u);
    REQUIRE(follow.avoidObstacles.cameraRadius == Catch::Approx(0.2f));
    REQUIRE(follow.avoidObstacles.dampingFromCollision == Catch::Approx(0.5f));
    REQUIRE(follow.maxDampTime() == Catch::Approx(0.5f));
    REQUIRE(follow.stage() == Stage::Body);
}

TEST_CASE("ThirdPersonFollow places the rig behind the target", "[unit][third-person-follow]")
{
    ThirdPersonFixture fixture;
    fixture.setTarget({1.0f, 2.0f, 3.0f});
    fixture.step();

    REQUIRE(fixture.follow->isValid());
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {1.5f, 2.0f, 5.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        math::Quat::identity(),
        1e-4f));

    fixture.follow->cameraDistance = 4.0f;
    fixture.follow->cameraSide = 0.0f;
    fixture.follow->shoulderOffset = {1.0f, -0.2f, 0.0f};
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {0.0f, 2.2f, 7.0f},
        1e-4f));
}

TEST_CASE("ThirdPersonFollow follows target yaw and projects the heading", "[unit][third-person-follow]")
{
    ThirdPersonFixture fixture;
    fixture.setTarget({}, yaw(90.0f));
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {2.0f, 0.0f, -0.5f},
        1e-4f));
    REQUIRE(math::approximately(
        math::rotate(fixture.camera->state().rawOrientation, math::Vec3::forward),
        {-1.0f, 0.0f, 0.0f},
        1e-4f));

    const math::Quat heading = ThirdPersonFollow::getHeading(
        yaw(90.0f),
        math::Vec3::up);
    REQUIRE(math::approximately(
        math::rotate(heading, math::Vec3::forward),
        {-1.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("ThirdPersonFollow damps target motion and resets on invalid state", "[unit][third-person-follow]")
{
    ThirdPersonFixture fixture;
    fixture.setTarget({});
    fixture.step();

    fixture.setTarget({10.0f, 0.0f, 0.0f});
    fixture.step();
    REQUIRE(fixture.camera->state().rawPosition.x < 10.0f);
    REQUIRE(fixture.camera->state().rawPosition.x > 0.0f);

    for (int frame = 0; frame < 120; ++frame)
        fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {10.5f, 0.0f, 2.0f},
        1e-3f));

    fixture.setTarget({20.0f, 0.0f, 0.0f});
    fixture.camera->updateCameraState(math::Vec3::up, -1.0f);
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {20.5f, 0.0f, 2.0f},
        1e-4f));
}

TEST_CASE("ThirdPersonFollow resolves optional hand and camera sphere casts", "[unit][third-person-follow]")
{
    ThirdPersonFixture fixture;
    fixture.setTarget({});
    fixture.step();

    fixture.follow->avoidObstacles.enabled = true;
    fixture.follow->avoidObstacles.ignoreEntity = fixture.target;
    SphereCastHit hit;
    hit.entity = 99;
    hit.point = {0.2f, 0.0f, 0.0f};
    hit.normal = {-1.0f, 0.0f, 0.0f};
    hit.distance = 0.2f;
    hit.radius = 0.21f;
    fixture.physics.nextSphereCast = hit;

    math::Vec3 root{};
    math::Vec3 shoulder{};
    math::Vec3 hand{};
    fixture.follow->getRigPositions(root, shoulder, hand);
    REQUIRE(math::approximately(root, {}, 1e-4f));
    REQUIRE(math::approximately(hand, {-0.01f, 0.0f, 0.0f}, 1e-3f));
    REQUIRE(fixture.physics.lastSphereRadius == Catch::Approx(0.21f));
    REQUIRE(fixture.physics.lastFilter.ignoreEntity == fixture.target);
    REQUIRE(fixture.physics.lastFilter.layers.bits == 1u);
}

TEST_CASE("ThirdPersonFollow corrects first-person look-at and target warps", "[unit][third-person-follow]")
{
    ThirdPersonFixture fixture;
    fixture.setTarget({});
    fixture.follow->shoulderOffset = {};
    fixture.follow->verticalArmLength = 0.0f;
    fixture.follow->cameraDistance = 0.0f;
    fixture.step();

    REQUIRE(hasBlendHint(
        fixture.camera->state().blendHint,
        BlendHint::IgnoreLookAtTarget));
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        {0.0f, 0.0f, -0.01f},
        1e-4f));

    fixture.follow->cameraDistance = 2.0f;
    fixture.setTarget({10.0f, 0.0f, 0.0f});
    for (int frame = 0; frame < 120; ++frame)
        fixture.step();
    fixture.setTarget({20.0f, 0.0f, 0.0f});
    fixture.camera->onTargetObjectWarped(fixture.target, {10.0f, 0.0f, 0.0f});
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {20.0f, 0.0f, 2.0f},
        1e-3f));
}

TEST_CASE("ThirdPersonFollow validates settings and reports collision damping", "[unit][third-person-follow]")
{
    ThirdPersonFollow follow;
    follow.cameraSide = 2.0f;
    follow.damping = {-1.0f, 0.25f, -3.0f};
    follow.avoidObstacles.cameraRadius = 0.0f;
    follow.avoidObstacles.dampingIntoCollision = 2.0f;
    follow.avoidObstacles.dampingFromCollision = 3.0f;
    follow.avoidObstacles.enabled = true;
    follow.validate();

    REQUIRE(follow.cameraSide == 1.0f);
    REQUIRE(follow.damping.x == 0.0f);
    REQUIRE(follow.damping.y == 0.25f);
    REQUIRE(follow.damping.z == 0.0f);
    REQUIRE(follow.avoidObstacles.cameraRadius == Catch::Approx(0.001f));
    REQUIRE(follow.maxDampTime() == Catch::Approx(3.0f));
}




