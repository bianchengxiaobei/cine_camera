#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/deoccluder.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct DeoccluderFixture
{
    DeoccluderFixture()
    {
        world.add(follow, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(lookAt, Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("deoccluder");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(follow);
        camera->targets.lookAt = TargetRef::entity(lookAt);
        camera->targets.customLookAt = true;
    }

    CameraState makeState(
        math::Vec3 cameraPosition = {0.0f, 0.0f, 10.0f},
        math::Vec3 targetPoint = {0.0f, 0.0f, 0.0f}) const
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
        deoccluder.postPipelineStageCallback(*camera, Stage::Body, state, deltaTime);
    }

    FakeEntityWorld world;
    FakePhysicsWorld physics;
    CameraSystem system{world, &physics};
    VirtualCamera* camera = nullptr;
    Deoccluder deoccluder;
    EntityId follow = 1;
    EntityId lookAt = 2;
};

SphereCastHit pullHit(float z = 3.0f)
{
    SphereCastHit hit;
    hit.entity = 5;
    hit.point = {0.0f, 0.0f, z};
    hit.normal = {0.0f, 0.0f, -1.0f};
    hit.distance = 2.0f;
    hit.radius = 0.4f;
    return hit;
}

SphereCastHit angledHit()
{
    SphereCastHit hit;
    hit.entity = 6;
    hit.point = {0.0f, 0.0f, 3.0f};
    hit.normal = math::normalize(math::Vec3{0.6f, 0.0f, 0.8f});
    hit.distance = 2.0f;
    hit.radius = 0.4f;
    return hit;
}

} // namespace

TEST_CASE("Deoccluder exposes original defaults and validation", "[unit][deoccluder]")
{
    Deoccluder deoccluder;
    REQUIRE(deoccluder.collideAgainst.bits == 1u);
    REQUIRE(deoccluder.ignoreEntity == 0);
    REQUIRE(deoccluder.transparentLayers.bits == 0u);
    REQUIRE(deoccluder.minimumDistanceFromTarget == Catch::Approx(0.3f));
    REQUIRE(deoccluder.avoidObstacles.enabled);
    REQUIRE(deoccluder.avoidObstacles.distanceLimit == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.minimumOcclusionTime == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.cameraRadius == Catch::Approx(0.4f));
    REQUIRE(deoccluder.avoidObstacles.strategy
        == Deoccluder::ResolutionStrategy::PullCameraForward);
    REQUIRE(deoccluder.avoidObstacles.maximumEffort == 4);
    REQUIRE(deoccluder.avoidObstacles.smoothingTime == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.damping == Catch::Approx(0.4f));
    REQUIRE(deoccluder.avoidObstacles.dampingWhenOccluded == Catch::Approx(0.2f));
    REQUIRE_FALSE(deoccluder.qualityEvaluation.enabled);
    REQUIRE(deoccluder.qualityEvaluation.optimalDistance == Catch::Approx(10.0f));
    REQUIRE(deoccluder.qualityEvaluation.nearLimit == Catch::Approx(5.0f));
    REQUIRE(deoccluder.qualityEvaluation.farLimit == Catch::Approx(30.0f));
    REQUIRE(deoccluder.qualityEvaluation.maxQualityBoost == Catch::Approx(0.2f));
    REQUIRE(deoccluder.maxDampTime() == Catch::Approx(0.4f));

    deoccluder.avoidObstacles.distanceLimit = -1.0f;
    deoccluder.avoidObstacles.minimumOcclusionTime = -1.0f;
    deoccluder.avoidObstacles.cameraRadius = -1.0f;
    deoccluder.avoidObstacles.smoothingTime = -1.0f;
    deoccluder.avoidObstacles.damping = -1.0f;
    deoccluder.avoidObstacles.dampingWhenOccluded = -1.0f;
    deoccluder.avoidObstacles.maximumEffort = 0;
    deoccluder.minimumDistanceFromTarget = 0.0f;
    deoccluder.qualityEvaluation.nearLimit = 40.0f;
    deoccluder.qualityEvaluation.farLimit = 20.0f;
    deoccluder.qualityEvaluation.optimalDistance = 100.0f;
    deoccluder.qualityEvaluation.maxQualityBoost = -1.0f;
    deoccluder.validate();

    REQUIRE(deoccluder.avoidObstacles.distanceLimit == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.minimumOcclusionTime == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.cameraRadius == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.smoothingTime == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.damping == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.dampingWhenOccluded == 0.0f);
    REQUIRE(deoccluder.avoidObstacles.maximumEffort == 1);
    REQUIRE(deoccluder.minimumDistanceFromTarget == Catch::Approx(0.01f));
    REQUIRE(deoccluder.qualityEvaluation.nearLimit == Catch::Approx(40.0f));
    REQUIRE(deoccluder.qualityEvaluation.farLimit == Catch::Approx(40.0f));
    REQUIRE(deoccluder.qualityEvaluation.optimalDistance == Catch::Approx(40.0f));
    REQUIRE(deoccluder.qualityEvaluation.maxQualityBoost == 0.0f);

    deoccluder.avoidObstacles.enabled = false;
    REQUIRE(deoccluder.maxDampTime() == 0.0f);
    deoccluder.reset();
    REQUIRE(deoccluder.avoidObstacles.enabled);
    REQUIRE(deoccluder.collideAgainst.bits == 1u);
}

TEST_CASE("Deoccluder pulls the camera in front of an obstacle", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.collideAgainst = LayerMask{0b111u};
    fixture.deoccluder.transparentLayers = LayerMask{0b010u};
    fixture.deoccluder.ignoreEntity = 9;
    fixture.deoccluder.avoidObstacles.distanceLimit = 4.0f;
    fixture.physics.nextSphereCast = pullHit();

    CameraState state = fixture.makeState();
    fixture.body(state);

    REQUIRE(fixture.physics.sphereCastCalls == 1);
    REQUIRE(fixture.physics.lastFilter.layers.bits == 0b101u);
    REQUIRE(fixture.physics.lastFilter.ignoreEntity == 9u);
    REQUIRE(fixture.physics.lastSphereRadius == Catch::Approx(0.4f));
    REQUIRE(fixture.physics.lastMaxDistance == Catch::Approx(4.0f));
    REQUIRE(fixture.physics.lastRay.origin.z == Catch::Approx(0.701f).margin(1e-4f));
    REQUIRE(fixture.physics.lastRay.direction.z == Catch::Approx(1.0f).margin(1e-4f));

    REQUIRE(state.correctedPosition().z == Catch::Approx(2.599f).margin(1e-4f));
    REQUIRE(state.rawPosition.z == Catch::Approx(10.0f));
    REQUIRE(fixture.deoccluder.cameraWasDisplaced(fixture.camera->cameraId()));
    REQUIRE(fixture.deoccluder.cameraDisplacementDistance(fixture.camera->cameraId())
        == Catch::Approx(7.401f).margin(1e-4f));
}

TEST_CASE("Deoccluder keeps the minimum distance from the target", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    SphereCastHit hit = pullHit();
    hit.point = {0.0f, 0.0f, 0.1f};
    fixture.physics.nextSphereCast = hit;

    CameraState state = fixture.makeState();
    fixture.body(state);

    REQUIRE(state.correctedPosition().z == Catch::Approx(0.701f).margin(1e-4f));
    REQUIRE((state.correctedPosition() - state.referenceLookAt).length()
        == Catch::Approx(0.701f).margin(1e-4f));
}

TEST_CASE("Deoccluder handles disabled and missing resolution data", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.avoidObstacles.enabled = false;
    CameraState state = fixture.makeState();
    fixture.body(state);
    REQUIRE_FALSE(fixture.deoccluder.cameraWasDisplaced(fixture.camera->cameraId()));
    REQUIRE(fixture.physics.sphereCastCalls == 0);

    fixture.deoccluder.reset();
    fixture.deoccluder.collideAgainst = LayerMask{0u};
    fixture.physics.nextSphereCast = pullHit();
    state = fixture.makeState();
    fixture.body(state);
    REQUIRE(math::approximately(state.positionCorrection, math::Vec3::zero));
    REQUIRE(fixture.physics.sphereCastCalls == 0);

    fixture.deoccluder.reset();
    fixture.deoccluder.transparentLayers = LayerMask{1u};
    state = fixture.makeState();
    fixture.body(state);
    REQUIRE(math::approximately(state.positionCorrection, math::Vec3::zero));
    REQUIRE(fixture.physics.sphereCastCalls == 0);

    fixture.deoccluder.reset();
    state = fixture.makeState();
    state.referenceLookAt = CameraState::kNoPoint;
    fixture.camera->targets.tracking = TargetRef{};
    fixture.body(state);
    REQUIRE(math::approximately(state.positionCorrection, math::Vec3::zero));
    REQUIRE(fixture.physics.sphereCastCalls == 0);
}

TEST_CASE("Deoccluder resolves toward the follow target offset", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.avoidObstacles.useFollowTarget.enabled = true;
    fixture.deoccluder.avoidObstacles.useFollowTarget.yOffset = 1.0f;
    fixture.physics.nextSphereCast = pullHit();

    CameraState state = fixture.makeState(
        {1.0f, 3.0f, 13.0f},
        {1.0f, 3.0f, 3.0f});
    fixture.body(state);

    REQUIRE(fixture.physics.lastRay.origin.x == Catch::Approx(1.0f).margin(1e-4f));
    REQUIRE(fixture.physics.lastRay.origin.y == Catch::Approx(3.0f).margin(1e-4f));
    REQUIRE(fixture.physics.lastRay.origin.z == Catch::Approx(3.701f).margin(1e-4f));
}

TEST_CASE("Deoccluder pushes around obstacles to restore distance", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.avoidObstacles.strategy =
        Deoccluder::ResolutionStrategy::PreserveCameraDistance;
    fixture.physics.sphereCastResults = {
        angledHit(),
        std::nullopt,
        std::nullopt,
        std::nullopt,
    };

    CameraState state = fixture.makeState();
    fixture.body(state);

    const float distance = (state.correctedPosition() - state.referenceLookAt).length();
    REQUIRE(distance > 8.0f);
    REQUIRE(distance <= 10.001f);
}

TEST_CASE("Deoccluder damps and delays occlusion resolution", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.physics.nextSphereCast = pullHit(3.0f);
    CameraState first = fixture.makeState();
    fixture.body(first);
    REQUIRE(first.correctedPosition().z == Catch::Approx(2.599f).margin(1e-4f));

    fixture.camera->previousStateIsValid = true;
    fixture.physics.nextSphereCast = pullHit(5.0f);
    CameraState damped = fixture.makeState();
    fixture.body(damped);
    const float dampedPosition = damped.correctedPosition().z;
    REQUIRE(dampedPosition > 2.6f);
    REQUIRE(dampedPosition < 4.6f);

    fixture.deoccluder.avoidObstacles.minimumOcclusionTime = 5.0f;
    fixture.world.setTime(1.0f);
    CameraState delayedFirst = fixture.makeState();
    fixture.body(delayedFirst);
    fixture.world.setTime(1.1f);
    fixture.physics.nextSphereCast = pullHit(5.0f);
    CameraState delayedSecond = fixture.makeState();
    fixture.body(delayedSecond);
    REQUIRE(delayedSecond.correctedPosition().z
        == Catch::Approx(dampedPosition).margin(1e-4f));
}

TEST_CASE("Deoccluder reports occlusion and shot quality", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.qualityEvaluation.enabled = true;
    fixture.physics.sphereCastResults = {
        std::nullopt,
        pullHit(),
    };

    CameraState state = fixture.makeState();
    fixture.body(state);
    fixture.deoccluder.postPipelineStageCallback(
        *fixture.camera,
        Stage::Finalize,
        state,
        1.0f / 60.0f);

    REQUIRE(fixture.deoccluder.isTargetObscured(fixture.camera->cameraId()));
    REQUIRE_FALSE(fixture.deoccluder.cameraWasDisplaced(fixture.camera->cameraId()));
    REQUIRE(state.shotQuality == Catch::Approx(0.24f).margin(1e-4f));

    fixture.physics.sphereCastResults = {
        pullHit(),
        std::nullopt,
    };
    fixture.physics.nextSphereCast = std::nullopt;
    fixture.camera->previousStateIsValid = false;
    CameraState displaced = fixture.makeState();
    fixture.body(displaced);
    fixture.deoccluder.postPipelineStageCallback(
        *fixture.camera,
        Stage::Finalize,
        displaced,
        1.0f / 60.0f);

    REQUIRE(fixture.deoccluder.cameraWasDisplaced(fixture.camera->cameraId()));
    REQUIRE_FALSE(fixture.deoccluder.isTargetObscured(fixture.camera->cameraId()));
    REQUIRE(displaced.shotQuality == Catch::Approx(0.8f).margin(1e-4f));
}



TEST_CASE("Deoccluder respects camera radius overlaps", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.avoidObstacles.strategy =
        Deoccluder::ResolutionStrategy::PreserveCameraDistance;
    fixture.physics.nextOverlapSphere.push_back(OverlapHit{7, {0.0f, 0.0f, 10.0f}, 0.0f});
    fixture.physics.boundsById[7] = Bounds{{0.0f, 0.0f, 9.5f}, {0.5f, 0.5f, 0.5f}};

    CameraState state = fixture.makeState();
    fixture.body(state);

    REQUIRE(state.correctedPosition().z == Catch::Approx(10.401f).margin(1e-4f));
    REQUIRE(fixture.deoccluder.cameraWasDisplaced(fixture.camera->cameraId()));
}

TEST_CASE("Deoccluder holds the nearest distance while smoothing", "[unit][deoccluder]")
{
    DeoccluderFixture fixture;
    fixture.deoccluder.avoidObstacles.smoothingTime = 1.0f;
    fixture.world.setTime(0.1f);
    fixture.physics.nextSphereCast = pullHit(3.0f);

    CameraState first = fixture.makeState();
    fixture.body(first);
    REQUIRE(first.correctedPosition().z == Catch::Approx(2.599f).margin(1e-4f));

    fixture.camera->previousStateIsValid = true;
    fixture.world.setTime(0.2f);
    fixture.physics.nextSphereCast = pullHit(5.0f);
    CameraState smoothed = fixture.makeState();
    fixture.body(smoothed);
    REQUIRE(smoothed.correctedPosition().z == Catch::Approx(2.599f).margin(1e-4f));
}
