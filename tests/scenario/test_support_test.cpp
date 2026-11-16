#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"
#include "fixed_step.hpp"

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("FixedStep and FakeEntityWorld provide deterministic clocks", "[scenario][support]")
{
    FixedStep step;
    FakeEntityWorld world;
    world.setDeltaTime(UpdateClock::Fixed, step.dt);
    world.setTime(step.time);
    world.setFrame(UpdateClock::Fixed, static_cast<std::uint64_t>(step.frame));

    REQUIRE(step.frame == 0);
    REQUIRE(step.time == 0.0f);
    REQUIRE(world.frame(UpdateClock::Fixed) == 0);
    REQUIRE(world.time() == 0.0f);

    step.advance();
    world.setTime(step.time);
    world.setFrame(UpdateClock::Fixed, static_cast<std::uint64_t>(step.frame));

    REQUIRE(step.frame == 1);
    REQUIRE(step.time == Catch::Approx(step.dt));
    REQUIRE(world.frame(UpdateClock::Fixed) == 1);
    REQUIRE(world.time() == Catch::Approx(step.dt));
    REQUIRE(world.fixedDeltaTime() == Catch::Approx(step.dt));

    step.advance(3);
    REQUIRE(step.frame == 4);
    REQUIRE(step.time == Catch::Approx(step.dt * 4.0f));
    step.advance(0);
    REQUIRE(step.frame == 4);
    step.reset();
    REQUIRE(step.frame == 0);
    REQUIRE(step.time == 0.0f);
}

TEST_CASE("FakeEntityWorld supports transforms, writability and change clocks", "[scenario][support]")
{
    FakeEntityWorld world;
    world.add(7, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity(), {1.0f, 1.0f, 1.0f}});
    world.setLastChangeClock(7, UpdateClock::Fixed);

    REQUIRE(world.exists(7));
    REQUIRE_FALSE(world.exists(8));
    REQUIRE(world.transform(7).position.x == 1.0f);
    REQUIRE(world.lastChangeClock(7) == UpdateClock::Fixed);
    REQUIRE(world.lastChangeClock(8) == UpdateClock::Late);

    const Transform moved{{4.0f, 5.0f, 6.0f}, math::Quat::identity(), {2.0f, 2.0f, 2.0f}};
    REQUIRE(world.trySetTransform(7, moved));
    REQUIRE(world.transform(7).scale.x == 2.0f);
    REQUIRE(world.lastChangeClock(7) == UpdateClock::Late);

    world.setWritable(7, false);
    REQUIRE_FALSE(world.trySetTransform(7, Transform{}));
    REQUIRE(world.transform(7).position.x == 4.0f);

    world.setDefaultWritable(false);
    world.add(8, Transform{});
    REQUIRE_FALSE(world.trySetTransform(8, Transform{}));

    world.remove(7);
    REQUIRE_FALSE(world.exists(7));
    world.reset();
    REQUIRE_FALSE(world.exists(8));
    REQUIRE(world.time() == 0.0f);
}

TEST_CASE("FakeCameraOutput captures applied camera data", "[scenario][support]")
{
    FakeCameraOutput output;
    LensSettings inherited;
    inherited.fieldOfView = 35.0f;
    output.setOutputLens(inherited);
    output.setAspect(2.0f);
    REQUIRE(output.outputLens().fieldOfView == Catch::Approx(35.0f));
    REQUIRE(output.aspect() == Catch::Approx(2.0f));

    AppliedCamera camera;
    camera.state.rawPosition = {1.0f, 2.0f, 3.0f};
    camera.hasCamera = true;
    camera.aspect = 2.0f;
    output.apply(camera);

    REQUIRE(output.applyCount == 1);
    REQUIRE(output.applied.hasCamera);
    REQUIRE(output.applied.state.rawPosition.z == 3.0f);
    REQUIRE(output.history.size() == 1);

    output.apply(camera);
    REQUIRE(output.applyCount == 2);
    REQUIRE(output.history.size() == 2);

    output.reset();
    REQUIRE(output.applyCount == 0);
    REQUIRE(output.history.empty());
    REQUIRE_FALSE(output.applied.hasCamera);
}

TEST_CASE("FakePhysicsWorld records queries and returns configured hits", "[scenario][support]")
{
    FakePhysicsWorld physics;
    const QueryFilter filter;

    RaycastHit raycastHit;
    raycastHit.entity = 7;
    raycastHit.point = {1.0f, 0.0f, 0.0f};
    raycastHit.distance = 3.0f;
    physics.nextRaycast = raycastHit;

    const auto ray = physics.raycast(Ray{}, 10.0f, filter);
    REQUIRE(ray.has_value());
    REQUIRE(ray->entity == 7);
    REQUIRE(physics.raycastCalls == 1);
    REQUIRE(physics.lastMaxDistance == 10.0f);

    SphereCastHit sphereHit;
    sphereHit.radius = 0.5f;
    sphereHit.entity = 8;
    physics.nextSphereCast = sphereHit;
    const auto sphere = physics.sphereCast(Ray{}, 0.5f, 4.0f, filter);
    REQUIRE(sphere.has_value());
    REQUIRE(sphere->radius == 0.5f);
    REQUIRE(physics.sphereCastCalls == 1);
    REQUIRE(physics.lastSphereRadius == 0.5f);

    OverlapHit overlap;
    overlap.entity = 9;
    physics.nextOverlapSphere = {overlap};
    const auto overlaps = physics.overlapSphere({}, 2.0f, filter);
    REQUIRE(overlaps.size() == 1);
    REQUIRE(overlaps[0].entity == 9);
    REQUIRE(physics.overlapSphereCalls == 1);

    Bounds bounds;
    bounds.center = {5.0f, 0.0f, 0.0f};
    bounds.extents = {1.0f, 2.0f, 3.0f};
    physics.boundsById[9] = bounds;
    const auto entityBounds = physics.bounds(9);
    REQUIRE(entityBounds.has_value());
    REQUIRE(entityBounds->center.x == 5.0f);
    REQUIRE_FALSE(physics.bounds(10).has_value());
    REQUIRE(physics.boundsCalls == 2);

    physics.reset();
    REQUIRE_FALSE(physics.raycast(Ray{}, 1.0f, filter).has_value());
    REQUIRE(physics.raycastCalls == 1);
    REQUIRE(physics.overlapSphere({}, 1.0f, filter).empty());
    REQUIRE(physics.overlapSphereCalls == 1);
}
