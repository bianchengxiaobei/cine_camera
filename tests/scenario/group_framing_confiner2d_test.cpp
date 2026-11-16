#include "fake_entity_world.hpp"

#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/components/confiner2d.hpp>
#include <cinerig/runtime/camera_system.hpp>
#include <cinerig/targets/group_framing.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

class AfterAimBody : public CameraComponent
{
public:
    Stage stage() const override { return Stage::Body; }
    bool isValid() const override { return true; }
    bool bodyAppliesAfterAim() const override { return true; }

    void mutateCameraState(CameraState& state, float deltaTime) override
    {
        (void)deltaTime;
        ++mutateCount;
        seenOrthoSize = state.lens.orthographicSize;
        state.rawPosition.x += 2.0f;
    }

    int mutateCount = 0;
    float seenOrthoSize = 0.0f;
};

} // namespace

TEST_CASE("GroupFraming with Confiner2D applies once after an after-Aim body",
    "[scenario][group-framing][confiner2d]")
{
    FakeEntityWorld world;
    world.add(
        1,
        Transform{{0.0f, 0.0f, 10.0f}, math::Quat::identity(), math::Vec3::one});
    world.add(
        2,
        Transform{math::Vec3::zero, math::Quat::identity(), math::Vec3::one});

    CameraSystem system(world);
    const GroupId groupId = system.createTargetGroup(
        std::make_unique<targets::TargetGroup>(world));
    auto* group = static_cast<targets::TargetGroup*>(system.targetGroup(groupId));
    REQUIRE(group != nullptr);
    group->addMember(2, 1.0f, 1.0f);

    const CameraId cameraId = system.createCamera<VirtualCamera>("group confiner");
    VirtualCamera* camera = system.camera(cameraId);
    REQUIRE(camera != nullptr);
    camera->ownerEntity = 1;
    camera->targets.lookAt = TargetRef::group(groupId);
    camera->targets.customLookAt = true;
    camera->lens.orthographic = true;
    camera->lens.orthographicSize = 10.0f;
    camera->lens.aspect = 1.0f;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 200.0f;

    AfterAimBody& body = camera->setBody<AfterAimBody>();
    auto confiner = std::make_unique<Confiner2D>();
    camera->addExtension(std::move(confiner));

    auto framingOwned = std::make_unique<targets::GroupFraming>();
    auto* framing = framingOwned.get();
    framing->damping = 0.0f;
    framing->framingSize = 0.5f;
    framing->validate();
    camera->addExtension(std::move(framingOwned));

    camera->internalUpdateCameraState(math::Vec3::up, 1.0f / 60.0f);

    REQUIRE(body.mutateCount == 1);
    REQUIRE(body.seenOrthoSize == Catch::Approx(10.0f).margin(1e-4f));
    REQUIRE(framing->groupBoundsPosition.x == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(camera->state().lens.orthographicSize
        == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(camera->state().correctedPosition().x
        == Catch::Approx(0.0f).margin(1e-4f));
}
