#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <type_traits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/deoccluder.hpp>
#include <cinerig/components/shot_quality_evaluator.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct ShotQualityFixture
{
    ShotQualityFixture()
    {
        world.add(
            cameraEntity,
            Transform{{0.0f, 0.0f, 10.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("shot quality");
        camera = system.camera(id);
        camera->ownerEntity = cameraEntity;
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

    void evaluate(CameraState& state, float deltaTime = 0.0f)
    {
        evaluator.postPipelineStageCallback(
            *camera,
            Stage::Finalize,
            state,
            deltaTime);
    }

    FakeEntityWorld world;
    FakePhysicsWorld physics;
    CameraSystem system{world, &physics};
    VirtualCamera* camera = nullptr;
    ShotQualityEvaluator evaluator;
    EntityId cameraEntity = 1;
};

SphereCastHit occludingHit()
{
    SphereCastHit hit;
    hit.entity = 5;
    hit.point = {0.0f, 0.0f, 5.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.distance = 5.0f;
    hit.radius = 0.3f;
    return hit;
}

} // namespace

TEST_CASE(
    "ShotQualityEvaluator and Deoccluder implement the marker interface",
    "[unit][shot-quality]")
{
    static_assert(
        std::is_base_of_v<IShotQualityEvaluator, ShotQualityEvaluator>);
    static_assert(std::is_base_of_v<IShotQualityEvaluator, Deoccluder>);
    SUCCEED();
}

TEST_CASE(
    "ShotQualityEvaluator exposes reference defaults and validation",
    "[unit][shot-quality]")
{
    ShotQualityEvaluator evaluator;
    REQUIRE(evaluator.occlusionLayers.bits == 1u);
    REQUIRE(evaluator.ignoreEntity == 0);
    REQUIRE(evaluator.minimumDistanceFromTarget == Catch::Approx(0.2f));
    REQUIRE(evaluator.cameraRadius == 0.0f);
    REQUIRE_FALSE(evaluator.distanceEvaluation.enabled);
    REQUIRE(evaluator.distanceEvaluation.optimalDistance == Catch::Approx(10.0f));
    REQUIRE(evaluator.distanceEvaluation.nearLimit == Catch::Approx(5.0f));
    REQUIRE(evaluator.distanceEvaluation.farLimit == Catch::Approx(30.0f));
    REQUIRE(evaluator.distanceEvaluation.maxQualityBoost == Catch::Approx(0.2f));

    evaluator.occlusionLayers = LayerMask{0u};
    evaluator.ignoreEntity = 42;
    evaluator.minimumDistanceFromTarget = 0.0f;
    evaluator.cameraRadius = -1.0f;
    evaluator.distanceEvaluation.nearLimit = 40.0f;
    evaluator.distanceEvaluation.farLimit = 20.0f;
    evaluator.distanceEvaluation.optimalDistance = 100.0f;
    evaluator.validate();

    REQUIRE(evaluator.minimumDistanceFromTarget == Catch::Approx(0.01f));
    REQUIRE(evaluator.cameraRadius == 0.0f);
    REQUIRE(evaluator.distanceEvaluation.nearLimit == Catch::Approx(40.0f));
    REQUIRE(evaluator.distanceEvaluation.farLimit == Catch::Approx(40.0f));
    REQUIRE(evaluator.distanceEvaluation.optimalDistance == Catch::Approx(40.0f));

    evaluator.reset();
    REQUIRE(evaluator.occlusionLayers.bits == 1u);
    REQUIRE(evaluator.ignoreEntity == 0);
    REQUIRE(evaluator.minimumDistanceFromTarget == Catch::Approx(0.2f));
    REQUIRE(evaluator.cameraRadius == 0.0f);
    REQUIRE_FALSE(evaluator.distanceEvaluation.enabled);
}

TEST_CASE(
    "ShotQualityEvaluator leaves states without a look-at point unchanged",
    "[unit][shot-quality]")
{
    ShotQualityFixture fixture;
    fixture.physics.nextSphereCast = occludingHit();

    CameraState state = fixture.makeState();
    state.referenceLookAt = CameraState::kNoPoint;
    state.shotQuality = 0.5f;

    fixture.evaluate(state);

    REQUIRE(state.shotQuality == Catch::Approx(0.5f));
    REQUIRE(fixture.physics.sphereCastCalls == 0);
}

TEST_CASE(
    "ShotQualityEvaluator penalizes offscreen targets without querying physics",
    "[unit][shot-quality]")
{
    ShotQualityFixture fixture;
    fixture.physics.nextSphereCast = occludingHit();

    CameraState state = fixture.makeState();
    state.referenceLookAt = {0.0f, 0.0f, 20.0f};
    state.rawOrientation = math::Quat::identity();
    state.shotQuality = 0.5f;

    fixture.evaluate(state);

    REQUIRE(state.shotQuality == Catch::Approx(0.1f));
    REQUIRE(fixture.physics.sphereCastCalls == 0);
}

TEST_CASE(
    "ShotQualityEvaluator penalizes target occlusion and reports the query",
    "[unit][shot-quality]")
{
    ShotQualityFixture fixture;
    fixture.evaluator.cameraRadius = 0.3f;
    fixture.evaluator.ignoreEntity = 77;
    fixture.evaluator.occlusionLayers = LayerMask{0b101u};
    fixture.physics.nextSphereCast = occludingHit();

    CameraState state = fixture.makeState();
    state.shotQuality = 0.5f;

    fixture.evaluate(state);

    REQUIRE(state.shotQuality == Catch::Approx(0.1f));
    REQUIRE(fixture.physics.sphereCastCalls == 1);
    REQUIRE(fixture.physics.lastRay.origin.z == Catch::Approx(10.0f));
    REQUIRE(fixture.physics.lastRay.direction.z == Catch::Approx(-1.0f));
    REQUIRE(fixture.physics.lastSphereRadius == Catch::Approx(0.3f));
    REQUIRE(fixture.physics.lastMaxDistance == Catch::Approx(9.8f));
    REQUIRE(fixture.physics.lastFilter.layers.bits == 0b101u);
    REQUIRE(fixture.physics.lastFilter.ignoreEntity == 77u);

    fixture.physics.nextSphereCast = std::nullopt;
    state = fixture.makeState();
    state.shotQuality = 0.5f;

    fixture.evaluate(state);

    REQUIRE(state.shotQuality == Catch::Approx(0.5f));
    REQUIRE(fixture.physics.sphereCastCalls == 2);
}

TEST_CASE(
    "ShotQualityEvaluator treats targets closer than the minimum distance as obscured",
    "[unit][shot-quality]")
{
    ShotQualityFixture fixture;
    fixture.physics.nextSphereCast = occludingHit();

    CameraState state = fixture.makeState({0.0f, 0.0f, 0.1f});
    state.shotQuality = 0.5f;

    fixture.evaluate(state);

    REQUIRE(state.shotQuality == Catch::Approx(0.1f));
    REQUIRE(fixture.physics.sphereCastCalls == 0);
}

TEST_CASE(
    "ShotQualityEvaluator distance evaluation matches the reference branches",
    "[unit][shot-quality]")
{
    ShotQualityFixture fixture;
    fixture.evaluator.distanceEvaluation.enabled = true;
    fixture.evaluator.distanceEvaluation.nearLimit = 5.0f;
    fixture.evaluator.distanceEvaluation.optimalDistance = 10.0f;
    fixture.evaluator.distanceEvaluation.farLimit = 30.0f;
    fixture.evaluator.distanceEvaluation.maxQualityBoost = 0.2f;
    fixture.evaluator.occlusionLayers = LayerMask{0u};

    const auto qualityAt = [&fixture](float z) {
        CameraState state = fixture.makeState({0.0f, 0.0f, z});
        state.shotQuality = 1.0f;
        fixture.evaluate(state);
        return state.shotQuality;
    };

    REQUIRE(qualityAt(3.0f) == Catch::Approx(1.0f));
    REQUIRE(qualityAt(5.0f) == Catch::Approx(1.0f));
    REQUIRE(qualityAt(10.0f) == Catch::Approx(1.2f));
    REQUIRE(qualityAt(20.0f) == Catch::Approx(1.133333f).margin(1e-5f));
    REQUIRE(qualityAt(50.0f) == Catch::Approx(1.0f));
    REQUIRE(fixture.physics.sphereCastCalls == 0);

    fixture.evaluator.distanceEvaluation.enabled = false;
    REQUIRE(qualityAt(10.0f) == Catch::Approx(1.0f));
}

TEST_CASE(
    "ShotQualityEvaluator skips missing physics and empty layer masks",
    "[unit][shot-quality]")
{
    ShotQualityEvaluator evaluator;
    VirtualCamera camera("no physics");

    CameraState state = CameraState::makeDefault();
    state.rawPosition = {0.0f, 0.0f, 10.0f};
    state.rawOrientation = math::Quat::identity();
    state.referenceLookAt = {0.0f, 0.0f, 0.0f};
    state.shotQuality = 0.5f;

    evaluator.postPipelineStageCallback(camera, Stage::Finalize, state, 0.0f);
    REQUIRE(state.shotQuality == Catch::Approx(0.5f));

    ShotQualityFixture fixture;
    fixture.evaluator.occlusionLayers = LayerMask{0u};
    fixture.physics.nextSphereCast = occludingHit();
    state = fixture.makeState();
    state.shotQuality = 0.5f;

    fixture.evaluate(state);

    REQUIRE(state.shotQuality == Catch::Approx(0.5f));
    REQUIRE(fixture.physics.sphereCastCalls == 0);
}
