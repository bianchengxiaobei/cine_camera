#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/decollider.hpp>
#include <cinerig/math/geometry.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct DecolliderFixture
{
    DecolliderFixture()
    {
        world.add(
            follow,
            Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(
            lookAt,
            Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("decollider");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(follow);
        camera->targets.lookAt = TargetRef::entity(lookAt);
        camera->targets.customLookAt = true;
    }

    CameraState makeState(
        math::Vec3 cameraPosition = {0.0f, 1.6f, 8.0f},
        math::Vec3 targetPoint = {0.0f, 1.5f, 0.0f}) const
    {
        CameraState state = CameraState::makeDefault();
        state.rawPosition = cameraPosition;
        state.rawOrientation = math::lookRotation(
            targetPoint - cameraPosition,
            math::Vec3::up);
        state.referenceLookAt = targetPoint;
        state.referenceUp = math::Vec3::up;
        return state;
    }

    void body(CameraState& state, float deltaTime = 1.0f / 60.0f)
    {
        decollider.postPipelineStageCallback(*camera, Stage::Body, state, deltaTime);
    }

    FakeEntityWorld world;
    FakePhysicsWorld physics;
    CameraSystem system{world, &physics};
    VirtualCamera* camera = nullptr;
    Decollider decollider;
    EntityId follow = 1;
    EntityId lookAt = 2;
};

void addOverlap(
    FakePhysicsWorld& physics,
    EntityId entity,
    math::Vec3 cameraPosition,
    math::Vec3 extents)
{
    physics.nextOverlapSphere.push_back(OverlapHit{entity, cameraPosition, 0.0f});
    physics.boundsById[entity] = Bounds{cameraPosition, extents};
}

} // namespace

TEST_CASE("Decollider exposes original defaults and validation", "[unit][decollider]")
{
    Decollider decollider;
    REQUIRE(decollider.cameraRadius == Catch::Approx(0.4f));
    REQUIRE_FALSE(decollider.decollision.enabled);
    REQUIRE(decollider.decollision.obstacleLayers.bits == 1u);
    REQUIRE(decollider.decollision.damping == Catch::Approx(0.5f));
    REQUIRE(decollider.decollision.smoothingTime == 0.0f);
    REQUIRE(decollider.terrainResolution.enabled);
    REQUIRE(decollider.terrainResolution.terrainLayers.bits == 1u);
    REQUIRE(decollider.terrainResolution.maximumRaycast == Catch::Approx(10.0f));
    REQUIRE(decollider.terrainResolution.damping == Catch::Approx(0.5f));
    REQUIRE(decollider.maxDampTime() == Catch::Approx(0.5f));

    decollider.cameraRadius = 0.0f;
    decollider.decollision.damping = -1.0f;
    decollider.decollision.smoothingTime = 3.0f;
    decollider.terrainResolution.maximumRaycast = -1.0f;
    decollider.terrainResolution.damping = 11.0f;
    decollider.validate();
    REQUIRE(decollider.cameraRadius == Catch::Approx(0.01f));
    REQUIRE(decollider.decollision.damping == 0.0f);
    REQUIRE(decollider.decollision.smoothingTime == Catch::Approx(2.0f));
    REQUIRE(decollider.terrainResolution.maximumRaycast == 0.0f);
    REQUIRE(decollider.terrainResolution.damping == Catch::Approx(10.0f));

    decollider.terrainResolution.enabled = false;
    REQUIRE(decollider.maxDampTime() == 0.0f);
    decollider.reset();
    REQUIRE(decollider.terrainResolution.enabled);
    REQUIRE(decollider.terrainResolution.maximumRaycast == Catch::Approx(10.0f));
    REQUIRE(decollider.cameraRadius == Catch::Approx(0.4f));
}

TEST_CASE("Decollider lifts the camera above terrain", "[unit][decollider]")
{
    DecolliderFixture fixture;
    SphereCastHit hit;
    hit.entity = 9;
    hit.distance = 9.6f;
    fixture.physics.nextSphereCast = hit;

    CameraState state = fixture.makeState();
    fixture.body(state);

    REQUIRE(fixture.physics.sphereCastCalls == 1);
    REQUIRE(fixture.physics.lastRay.origin.y == Catch::Approx(11.6f).margin(1e-4f));
    REQUIRE(fixture.physics.lastRay.direction.y == Catch::Approx(-1.0f).margin(1e-4f));
    REQUIRE(fixture.physics.lastSphereRadius == Catch::Approx(0.4001f).margin(1e-4f));
    REQUIRE(fixture.physics.lastMaxDistance == Catch::Approx(10.0f));
    REQUIRE(fixture.physics.lastFilter.layers.bits == 1u);
    REQUIRE(state.correctedPosition().y == Catch::Approx(2.0001f).margin(1e-4f));
    REQUIRE(state.rawPosition.y == Catch::Approx(1.6f));
}

TEST_CASE("Decollider damps falling terrain height", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.camera->previousStateIsValid = true;
    SphereCastHit hit;
    hit.distance = 9.0f;
    fixture.physics.nextSphereCast = hit;
    CameraState first = fixture.makeState();
    fixture.body(first);
    REQUIRE(first.correctedPosition().y == Catch::Approx(2.6001f).margin(1e-4f));

    hit.distance = 9.5f;
    fixture.physics.nextSphereCast = hit;
    CameraState second = fixture.makeState();
    fixture.body(second);
    REQUIRE(second.correctedPosition().y > 2.1f);
    REQUIRE(second.correctedPosition().y < 2.6001f);
}

TEST_CASE("Decollider pushes the camera out of overlapping obstacles", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.decollider.terrainResolution.enabled = false;
    fixture.decollider.decollision.enabled = true;
    fixture.decollider.decollision.obstacleLayers = LayerMask{0b10u};
    addOverlap(
        fixture.physics,
        5,
        {0.0f, 1.6f, 8.0f},
        {0.5f, 0.5f, 0.2f});

    CameraState state = fixture.makeState();
    fixture.body(state);

    REQUIRE(fixture.physics.overlapSphereCalls == 1);
    REQUIRE(fixture.physics.lastOverlapRadius == Catch::Approx(0.3999f).margin(1e-4f));
    REQUIRE(fixture.physics.lastFilter.layers.bits == 0b10u);
    REQUIRE(state.correctedPosition().z == Catch::Approx(7.0f).margin(1e-4f));
    REQUIRE(state.positionCorrection.z == Catch::Approx(-1.0f).margin(1e-4f));
}

TEST_CASE("Decollider excludes terrain layers and ignores clear cameras", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.decollider.decollision.enabled = true;
    fixture.decollider.decollision.obstacleLayers = LayerMask{1u};

    CameraState state = fixture.makeState();
    fixture.body(state);
    REQUIRE(fixture.physics.overlapSphereCalls == 0);
    REQUIRE(fixture.physics.sphereCastCalls == 1);
    REQUIRE(math::approximately(state.positionCorrection, math::Vec3::zero));

    fixture.decollider.terrainResolution.enabled = false;
    fixture.decollider.decollision.obstacleLayers = LayerMask{0b10u};
    state = fixture.makeState();
    fixture.body(state);
    REQUIRE(fixture.physics.overlapSphereCalls == 1);
    REQUIRE(math::approximately(state.positionCorrection, math::Vec3::zero));
}

TEST_CASE("Decollider holds the nearest distance while smoothing", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.decollider.terrainResolution.enabled = false;
    fixture.decollider.decollision.enabled = true;
    fixture.decollider.decollision.obstacleLayers = LayerMask{0b10u};
    fixture.decollider.decollision.smoothingTime = 1.0f;
    fixture.decollider.decollision.damping = 0.0f;
    fixture.camera->previousStateIsValid = true;
    fixture.world.setTime(0.1f);
    addOverlap(
        fixture.physics,
        5,
        {0.0f, 1.6f, 8.0f},
        {0.5f, 0.5f, 0.2f});

    CameraState first = fixture.makeState();
    fixture.body(first);
    REQUIRE(first.correctedPosition().z == Catch::Approx(7.0f).margin(1e-4f));

    fixture.physics.nextOverlapSphere.clear();
    fixture.world.setTime(0.2f);
    CameraState held = fixture.makeState();
    fixture.body(held);
    REQUIRE(held.correctedPosition().z == Catch::Approx(7.0f).margin(1e-4f));

    fixture.world.setTime(1.2f);
    CameraState released = fixture.makeState();
    fixture.body(released);
    REQUIRE(released.correctedPosition().z == Catch::Approx(8.0f).margin(1e-4f));
}

TEST_CASE("Decollider damps the return from an obstacle", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.decollider.terrainResolution.enabled = false;
    fixture.decollider.decollision.enabled = true;
    fixture.decollider.decollision.obstacleLayers = LayerMask{0b10u};
    addOverlap(
        fixture.physics,
        5,
        {0.0f, 1.6f, 8.0f},
        {0.5f, 0.5f, 0.2f});

    CameraState first = fixture.makeState();
    fixture.body(first);
    REQUIRE(first.correctedPosition().z == Catch::Approx(7.0f).margin(1e-4f));

    fixture.camera->previousStateIsValid = true;
    fixture.physics.nextOverlapSphere.clear();
    CameraState damped = fixture.makeState();
    fixture.body(damped);
    REQUIRE(damped.correctedPosition().z > 7.1f);
    REQUIRE(damped.correctedPosition().z < 7.2f);
}

TEST_CASE("Decollider resolves from the follow target offset", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.decollider.terrainResolution.enabled = false;
    fixture.decollider.decollision.enabled = true;
    fixture.decollider.decollision.obstacleLayers = LayerMask{0b10u};
    fixture.decollider.decollision.useFollowTarget.enabled = true;
    fixture.decollider.decollision.useFollowTarget.yOffset = 1.0f;
    addOverlap(
        fixture.physics,
        5,
        {1.0f, 3.0f, 13.0f},
        {0.5f, 0.5f, 0.2f});

    CameraState state = fixture.makeState(
        {1.0f, 3.0f, 13.0f},
        {0.0f, 0.0f, 0.0f});
    fixture.body(state);

    REQUIRE(state.correctedPosition().x == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(state.correctedPosition().z == Catch::Approx(12.0f).margin(1e-4f));
}

TEST_CASE("Decollider preserves the look-at screen composition", "[unit][decollider]")
{
    DecolliderFixture fixture;
    fixture.decollider.terrainResolution.enabled = false;
    fixture.decollider.decollision.enabled = true;
    fixture.decollider.decollision.obstacleLayers = LayerMask{0b10u};
    addOverlap(
        fixture.physics,
        5,
        {0.0f, 1.6f, 8.0f},
        {0.5f, 0.5f, 0.2f});

    CameraState state = fixture.makeState();
    state.rawOrientation = state.rawOrientation
        * math::quatFromAxisAngle(math::Vec3::up, 10.0f * math::DegToRad);
    const math::Vec2 before = math::getCameraRotationToTarget(
        state.rawOrientation,
        state.referenceLookAt - state.correctedPosition(),
        state.referenceUp);
    fixture.body(state);
    const math::Vec2 after = math::getCameraRotationToTarget(
        state.rawOrientation,
        state.referenceLookAt - state.correctedPosition(),
        state.referenceUp);
    REQUIRE(after.x == Catch::Approx(before.x).margin(1e-4f));
    REQUIRE(after.y == Catch::Approx(before.y).margin(1e-4f));
}
