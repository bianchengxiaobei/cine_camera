#include "fake_entity_world.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/rotate_with_follow_target.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct RotateFixture
{
    RotateFixture()
    {
        world.add(target, Transform{{0.0f, 1.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(cameraEntity, Transform{{0.0f, 2.0f, 8.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("rotate with follow");
        camera = system.camera(id);
        camera->ownerEntity = cameraEntity;
        camera->targets.tracking = TargetRef::entity(target);
        rotate = &camera->setAim<RotateWithFollowTarget>();
    }

    void setTargetRotation(math::Quat rotation)
    {
        world.set(target, Transform{{0.0f, 1.0f, 0.0f}, rotation, math::Vec3::one});
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
    RotateWithFollowTarget* rotate = nullptr;
    EntityId target = 1;
    EntityId cameraEntity = 2;
    float time = 0.0f;
};

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

} // namespace

TEST_CASE("RotateWithFollowTarget matches the target orientation with zero damping", "[unit][rotate-with-follow]")
{
    RotateWithFollowTarget rotate;
    REQUIRE(rotate.stage() == Stage::Aim);
    REQUIRE(rotate.damping == 0.0f);
    REQUIRE(rotate.maxDampTime() == 0.0f);

    RotateFixture fixture;
    REQUIRE(fixture.rotate->isValid());
    const math::Quat firstTarget = yaw(60.0f);
    fixture.setTargetRotation(firstTarget);
    fixture.step();

    REQUIRE(math::angle(fixture.camera->state().rawOrientation, firstTarget) < 1e-4f);
    REQUIRE(math::approximately(
        fixture.camera->state().referenceUp,
        math::rotate(firstTarget, math::Vec3::up),
        1e-4f));

    const math::Quat secondTarget = yaw(-35.0f);
    fixture.setTargetRotation(secondTarget);
    fixture.step();
    REQUIRE(math::angle(fixture.camera->state().rawOrientation, secondTarget) < 1e-4f);
}

TEST_CASE("RotateWithFollowTarget damps toward the target and snaps on negative time", "[unit][rotate-with-follow]")
{
    RotateFixture fixture;
    fixture.rotate->damping = 1.0f;
    REQUIRE(fixture.rotate->maxDampTime() == 1.0f);
    fixture.step();

    const math::Quat targetRotation = yaw(90.0f);
    fixture.setTargetRotation(targetRotation);
    fixture.step();

    const math::Quat firstFrame = fixture.camera->state().rawOrientation;
    REQUIRE(math::angle(math::Quat::identity(), firstFrame) > 1e-3f);
    REQUIRE(math::angle(firstFrame, targetRotation) > 1e-3f);

    for (int frame = 0; frame < 240; ++frame)
        fixture.step();
    REQUIRE(math::angle(fixture.camera->state().rawOrientation, targetRotation) < 1e-3f);

    const math::Quat newTarget = yaw(-120.0f);
    fixture.setTargetRotation(newTarget);
    fixture.camera->updateCameraState(math::Vec3::up, -1.0f);
    REQUIRE(math::angle(fixture.camera->state().rawOrientation, newTarget) < 1e-4f);
    REQUIRE(math::approximately(
        fixture.camera->state().referenceUp,
        math::rotate(newTarget, math::Vec3::up),
        1e-4f));
}

TEST_CASE("RotateWithFollowTarget uses follow-target attachment damping", "[unit][rotate-with-follow]")
{
    RotateFixture fixture;
    fixture.rotate->damping = 0.5f;
    fixture.step();

    const math::Quat targetRotation = yaw(90.0f);
    fixture.setTargetRotation(targetRotation);
    fixture.camera->followTargetAttachment = 0.0f;
    CameraState detached = fixture.camera->state();
    fixture.rotate->mutateCameraState(detached, 1.0f / 60.0f);
    REQUIRE(math::angle(detached.rawOrientation, math::Quat::identity()) < 1e-4f);

    fixture.camera->followTargetAttachment = 1.0f;
    CameraState attached = fixture.camera->state();
    fixture.rotate->mutateCameraState(attached, 1.0f / 60.0f);
    REQUIRE(math::angle(attached.rawOrientation, math::Quat::identity()) > 1e-3f);
    REQUIRE(math::angle(attached.rawOrientation, targetRotation) > 1e-3f);
}

TEST_CASE("RotateWithFollowTarget ignores missing targets", "[unit][rotate-with-follow]")
{
    RotateFixture fixture;
    fixture.setTargetRotation(yaw(30.0f));
    fixture.step();
    const math::Quat before = fixture.camera->state().rawOrientation;

    fixture.world.remove(fixture.target);
    REQUIRE_FALSE(fixture.rotate->isValid());
    fixture.step();
    REQUIRE(math::angle(fixture.camera->state().rawOrientation, before) < 1e-4f);
}
