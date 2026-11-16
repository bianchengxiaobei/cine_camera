#include "fake_entity_world.hpp"
#include "pipeline_test_components.hpp"
#include "test_camera.hpp"

#include <memory>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

VirtualCamera& makeCamera(CameraSystem& system, const char* name)
{
    const CameraId id = system.createCamera<VirtualCamera>(name);
    return *system.camera(id);
}

class FakeTargetGroup final : public TargetGroup
{
public:
    struct Entry
    {
        math::Vec3 position;
        math::Quat rotation = math::Quat::identity();
        float weight = 1.0f;
    };

    bool isValid() const override { return valid; }
    bool isEmpty() const override { return entries.empty(); }
    std::size_t targetCount() const override { return entries.size(); }
    math::Vec3 targetPosition(std::size_t index) const override
    {
        return index < entries.size() ? entries[index].position : math::Vec3{};
    }
    math::Quat targetRotation(std::size_t index) const override
    {
        return index < entries.size() ? entries[index].rotation : math::Quat::identity();
    }
    float targetWeight(std::size_t index) const override
    {
        return index < entries.size() ? entries[index].weight : 0.0f;
    }
    float targetRadius(std::size_t) const override { return 0.0f; }

    bool valid = true;
    std::vector<Entry> entries;
};

class TargetAwareComponent final : public CameraComponent
{
public:
    Stage stage() const override { return Stage::Body; }
    bool isValid() const override { return followTargetPosition().has_value(); }
    void mutateCameraState(CameraState& state, float) override
    {
        mutated = true;
        state.rawPosition += math::Vec3{100.0f, 0.0f, 0.0f};
    }

    bool mutated = false;
};

} // namespace

TEST_CASE("Virtual camera pulls its owner transform, up and lens", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);

    Transform owner;
    owner.position = {1.0f, 2.0f, 3.0f};
    owner.rotation = math::quatFromAxisAngle(math::Vec3::up, 0.4f);
    world.add(1, owner);

    VirtualCamera& camera = makeCamera(system, "owner");
    camera.ownerEntity = 1;
    camera.lens.fieldOfView = 55.0f;
    camera.blendHint = BlendHint::IgnoreLookAtTarget;

    const math::Vec3 worldUp{0.0f, 0.0f, 1.0f};
    camera.internalUpdateCameraState(worldUp, 0.01f);

    REQUIRE(math::approximately(camera.state().rawPosition, owner.position));
    REQUIRE(math::approximately(camera.state().rawOrientation, owner.rotation));
    REQUIRE(math::approximately(camera.state().referenceUp, worldUp));
    REQUIRE(camera.state().lens.fieldOfView == 55.0f);
    REQUIRE(camera.state().shotQuality == 1.0f);
    REQUIRE(hasBlendHint(camera.state().blendHint, BlendHint::IgnoreLookAtTarget));
    REQUIRE(camera.previousStateIsValid);
}

TEST_CASE("Tracking, custom look-at, camera and group targets resolve safely", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity()});
    world.add(2, Transform{{10.0f, 20.0f, 30.0f}, math::quatFromAxisAngle(math::Vec3::up, 0.2f)});
    world.add(3, Transform{{4.0f, 5.0f, 6.0f}, math::Quat::identity()});

    VirtualCamera& camera = makeCamera(system, "targets");
    camera.ownerEntity = 1;
    camera.targets.tracking = TargetRef::entity(2);

    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(camera.state().hasLookAt());
    REQUIRE(math::approximately(camera.state().referenceLookAt, {10.0f, 20.0f, 30.0f}));
    REQUIRE(camera.followTargetPosition().has_value());
    REQUIRE(math::approximately(*camera.lookAtTargetPosition(), {10.0f, 20.0f, 30.0f}));

    camera.targets.customLookAt = true;
    camera.targets.lookAt = TargetRef::entity(3);
    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(math::approximately(camera.state().referenceLookAt, {4.0f, 5.0f, 6.0f}));

    const CameraId otherId = system.createCamera<VirtualCamera>("other");
    VirtualCamera& other = *system.camera(otherId);
    other.forceCameraPosition({5.0f, 6.0f, 7.0f}, math::Quat::identity());
    camera.targets.tracking = TargetRef::camera(otherId);
    camera.targets.customLookAt = false;
    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(math::approximately(camera.state().referenceLookAt, {5.0f, 6.0f, 7.0f}));

    auto group = std::make_unique<FakeTargetGroup>();
    group->entries.push_back({{0.0f, 0.0f, 0.0f}, math::Quat::identity(), 1.0f});
    group->entries.push_back({{10.0f, 0.0f, 0.0f}, math::Quat::identity(), 3.0f});
    const GroupId groupId = system.createTargetGroup(std::move(group));
    camera.targets.tracking = TargetRef::group(groupId);
    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(math::approximately(camera.state().referenceLookAt, {7.5f, 0.0f, 0.0f}));
}

TEST_CASE("Missing targets are invalid without throwing", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{});
    VirtualCamera& camera = makeCamera(system, "missing");
    camera.ownerEntity = 1;
    camera.targets.tracking = TargetRef::entity(9999);

    TargetAwareComponent& body = camera.setBody<TargetAwareComponent>();
    const math::Vec3 before = camera.state().rawPosition;
    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);

    REQUIRE_FALSE(camera.followTargetPosition().has_value());
    REQUIRE_FALSE(camera.lookAtTargetPosition().has_value());
    REQUIRE_FALSE(camera.state().hasLookAt());
    REQUIRE_FALSE(body.mutated);
    REQUIRE(math::approximately(camera.state().rawPosition, before));
}

TEST_CASE("Child cameras inherit parent targets when no local target exists", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{});
    world.add(2, Transform{{8.0f, 9.0f, 10.0f}, math::Quat::identity()});

    VirtualCamera& parent = makeCamera(system, "parent");
    VirtualCamera& child = makeCamera(system, "child");
    child.setParent(&parent);
    child.ownerEntity = 1;
    parent.targets.tracking = TargetRef::entity(2);

    child.internalUpdateCameraState(math::Vec3::up, 0.01f);

    REQUIRE(child.effectiveFollowTarget() == TargetRef::entity(2));
    REQUIRE(child.effectiveLookAtTarget() == TargetRef::entity(2));
    REQUIRE(math::approximately(child.state().referenceLookAt, {8.0f, 9.0f, 10.0f}));
}

TEST_CASE("Transform write-back is optional and preserves writable owners", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{});
    VirtualCamera& camera = makeCamera(system, "write-back");
    camera.ownerEntity = 1;

    RecordingComponent& body = camera.setBody<RecordingComponent>(Stage::Body, "body");
    body.mutateOffset = {5.0f, 0.0f, 0.0f};

    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(math::approximately(camera.state().rawPosition, {5.0f, 0.0f, 0.0f}));
    REQUIRE(math::approximately(world.transform(1).position, {5.0f, 0.0f, 0.0f}));

    world.set(1, Transform{});
    world.setWritable(1, false);
    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(math::approximately(camera.state().rawPosition, {5.0f, 0.0f, 0.0f}));
    REQUIRE(math::approximately(world.transform(1).position, {0.0f, 0.0f, 0.0f}));
}

TEST_CASE("Force position and target warp notify components and extensions", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    world.add(1, Transform{});
    world.add(2, Transform{{1.0f, 2.0f, 3.0f}, math::Quat::identity()});
    VirtualCamera& camera = makeCamera(system, "force");
    camera.ownerEntity = 1;
    camera.targets.tracking = TargetRef::entity(2);

    RecordingComponent& body = camera.setBody<RecordingComponent>(Stage::Body, "body");
    auto extension = std::make_unique<RecordingExtension>("ext");
    RecordingExtension* extensionPtr = extension.get();
    camera.addExtension(std::move(extension));

    camera.forceCameraPosition({10.0f, 0.0f, 0.0f}, math::Quat::identity());
    REQUIRE(math::approximately(camera.state().rawPosition, {10.0f, 0.0f, 0.0f}));
    REQUIRE(math::approximately(world.transform(1).position, {10.0f, 0.0f, 0.0f}));
    REQUIRE(math::approximately(body.forcedPosition, {10.0f, 0.0f, 0.0f}));
    REQUIRE(math::approximately(extensionPtr->forcedPosition, {10.0f, 0.0f, 0.0f}));

    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    camera.onTargetObjectWarped(2, {1.0f, 2.0f, 3.0f});
    REQUIRE(math::approximately(camera.state().rawPosition, {11.0f, 2.0f, 3.0f}));
    REQUIRE(math::approximately(camera.state().referenceLookAt, {2.0f, 4.0f, 6.0f}));
    REQUIRE(body.warpedTarget == 2);
    REQUIRE(math::approximately(body.warpDelta, {1.0f, 2.0f, 3.0f}));
    REQUIRE(extensionPtr->warpedTarget == 2);
}

TEST_CASE("Negative delta resets previous state for the pipeline and completes", "[scenario][pipeline][target]")
{
    FakeEntityWorld world;
    CameraSystem system(world);
    VirtualCamera& camera = makeCamera(system, "negative-dt");
    RecordingComponent& body = camera.setBody<RecordingComponent>(Stage::Body, "body");

    camera.previousStateIsValid = true;
    camera.internalUpdateCameraState(math::Vec3::up, 0.01f);
    REQUIRE(camera.previousStateIsValid);
    REQUIRE(body.previousAtPre);

    camera.internalUpdateCameraState(math::Vec3::up, -1.0f);
    REQUIRE_FALSE(body.previousAtPre);
    REQUIRE_FALSE(body.previousAtMutate);
    REQUIRE(camera.previousStateIsValid);
}
