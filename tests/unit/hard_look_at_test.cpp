#include "fake_entity_world.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/hard_look_at.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct HardLookAtFixture
{
    HardLookAtFixture()
    {
        world.add(target, Transform{{0.0f, 1.0f, 5.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(cameraEntity, Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("hard look at");
        camera = system.camera(id);
        camera->ownerEntity = cameraEntity;
        camera->targets.lookAt = TargetRef::entity(target);
        camera->targets.customLookAt = true;
        aim = &camera->setAim<HardLookAt>();
    }

    void setTarget(math::Vec3 position, math::Quat rotation = math::Quat::identity())
    {
        world.set(target, Transform{position, rotation, math::Vec3::one});
    }

    void step(float deltaTime = 1.0f / 60.0f)
    {
        time += deltaTime;
        world.setTime(time);
        camera->updateCameraState(math::Vec3::up, deltaTime);
    }

    math::Vec3 forward() const
    {
        return math::rotate(camera->state().rawOrientation, math::Vec3::forward);
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    HardLookAt* aim = nullptr;
    EntityId target = 1;
    EntityId cameraEntity = 2;
    float time = 0.0f;
};

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

} // namespace

TEST_CASE("HardLookAt exposes its Aim defaults and points at the target", "[unit][hard-look-at]")
{
    HardLookAt aim;
    REQUIRE(aim.stage() == Stage::Aim);
    REQUIRE(math::approximately(aim.lookAtOffset, math::Vec3{}));

    HardLookAtFixture fixture;
    REQUIRE(fixture.aim->isValid());
    fixture.step();

    const math::Vec3 expected = math::normalize(math::Vec3{0.0f, 1.0f, 5.0f});
    REQUIRE(math::approximately(fixture.forward(), expected, 1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().referenceLookAt,
        {0.0f, 1.0f, 5.0f},
        1e-4f));
}

TEST_CASE("HardLookAt rotates the offset with the LookAt target", "[unit][hard-look-at]")
{
    HardLookAtFixture fixture;
    fixture.aim->lookAtOffset = {1.0f, 0.0f, 0.0f};
    fixture.setTarget({2.0f, 1.0f, 5.0f}, yaw(90.0f));
    fixture.step();

    REQUIRE(math::approximately(
        fixture.forward(),
        math::normalize(math::Vec3{2.0f, 1.0f, 4.0f}),
        1e-4f));

    fixture.aim->lookAtOffset = {0.0f, 0.0f, 1.0f};
    fixture.step();
    REQUIRE(math::approximately(
        fixture.forward(),
        math::normalize(math::Vec3{3.0f, 1.0f, 5.0f}),
        1e-4f));
}

TEST_CASE("HardLookAt handles directions parallel to reference up", "[unit][hard-look-at]")
{
    HardLookAtFixture fixture;
    fixture.setTarget({0.0f, 5.0f, 0.0f});
    fixture.step();

    REQUIRE(math::approximately(fixture.forward(), math::Vec3::up, 1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        math::fromToRotation(math::Vec3::forward, math::Vec3{0.0f, 5.0f, 0.0f}),
        1e-4f));

    fixture.setTarget({0.0f, -5.0f, 0.0f});
    fixture.step();
    REQUIRE(math::approximately(fixture.forward(), math::Vec3::down, 1e-4f));
}

TEST_CASE("HardLookAt ignores missing targets and zero-length directions", "[unit][hard-look-at]")
{
    HardLookAtFixture fixture;
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        math::Quat::identity(),
        1e-4f));

    fixture.world.remove(fixture.target);
    REQUIRE_FALSE(fixture.aim->isValid());
    const math::Quat before = fixture.camera->state().rawOrientation;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.camera->state().rawOrientation,
        before,
        1e-4f));
}
