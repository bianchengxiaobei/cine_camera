#include "fake_entity_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>

#include <cinerig/camera/camera_component.hpp>
#include <cinerig/camera/camera_lens_cache_invalidator.hpp>
#include <cinerig/components/confiner2d.hpp>
#include <cinerig/runtime/camera_system.hpp>
#include <cinerig/targets/group_framing.hpp>
#include <cinerig/targets/target_group.hpp>

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

class AimProbe : public CameraComponent
{
public:
    Stage stage() const override { return Stage::Aim; }
    bool isValid() const override { return true; }
    void mutateCameraState(CameraState& state, float deltaTime) override
    {
        (void)deltaTime;
        seenOrthoSize = state.lens.orthographicSize;
    }

    float seenOrthoSize = 0.0f;
};

class CountingInvalidator : public CameraExtension,
                             public ICameraLensCacheInvalidator
{
public:
    void invalidateLensCache() override { ++count; }

    int count = 0;
};

struct GroupFramingFixture
{
    GroupFramingFixture()
    {
        world.add(cameraEntity, Transform{{0.0f, 0.0f, 10.0f}, math::Quat::identity(), math::Vec3::one});
        world.add(memberEntity, Transform{math::Vec3::zero, math::Quat::identity(), math::Vec3::one});

        const GroupId groupId = system.createTargetGroup(
            std::make_unique<targets::TargetGroup>(world));
        group = static_cast<targets::TargetGroup*>(system.targetGroup(groupId));
        group->addMember(memberEntity, 1.0f, 1.0f);

        const CameraId cameraId = system.createCamera<VirtualCamera>("group framing");
        camera = system.camera(cameraId);
        camera->ownerEntity = cameraEntity;
        camera->targets.lookAt = TargetRef::group(groupId);
        camera->targets.customLookAt = true;
        camera->lens.fieldOfView = 40.0f;
        camera->lens.aspect = 1.0f;
        camera->lens.nearClipPlane = 0.1f;
        camera->lens.farClipPlane = 200.0f;

        auto extension = std::make_unique<targets::GroupFraming>();
        framing = extension.get();
        framing->damping = 0.0f;
        framing->framingSize = 0.5f;
        framing->validate();
        camera->addExtension(std::move(extension));
    }

    void step(float deltaTime = 1.0f / 60.0f)
    {
        camera->internalUpdateCameraState(math::Vec3::up, deltaTime);
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    targets::GroupFraming* framing = nullptr;
    targets::TargetGroup* group = nullptr;
    EntityId cameraEntity = 1;
    EntityId memberEntity = 2;
};

float fovForHeight(float height, float distance)
{
    return 2.0f * std::atan(height / (2.0f * distance)) * math::RadToDeg;
}

} // namespace

TEST_CASE("GroupFraming exposes the original defaults and validates ranges", "[unit][group-framing]")
{
    targets::GroupFraming framing;
    REQUIRE(framing.framingMode == targets::GroupFraming::FramingMode::HorizontalAndVertical);
    REQUIRE(framing.framingSize == 0.8f);
    REQUIRE(framing.damping == 2.0f);
    REQUIRE(framing.sizeAdjustment == targets::GroupFraming::SizeAdjustmentMode::DollyThenZoom);
    REQUIRE(framing.lateralAdjustment == targets::GroupFraming::LateralAdjustmentMode::ChangePosition);
    REQUIRE(framing.fovRange.x == 1.0f);
    REQUIRE(framing.fovRange.y == 100.0f);
    REQUIRE(framing.dollyRange.x == -100.0f);
    REQUIRE(framing.dollyRange.y == 100.0f);
    REQUIRE(framing.orthoSizeRange.x == 1.0f);
    REQUIRE(framing.orthoSizeRange.y == 1000.0f);

    framing.framingSize = 0.0f;
    framing.damping = -2.0f;
    framing.fovRange = {200.0f, 150.0f};
    framing.dollyRange = {5.0f, -5.0f};
    framing.orthoSizeRange = {-1.0f, 0.0f};
    framing.validate();
    REQUIRE(framing.framingSize == targets::GroupFraming::kMinimumGroupSize);
    REQUIRE(framing.damping == 0.0f);
    REQUIRE(framing.fovRange.x == 150.0f);
    REQUIRE(framing.fovRange.y == 150.0f);
    REQUIRE(framing.dollyRange.x == 5.0f);
    REQUIRE(framing.dollyRange.y == 5.0f);
    REQUIRE(framing.orthoSizeRange.x == 0.01f);
    REQUIRE(framing.orthoSizeRange.y == 0.01f);

    framing.damping = 3.5f;
    REQUIRE(framing.maxDampTime() == 3.5f);
}

TEST_CASE("TargetGroup computes view-space bounds and angles", "[unit][group-framing]")
{
    GroupFramingFixture fixture;

    const math::Bounds bounds = fixture.group->viewSpaceBoundingBox(
        {0.0f, 0.0f, 10.0f},
        math::Quat::identity(),
        false);
    REQUIRE(math::approximately(bounds.center, {0.0f, 0.0f, -10.0f}, 1e-4f));
    REQUIRE(math::approximately(bounds.extents, {1.0f, 1.0f, 1.0f}, 1e-4f));

    math::Vec2 minAngles{};
    math::Vec2 maxAngles{};
    math::Vec2 zRange{};
    fixture.group->viewSpaceAngularBounds(
        {0.0f, 0.0f, 10.0f},
        math::Quat::identity(),
        minAngles,
        maxAngles,
        zRange);
    REQUIRE(math::approximately(minAngles.x, -5.71059f, 1e-3f));
    REQUIRE(math::approximately(maxAngles.x, 5.71059f, 1e-3f));
    REQUIRE(math::approximately(minAngles.y, -5.71059f, 1e-3f));
    REQUIRE(math::approximately(maxAngles.y, 5.71059f, 1e-3f));
    REQUIRE(math::approximately(zRange.x, 9.0f, 1e-4f));
    REQUIRE(math::approximately(zRange.y, 11.0f, 1e-4f));
}

TEST_CASE("GroupFraming zoom-only keeps position and fits the group", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::ZoomOnly;
    fixture.step();

    INFO("pos " << fixture.camera->state().correctedPosition().x << ", "
        << fixture.camera->state().correctedPosition().y << ", "
        << fixture.camera->state().correctedPosition().z << " fov "
        << fixture.camera->state().lens.fieldOfView
        << " bounds " << fixture.framing->groupBounds.center.z
        << " extents " << fixture.framing->groupBounds.extents.x);
    REQUIRE(math::approximately(
        fixture.camera->state().correctedPosition(),
        {0.0f, 0.0f, 10.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().lens.fieldOfView,
        fovForHeight(3.6f, 9.0f),
        1e-3f));
    REQUIRE(math::approximately(fixture.framing->groupBounds.extents.z, 1.0f, 1e-4f));
}

TEST_CASE("GroupFraming honors framing mode and aspect", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::ZoomOnly;
    fixture.framing->framingMode = targets::GroupFraming::FramingMode::Horizontal;
    fixture.camera->lens.aspect = 2.0f;
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().lens.fieldOfView,
        fovForHeight(1.8f, 9.0f),
        1e-3f));
}

TEST_CASE("GroupFraming dolly range can disable translation before zoom", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::DollyThenZoom;
    fixture.framing->dollyRange = {0.0f, 0.0f};
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().correctedPosition(),
        {0.0f, 0.0f, 10.0f},
        1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().lens.fieldOfView,
        fovForHeight(3.6f, 9.0f),
        1e-3f));
}

TEST_CASE("GroupFraming can reframe by rotation without moving", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.world.set(fixture.memberEntity, {
        {3.0f, 1.0f, 0.0f},
        math::Quat::identity(),
        math::Vec3::one});
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::ZoomOnly;
    fixture.framing->lateralAdjustment = targets::GroupFraming::LateralAdjustmentMode::ChangeRotation;
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().correctedPosition(),
        {0.0f, 0.0f, 10.0f},
        1e-4f));

    const math::Vec3 lookDirection = math::normalize(
        fixture.group->groupPosition() - fixture.camera->state().correctedPosition());
    const math::Vec3 cameraForward = math::rotate(
        fixture.camera->state().correctedOrientation(),
        math::Vec3::forward);
    REQUIRE(math::dot(cameraForward, lookDirection) > 0.999f);
}

TEST_CASE("GroupFraming center offset rotates toward the offset group position", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::ZoomOnly;
    fixture.framing->lateralAdjustment = targets::GroupFraming::LateralAdjustmentMode::ChangeRotation;
    fixture.framing->centerOffset = {0.2f, 0.2f};
    fixture.step();

    const math::Vec3 forward = math::rotate(
        fixture.camera->state().correctedOrientation(),
        math::Vec3::forward);
    REQUIRE(forward.x < -0.01f);
    REQUIRE(forward.y < -0.01f);
    REQUIRE(math::approximately(
        fixture.camera->state().correctedPosition(),
        {0.0f, 0.0f, 10.0f},
        1e-4f));
}

TEST_CASE("GroupFraming adjusts orthographic size and lateral position", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 10.0f;
    fixture.camera->targets.lookAt = TargetRef::group(1);
    fixture.framing->damping = 0.0f;
    fixture.step();

    REQUIRE(math::approximately(fixture.camera->state().lens.orthographicSize, 2.0f, 1e-4f));
    REQUIRE(math::approximately(
        fixture.camera->state().correctedPosition(),
        {0.0f, 0.0f, 10.0f},
        1e-4f));
}

TEST_CASE("GroupFraming falls back from look-at to follow group", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->targets.tracking = fixture.camera->targets.lookAt;
    fixture.camera->targets.lookAt = TargetRef::entity(fixture.memberEntity);
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::ZoomOnly;
    fixture.step();

    REQUIRE(math::approximately(
        fixture.camera->state().lens.fieldOfView,
        fovForHeight(3.6f, 9.0f),
        1e-3f));
}

TEST_CASE("GroupFraming applies on the correct stage for after-aim bodies", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->setBody<AfterAimBody>();
    fixture.framing->sizeAdjustment = targets::GroupFraming::SizeAdjustmentMode::ZoomOnly;

    CameraState state = CameraState::makeDefault();
    state.lens.fieldOfView = 40.0f;
    state.rawPosition = {0.0f, 0.0f, 10.0f};
    state.referenceLookAt = math::Vec3::zero;

    fixture.framing->prePipelineMutateCameraStateCallback(*fixture.camera, state, 1.0f / 60.0f);
    fixture.framing->postPipelineStageCallback(*fixture.camera, Stage::Aim, state, 1.0f / 60.0f);
    REQUIRE(state.lens.fieldOfView == 40.0f);

    fixture.framing->postPipelineStageCallback(*fixture.camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(state.lens.fieldOfView == 40.0f);

    fixture.framing->postPipelineStageCallback(*fixture.camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(math::approximately(
        state.lens.fieldOfView,
        fovForHeight(3.6f, 9.0f),
        1e-3f));
}

TEST_CASE("GroupFraming selects Body when Confiner2D is attached", "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 10.0f;

    auto confiner = std::make_unique<Confiner2D>();
    Confiner2D* confinerPtr = confiner.get();
    fixture.camera->addExtension(std::move(confiner));
    AimProbe& probe = fixture.camera->setAim<AimProbe>();

    fixture.step();
    REQUIRE(probe.seenOrthoSize == Catch::Approx(2.0f).margin(1e-4f));
    REQUIRE(fixture.camera->state().lens.orthographicSize
        == Catch::Approx(2.0f).margin(1e-4f));

    fixture.camera->removeExtension(confinerPtr);
    fixture.framing->reset(*fixture.camera);
    fixture.step();
    REQUIRE(probe.seenOrthoSize == Catch::Approx(10.0f).margin(1e-4f));
}

TEST_CASE("GroupFraming ignores disabled lens cache invalidators",
    "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 10.0f;

    auto confiner = std::make_unique<Confiner2D>();
    confiner->setEnabled(false);
    fixture.camera->addExtension(std::move(confiner));
    AimProbe& probe = fixture.camera->setAim<AimProbe>();

    fixture.step();
    REQUIRE(probe.seenOrthoSize == Catch::Approx(10.0f).margin(1e-4f));
    REQUIRE(fixture.camera->state().lens.orthographicSize
        == Catch::Approx(2.0f).margin(1e-4f));
}

TEST_CASE("GroupFraming invalidates lens cache only when orthographic size changes",
    "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 10.0f;

    auto invalidator = std::make_unique<CountingInvalidator>();
    CountingInvalidator* invalidatorPtr = invalidator.get();
    fixture.camera->addExtension(std::move(invalidator));

    fixture.step();
    REQUIRE(invalidatorPtr->count == 1);
    REQUIRE(fixture.camera->state().lens.orthographicSize
        == Catch::Approx(2.0f).margin(1e-4f));

    fixture.step();
    REQUIRE(invalidatorPtr->count == 1);

    fixture.framing->framingSize = 0.25f;
    fixture.step();
    REQUIRE(invalidatorPtr->count == 2);
    REQUIRE(fixture.camera->state().lens.orthographicSize
        == Catch::Approx(4.0f).margin(1e-4f));
}

TEST_CASE("GroupFraming does not invalidate an unchanged orthographic size",
    "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 10.0f;
    fixture.framing->framingSize = 0.1f;

    auto invalidator = std::make_unique<CountingInvalidator>();
    CountingInvalidator* invalidatorPtr = invalidator.get();
    fixture.camera->addExtension(std::move(invalidator));

    fixture.step();
    fixture.step();
    REQUIRE(invalidatorPtr->count == 0);
    REQUIRE(fixture.camera->state().lens.orthographicSize
        == Catch::Approx(10.0f).margin(1e-4f));
}

TEST_CASE("GroupFraming cache reset clears the invalidator and previous size",
    "[unit][group-framing]")
{
    GroupFramingFixture fixture;
    fixture.camera->lens.orthographic = true;
    fixture.camera->lens.orthographicSize = 10.0f;

    auto invalidator = std::make_unique<CountingInvalidator>();
    CountingInvalidator* invalidatorPtr = invalidator.get();
    fixture.camera->addExtension(std::move(invalidator));

    fixture.step();
    REQUIRE(invalidatorPtr->count == 1);

    fixture.camera->lens.orthographicSize = 3.0f;
    fixture.framing->reset(*fixture.camera);
    fixture.step();
    REQUIRE(invalidatorPtr->count == 2);

    fixture.camera->lens.orthographicSize = 10.0f;
    fixture.framing->framingSize = 0.25f;
    invalidatorPtr->setEnabled(false);
    fixture.framing->reset(*fixture.camera);
    fixture.step();
    REQUIRE(invalidatorPtr->count == 2);
}



