#include "fake_entity_world.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/camera_offset.hpp>
#include <cinerig/math/geometry.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct CameraOffsetFixture
{
    CameraOffsetFixture()
    {
        world.add(target, Transform{{2.0f, 1.0f, 10.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(cameraEntity, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("camera offset");
        camera = system.camera(id);
        camera->ownerEntity = cameraEntity;
        camera->targets.lookAt = TargetRef::entity(target);
        camera->targets.customLookAt = true;
        auto extension = std::make_unique<CameraOffset>();
        offset = extension.get();
        camera->addExtension(std::move(extension));
    }

    void step(float deltaTime = 1.0f / 60.0f)
    {
        time += deltaTime;
        world.setTime(time);
        camera->updateCameraState(math::Vec3::up, deltaTime);
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    CameraOffset* offset = nullptr;
    EntityId target = 1;
    EntityId cameraEntity = 2;
    float time = 0.0f;
};

} // namespace

TEST_CASE("CameraOffset exposes defaults and applies a camera-space offset", "[unit][camera-offset]")
{
    CameraOffset offset;
    REQUIRE(offset.applyAfter == Stage::Aim);
    REQUIRE_FALSE(offset.preserveComposition);
    REQUIRE(math::approximately(offset.offset, math::Vec3{}));

    CameraOffsetFixture fixture;
    fixture.world.set(fixture.cameraEntity, Transform{
        {1.0f, 2.0f, 3.0f},
        math::quatFromAxisAngle(math::Vec3::up, 90.0f * math::DegToRad),
        math::Vec3::one});
    fixture.offset->offset = {1.0f, 0.0f, 0.0f};
    fixture.step();

    const math::Vec3 expected = math::rotate(
        math::quatFromAxisAngle(math::Vec3::up, 90.0f * math::DegToRad),
        math::Vec3{1.0f, 0.0f, 0.0f});
    REQUIRE(math::approximately(
        fixture.camera->state().positionCorrection,
        expected,
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().rawPosition,
        {1.0f, 2.0f, 3.0f},
        1e-4f));
}

TEST_CASE("CameraOffset applies only at the selected stage", "[unit][camera-offset]")
{
    CameraOffsetFixture fixture;
    fixture.offset->offset = {0.0f, 1.0f, 0.0f};

    CameraState state = CameraState::makeDefault();
    fixture.offset->postPipelineStageCallback(*fixture.camera, Stage::Body, state, 0.0f);
    REQUIRE(math::approximately(state.positionCorrection, math::Vec3{}));

    fixture.offset->postPipelineStageCallback(*fixture.camera, Stage::Aim, state, 0.0f);
    REQUIRE(math::approximately(state.positionCorrection, {0.0f, 1.0f, 0.0f}, 1e-4f));

    fixture.camera->removeExtension(fixture.offset);
    auto finalizeOffset = std::make_unique<CameraOffset>();
    finalizeOffset->offset = {1.0f, 0.0f, 0.0f};
    finalizeOffset->applyAfter = Stage::Finalize;
    CameraOffset* finalize = finalizeOffset.get();
    fixture.camera->addExtension(std::move(finalizeOffset));
    CameraState finalizeState = CameraState::makeDefault();
    finalize->postPipelineStageCallback(*fixture.camera, Stage::Aim, finalizeState, 0.0f);
    REQUIRE(math::approximately(finalizeState.positionCorrection, math::Vec3{}));
    finalize->postPipelineStageCallback(*fixture.camera, Stage::Finalize, finalizeState, 0.0f);
    REQUIRE(math::approximately(finalizeState.positionCorrection, {1.0f, 0.0f, 0.0f}, 1e-4f));
}

TEST_CASE("CameraOffset translates the reference look-at without preservation", "[unit][camera-offset]")
{
    CameraOffsetFixture fixture;
    fixture.offset->offset = {0.0f, 2.0f, 0.0f};
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().positionCorrection,
        {0.0f, 2.0f, 0.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        {2.0f, 3.0f, 10.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        math::Quat::identity(),
        1e-4f));

    fixture.offset->applyAfter = Stage::Body;
    fixture.offset->preserveComposition = true;
    fixture.offset->offset = {0.0f, 1.0f, 0.0f};
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        {2.0f, 2.0f, 10.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        math::Quat::identity(),
        1e-4f));
}

TEST_CASE("CameraOffset preserves the target screen rotation when requested", "[unit][camera-offset]")
{
    CameraOffsetFixture fixture;
    fixture.world.set(fixture.cameraEntity, Transform{
        {0.0f, 0.0f, 0.0f},
        math::Quat::identity(),
        math::Vec3::one});
    fixture.offset->offset = {0.8f, 0.4f, 0.0f};
    fixture.offset->preserveComposition = true;

    const math::Vec3 targetPosition{2.0f, 1.0f, 10.0f};
    const math::Vec2 before = math::getCameraRotationToTarget(
        math::Quat::identity(),
        targetPosition,
        math::Vec3::up);
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        targetPosition,
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().correctedPosition(),
        {0.8f, 0.4f, 0.0f},
        1e-4f));

    const math::Vec2 after = math::getCameraRotationToTarget(
        fixture.camera->state().rawOrientation,
        fixture.camera->state().referenceLookAt - fixture.camera->state().correctedPosition(),
        fixture.camera->state().referenceUp);
    REQUIRE(math::approximately(after, before, 1e-4f));
}
