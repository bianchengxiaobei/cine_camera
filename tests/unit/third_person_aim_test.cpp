#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/third_person_aim.hpp>
#include <cinerig/components/third_person_follow.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct AimFixture
{
    AimFixture()
    {
        world.add(target, Transform{{0.0f, 1.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("third person aim");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        follow = &camera->setBody<ThirdPersonFollow>();
        camera->addExtension(std::make_unique<ThirdPersonAim>());
        aim = dynamic_cast<ThirdPersonAim*>(camera->extension(0));
        REQUIRE(aim != nullptr);
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
    ThirdPersonAim* aim = nullptr;
    EntityId target = 1;
    float time = 0.0f;
};

} // namespace

TEST_CASE("ThirdPersonAim exposes the original defaults and validates distance", "[unit][third-person-aim]")
{
    ThirdPersonAim aim;
    REQUIRE(aim.aimDistance == Catch::Approx(200.0f));
    REQUIRE(aim.aimCollisionFilter.bits == 1u);
    REQUIRE(aim.ignoreEntity == 0u);
    REQUIRE(aim.noiseCancellation);
    aim.aimDistance = 0.25f;
    aim.validate();
    REQUIRE(aim.aimDistance == 1.0f);
}

TEST_CASE("ThirdPersonAim detects a hit and stabilizes the camera", "[unit][third-person-aim]")
{
    AimFixture fixture;
    RaycastHit hit;
    hit.entity = 42;
    hit.point = {0.0f, 1.0f, -5.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.distance = 7.0f;
    fixture.physics.nextRaycast = hit;

    fixture.step();

    REQUIRE(math::approximately(fixture.aim->aimTarget(), hit.point, 1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        hit.point,
        1e-4f));
    REQUIRE(fixture.physics.raycastCalls == 2);
    REQUIRE(fixture.physics.lastFilter.layers.bits == 1u);

    const math::Vec3 direction = hit.point - fixture.camera->state().finalPosition();
    REQUIRE(math::approximately(
        math::rotate(fixture.camera->state().rawOrientation, math::Vec3::forward),
        math::normalize(direction),
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().orientationCorrection,
        math::Quat::identity(),
        1e-4f));
}

TEST_CASE("ThirdPersonAim falls back to the configured distance on a miss", "[unit][third-person-aim]")
{
    AimFixture fixture;
    fixture.aim->aimDistance = 8.0f;
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        {0.5f, 1.0f, -6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.aim->aimTarget(),
        {0.5f, 1.0f, -6.0f},
        1e-4f));
}

TEST_CASE("ThirdPersonAim honors filters and can be disabled for aim detection", "[unit][third-person-aim]")
{
    AimFixture fixture;
    fixture.aim->aimCollisionFilter.bits = 0u;
    fixture.step();
    REQUIRE(fixture.physics.raycastCalls == 0);
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        {0.5f, 1.0f, -198.0f},
        1e-3f));

    fixture.physics.reset();
    fixture.aim->aimCollisionFilter.bits = 1u;
    fixture.aim->ignoreEntity = fixture.target;
    RaycastHit hit;
    hit.entity = 7;
    hit.point = {0.0f, 1.0f, -4.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.distance = 4.0f;
    fixture.physics.nextRaycast = hit;
    fixture.step();
    REQUIRE(fixture.physics.lastFilter.ignoreEntity == fixture.target);
}

TEST_CASE("ThirdPersonAim can detect without cancelling camera noise", "[unit][third-person-aim]")
{
    AimFixture fixture;
    fixture.aim->noiseCancellation = false;
    RaycastHit hit;
    hit.entity = 8;
    hit.point = {0.0f, 1.0f, -3.0f};
    hit.normal = {0.0f, 0.0f, 1.0f};
    hit.distance = 3.0f;
    fixture.physics.nextRaycast = hit;

    fixture.step();

    REQUIRE(math::approximately(fixture.aim->aimTarget(), hit.point, 1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        hit.point,
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        math::Quat::identity(),
        1e-4f));
}

TEST_CASE("ThirdPersonAim stabilizes an existing look-at without a follow target", "[unit][third-person-aim]")
{
    FakeEntityWorld world;
    const EntityId lookAtEntity = 5;
    world.add(lookAtEntity, Transform{{5.0f, 1.0f, -5.0f}, math::Quat::identity(), math::Vec3::one});
    CameraSystem system{world};
    const CameraId id = system.createCamera<VirtualCamera>("aim only");
    VirtualCamera* camera = system.camera(id);
    camera->targets.lookAt = TargetRef::entity(lookAtEntity);
    camera->targets.customLookAt = true;
    camera->addExtension(std::make_unique<ThirdPersonAim>());

    world.setTime(1.0f / 60.0f);
    camera->updateCameraState(math::Vec3::up, 1.0f / 60.0f);

    const math::Vec3 direction = {5.0f, 1.0f, -5.0f};
    REQUIRE(math::approximately(
        math::rotate(camera->state().rawOrientation, math::Vec3::forward),
        math::normalize(direction),
        1e-4f));
}
