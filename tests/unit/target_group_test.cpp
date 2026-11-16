#include "fake_entity_world.hpp"

#include <cmath>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>
#include <cinerig/targets/target_group.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

struct TargetGroupFixture
{
    TargetGroupFixture()
    {
        world.add(a, Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(b, Transform{{10.0f, 0.0f, 0.0f}, yaw(90.0f), math::Vec3::one});
    }

    FakeEntityWorld world;
    targets::TargetGroup group{world};
    EntityId a = 1;
    EntityId b = 2;
};

} // namespace

TEST_CASE("TargetGroup exposes defaults and member operations", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    REQUIRE(fixture.group.positionMode == targets::TargetGroup::PositionMode::GroupCenter);
    REQUIRE(fixture.group.rotationMode == targets::TargetGroup::RotationMode::Manual);
    REQUIRE(fixture.group.isValid());
    REQUIRE(fixture.group.isEmpty());
    REQUIRE(fixture.group.targetCount() == 0);

    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.75f);
    REQUIRE_FALSE(fixture.group.isEmpty());
    REQUIRE(fixture.group.targetCount() == 2);
    REQUIRE(fixture.group.findMember(fixture.b) == 1);
    REQUIRE(fixture.group.findMember(999) == targets::TargetGroup::npos);
    REQUIRE(fixture.group.targetWeight(1) == Catch::Approx(3.0f));
    REQUIRE(fixture.group.targetRadius(1) == Catch::Approx(0.75f));

    REQUIRE(fixture.group.removeMember(fixture.a));
    REQUIRE_FALSE(fixture.group.removeMember(fixture.a));
    REQUIRE(fixture.group.targetCount() == 1);
    fixture.group.clear();
    REQUIRE(fixture.group.targetCount() == 0);
}

TEST_CASE("TargetGroup averages weighted positions and rotations", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.group.positionMode = targets::TargetGroup::PositionMode::GroupAverage;
    fixture.group.rotationMode = targets::TargetGroup::RotationMode::GroupAverage;
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.5f);

    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    const math::Vec3 averageForward = math::normalize(
        math::Vec3{0.0f, 0.0f, -1.0f} + math::Vec3{-3.0f, 0.0f, 0.0f});
    REQUIRE(math::approximately(
        math::rotate(fixture.group.groupRotation(), math::Vec3::forward),
        averageForward,
        1e-4f));

    fixture.group.rotationMode = targets::TargetGroup::RotationMode::Manual;
    fixture.group.manualRotation = yaw(45.0f);
    REQUIRE(math::approximately(
        fixture.group.groupRotation(),
        yaw(45.0f),
        1e-4f));
}

TEST_CASE("TargetGroup computes group-center, box, and sphere bounds", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.5f);

    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {7.42424f, 0.0f, 0.0f},
        1e-3f));

    const math::Bounds box = fixture.group.boundingBox();
    REQUIRE(math::approximately(box.min(), {4.83333f, -0.5f, -0.5f}, 1e-3f));
    REQUIRE(math::approximately(box.max(), {10.5f, 0.5f, 0.5f}, 1e-3f));

    const math::BoundingSphere sphere = fixture.group.boundingSphere();
    REQUIRE(math::approximately(sphere.center, {7.42424f, 0.0f, 0.0f}, 1e-3f));
    REQUIRE(sphere.radius == Catch::Approx(2.83333f).margin(1e-3f));
}

TEST_CASE("TargetGroup ignores missing and zero-weight members", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.group.addMember(999, 1.0f, 0.5f);
    fixture.group.addMember(fixture.a, 0.0f, 0.5f);
    fixture.group.addMember(fixture.b, -1.0f, -2.0f);
    fixture.group.manualPosition = {4.0f, 5.0f, 6.0f};
    fixture.group.manualRotation = yaw(30.0f);

    REQUIRE(fixture.group.isEmpty());
    REQUIRE(math::approximately(fixture.group.groupPosition(), {4.0f, 5.0f, 6.0f}, 1e-4f));
    REQUIRE(math::approximately(fixture.group.groupRotation(), yaw(30.0f), 1e-4f));
    REQUIRE(fixture.group.targetWeight(0) == 0.0f);
    REQUIRE(fixture.group.targetWeight(1) == 0.0f);
    REQUIRE(fixture.group.targetRadius(2) == Catch::Approx(0.0f));
    REQUIRE(math::approximately(fixture.group.boundingBox().center, {4.0f, 5.0f, 6.0f}, 1e-4f));
}

TEST_CASE("TargetGroup resolves group targets through CameraSystem", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.5f);

    CameraSystem system(fixture.world);
    const GroupId groupId = system.createTargetGroup(
        std::make_unique<targets::TargetGroup>(fixture.world));
    auto* group = static_cast<targets::TargetGroup*>(system.targetGroup(groupId));
    REQUIRE(group != nullptr);
    group->addMember(fixture.a, 1.0f, 0.5f);
    group->addMember(fixture.b, 3.0f, 0.5f);

    const CameraId cameraId = system.createCamera<VirtualCamera>("group camera");
    VirtualCamera* camera = system.camera(cameraId);
    camera->targets.tracking = TargetRef::group(groupId);
    camera->internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    REQUIRE(math::approximately(
        camera->state().referenceLookAt,
        group->groupPosition(),
        1e-4f));
}



