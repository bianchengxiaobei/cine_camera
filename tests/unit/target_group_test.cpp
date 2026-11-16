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
    REQUIRE(fixture.group.updateMethod == targets::TargetGroup::GroupUpdateMethod::Late);
    REQUIRE(fixture.group.groupEntity == 0);
    REQUIRE_FALSE(fixture.group.writeBackSucceeded());
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
    fixture.group.doUpdate();
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

TEST_CASE("TargetGroup gates updates by selected clock frame", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    const EntityId owner = 3;
    fixture.world.add(
        owner,
        Transform{{-1.0f, -1.0f, -1.0f}, math::Quat::identity(), math::Vec3::one});
    fixture.group.groupEntity = owner;
    fixture.group.updateMethod = targets::TargetGroup::GroupUpdateMethod::Fixed;
    fixture.group.positionMode = targets::TargetGroup::PositionMode::GroupAverage;
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.5f);
    fixture.world.setFrame(UpdateClock::Fixed, 1);
    fixture.world.setFrame(UpdateClock::Late, 1);

    fixture.group.update(UpdateClock::Late);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {-1.0f, -1.0f, -1.0f},
        1e-4f));

    fixture.group.update(UpdateClock::Fixed);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    fixture.world.set(
        fixture.a,
        Transform{{10.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    fixture.group.update(UpdateClock::Fixed);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    fixture.world.setFrame(UpdateClock::Fixed, 2);
    fixture.group.update(UpdateClock::Fixed);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {10.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.updateMethod = targets::TargetGroup::GroupUpdateMethod::Late;
    fixture.world.setFrame(UpdateClock::Late, 2);
    fixture.world.set(
        fixture.b,
        Transform{{20.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    fixture.group.update(UpdateClock::Fixed);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {10.0f, 0.0f, 0.0f},
        1e-4f));
    fixture.group.update(UpdateClock::Late);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {17.5f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("TargetGroup refreshes accessors lazily on the selected clock", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.world.setFrame(UpdateClock::Late, 1);
    fixture.group.positionMode = targets::TargetGroup::PositionMode::GroupAverage;
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.5f);

    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    fixture.world.set(
        fixture.b,
        Transform{{20.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.doUpdate();
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {15.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.world.setFrame(UpdateClock::Late, 2);
    fixture.world.set(
        fixture.b,
        Transform{{30.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {22.5f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.updateMethod = targets::TargetGroup::GroupUpdateMethod::Fixed;
    fixture.world.setFrame(UpdateClock::Fixed, 1);
    fixture.world.set(
        fixture.a,
        Transform{{10.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {25.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.world.set(
        fixture.a,
        Transform{{20.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {25.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.world.setFrame(UpdateClock::Fixed, 2);
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {27.5f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("TargetGroup writes the owner transform only when writable", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    const EntityId owner = 3;
    fixture.world.add(
        owner,
        Transform{{1.0f, 2.0f, 3.0f}, yaw(15.0f), {2.0f, 3.0f, 4.0f}});
    fixture.world.setFrame(UpdateClock::Late, 1);
    fixture.group.groupEntity = owner;
    fixture.group.positionMode = targets::TargetGroup::PositionMode::GroupAverage;
    fixture.group.rotationMode = targets::TargetGroup::RotationMode::GroupAverage;
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    fixture.group.addMember(fixture.b, 3.0f, 0.5f);

    SECTION("writable owner receives the computed transform")
    {
        fixture.group.update(UpdateClock::Late);
        REQUIRE(fixture.group.writeBackSucceeded());

        const Transform transform = fixture.world.transform(owner);
        REQUIRE(math::approximately(
            transform.position,
            fixture.group.groupPosition(),
            1e-4f));
        REQUIRE(math::approximately(
            transform.rotation,
            fixture.group.groupRotation(),
            1e-4f));
        REQUIRE(math::approximately(transform.scale, {2.0f, 3.0f, 4.0f}, 1e-4f));
    }

    SECTION("read-only owner preserves its transform and the cached group state")
    {
        fixture.world.setWritable(owner, false);
        fixture.group.update(UpdateClock::Late);
        REQUIRE_FALSE(fixture.group.writeBackSucceeded());

        const Transform transform = fixture.world.transform(owner);
        REQUIRE(math::approximately(transform.position, {1.0f, 2.0f, 3.0f}, 1e-4f));
        REQUIRE(math::approximately(transform.rotation, yaw(15.0f), 1e-4f));
        REQUIRE(math::approximately(transform.scale, {2.0f, 3.0f, 4.0f}, 1e-4f));
        REQUIRE(math::approximately(
            fixture.group.groupPosition(),
            {7.5f, 0.0f, 0.0f},
            1e-4f));
        REQUIRE(math::approximately(
            fixture.group.boundingBox().center,
            {7.66667f, 0.0f, 0.0f},
            1e-3f));
    }
}

TEST_CASE("TargetGroup uses the owner transform as manual fallback", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    const EntityId owner = 3;
    fixture.world.add(
        owner,
        Transform{{4.0f, 5.0f, 6.0f}, yaw(30.0f), math::Vec3::one});
    fixture.world.setFrame(UpdateClock::Late, 1);
    fixture.group.groupEntity = owner;

    fixture.group.update(UpdateClock::Late);
    REQUIRE(fixture.group.writeBackSucceeded());
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {4.0f, 5.0f, 6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.group.groupRotation(),
        yaw(30.0f),
        1e-4f));
    REQUIRE(math::approximately(
        fixture.group.boundingBox().center,
        {4.0f, 5.0f, 6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.group.boundingSphere().center,
        {4.0f, 5.0f, 6.0f},
        1e-4f));

    fixture.group.manualPosition = {9.0f, 9.0f, 9.0f};
    fixture.group.manualRotation = yaw(90.0f);
    fixture.group.doUpdate();
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {4.0f, 5.0f, 6.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.group.groupRotation(),
        yaw(30.0f),
        1e-4f));

    fixture.world.remove(owner);
    fixture.group.doUpdate();
    REQUIRE_FALSE(fixture.group.writeBackSucceeded());
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {9.0f, 9.0f, 9.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.group.groupRotation(),
        yaw(90.0f),
        1e-4f));
}

TEST_CASE("TargetGroup invalidates the cache for member changes", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.world.setFrame(UpdateClock::Late, 1);
    fixture.group.positionMode = targets::TargetGroup::PositionMode::GroupAverage;

    fixture.group.addMember(fixture.a, 1.0f, 0.5f);
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {0.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.addMember(fixture.b, 3.0f, 0.5f);
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    REQUIRE(fixture.group.removeMember(fixture.b));
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {0.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.addMember(fixture.b, 3.0f, 0.5f);
    fixture.group.members()[1].weight = 1.0f;
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {5.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.members()[0].radius = 2.0f;
    REQUIRE(math::approximately(
        fixture.group.boundingBox().min(),
        {-2.0f, -2.0f, -2.0f},
        1e-4f));

    fixture.group.members()[0].weight = 0.0f;
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {10.0f, 0.0f, 0.0f},
        1e-4f));

    fixture.group.members()[1].weight = 0.0f;
    fixture.group.validate();
    REQUIRE(fixture.group.isEmpty());
    REQUIRE(math::approximately(
        fixture.group.groupPosition(),
        {0.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("TargetGroup view-space queries use the cached frame", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    fixture.world.setFrame(UpdateClock::Late, 1);
    fixture.group.addMember(fixture.a, 1.0f, 0.5f);

    const math::Bounds initial = fixture.group.viewSpaceBoundingBox(
        {0.0f, 0.0f, 10.0f},
        math::Quat::identity(),
        false);
    REQUIRE(math::approximately(initial.center, {0.0f, 0.0f, -10.0f}, 1e-4f));
    REQUIRE(math::approximately(initial.extents, {0.5f, 0.5f, 0.5f}, 1e-4f));

    fixture.world.set(
        fixture.a,
        Transform{{4.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    const math::Bounds cached = fixture.group.viewSpaceBoundingBox(
        {0.0f, 0.0f, 10.0f},
        math::Quat::identity(),
        false);
    REQUIRE(math::approximately(cached.center, {0.0f, 0.0f, -10.0f}, 1e-4f));

    fixture.world.setFrame(UpdateClock::Late, 2);
    const math::Bounds refreshed = fixture.group.viewSpaceBoundingBox(
        {0.0f, 0.0f, 10.0f},
        math::Quat::identity(),
        false);
    REQUIRE(math::approximately(refreshed.center, {4.0f, 0.0f, -10.0f}, 1e-4f));
}

TEST_CASE("CameraSystem updates registered target groups", "[unit][target-group]")
{
    TargetGroupFixture fixture;
    const EntityId owner = 3;
    fixture.world.add(
        owner,
        Transform{{-1.0f, -1.0f, -1.0f}, math::Quat::identity(), math::Vec3::one});
    fixture.world.setFrame(UpdateClock::Late, 1);
    fixture.world.setFrame(UpdateClock::Fixed, 1);

    CameraSystem system(fixture.world);
    const GroupId groupId = system.createTargetGroup(
        std::make_unique<targets::TargetGroup>(fixture.world));
    auto* group = static_cast<targets::TargetGroup*>(system.targetGroup(groupId));
    REQUIRE(group != nullptr);
    group->groupEntity = owner;
    group->positionMode = targets::TargetGroup::PositionMode::GroupAverage;
    group->addMember(fixture.a, 1.0f, 0.5f);
    group->addMember(fixture.b, 3.0f, 0.5f);

    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    group->updateMethod = targets::TargetGroup::GroupUpdateMethod::Fixed;
    fixture.world.setFrame(UpdateClock::Late, 2);
    fixture.world.set(
        fixture.a,
        Transform{{10.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
    system.lateUpdate(1.0f / 60.0f);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {7.5f, 0.0f, 0.0f},
        1e-4f));

    system.fixedUpdate(1.0f / 60.0f);
    REQUIRE(math::approximately(
        fixture.world.transform(owner).position,
        {10.0f, 0.0f, 0.0f},
        1e-4f));
}



