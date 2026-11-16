#include "fake_entity_world.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/pan_tilt.hpp>
#include <cinerig/core/input_axis_controller.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct PanTiltFixture
{
    PanTiltFixture()
    {
        world.add(target, Transform{});
        cameraEntity = 2;
        world.add(cameraEntity, {
            {0.0f, 1.0f, 5.0f},
            math::Quat::identity(),
            math::Vec3::one,
        });
        const CameraId id = system.createCamera<VirtualCamera>("pan tilt");
        camera = system.camera(id);
        camera->ownerEntity = cameraEntity;
        camera->targets.tracking = TargetRef::entity(target);
        panTilt = &camera->setAim<PanTilt>();
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

    math::Vec3 forward() const
    {
        return math::rotate(camera->state().rawOrientation, math::Vec3::forward);
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    PanTilt* panTilt = nullptr;
    EntityId target = 1;
    EntityId cameraEntity = 0;
    float time = 0.0f;
};

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

struct TestInputSource final : IInputSource
{
    float horizontal = 0.0f;
    float vertical = 0.0f;

    float axis(std::string_view name) const override
    {
        if (name == "Horizontal")
            return horizontal;
        if (name == "Vertical")
            return vertical;
        return 0.0f;
    }

    math::Vec2 axis2D(std::string_view) const override { return {}; }
};

} // namespace

TEST_CASE("PanTilt exposes the original axis defaults", "[unit][pan-tilt]")
{
    InputAxis pan = PanTilt::makeDefaultPanAxis();
    REQUIRE(pan.range.x == -180.0f);
    REQUIRE(pan.range.y == 180.0f);
    REQUIRE(pan.wrap);
    REQUIRE(pan.center == 0.0f);
    REQUIRE(pan.value == 0.0f);

    InputAxis tilt = PanTilt::makeDefaultTiltAxis();
    REQUIRE(tilt.range.x == -70.0f);
    REQUIRE(tilt.range.y == 70.0f);
    REQUIRE_FALSE(tilt.wrap);
    REQUIRE(tilt.center == 0.0f);
    REQUIRE(tilt.value == 0.0f);

    PanTilt component;
    component.tiltAxis.range = {-120.0f, 120.0f};
    component.validate();
    REQUIRE(component.tiltAxis.range.x == -90.0f);
    REQUIRE(component.tiltAxis.range.y == 90.0f);
}

TEST_CASE("PanTilt applies right-handed pan and tilt signs", "[unit][pan-tilt]")
{
    PanTiltFixture fixture;
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::World;

    fixture.panTilt->panAxis.value = 90.0f;
    fixture.step();
    REQUIRE(math::approximately(fixture.forward(), {1.0f, 0.0f, 0.0f}, 1e-3f));

    fixture.panTilt->panAxis.value = 0.0f;
    fixture.panTilt->tiltAxis.value = 45.0f;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.forward(),
        {0.0f, -std::sin(45.0f * math::DegToRad), -std::cos(45.0f * math::DegToRad)},
        1e-3f));
}

TEST_CASE("PanTilt supports tracking, look-at, and parent reference frames", "[unit][pan-tilt]")
{
    PanTiltFixture fixture;
    const math::Quat targetRotation = yaw(90.0f);
    fixture.setTarget({}, targetRotation);
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::TrackingTarget;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.forward(),
        math::rotate(targetRotation, math::Vec3::forward),
        1e-3f));

    const EntityId lookAt = 20;
    fixture.world.add(lookAt, {
        {},
        yaw(-90.0f),
        math::Vec3::one,
    });
    fixture.camera->targets.lookAt = TargetRef::entity(lookAt);
    fixture.camera->targets.customLookAt = true;
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::LookAtTarget;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.forward(),
        math::rotate(yaw(-90.0f), math::Vec3::forward),
        1e-3f));

    const CameraId parentId = fixture.system.createCamera<VirtualCamera>("parent");
    VirtualCamera* parent = fixture.system.camera(parentId);
    parent->ownerEntity = 30;
    fixture.world.add(parent->ownerEntity, {
        {},
        yaw(45.0f),
        math::Vec3::one,
    });
    parent->updateCameraState(math::Vec3::up, 1.0f / 60.0f);
    fixture.camera->setParent(parent);
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::ParentObject;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.forward(),
        math::rotate(yaw(45.0f), math::Vec3::forward),
        1e-3f));
}

TEST_CASE("PanTilt computes recenter targets in the selected frame", "[unit][pan-tilt]")
{
    PanTiltFixture fixture;
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::World;
    fixture.panTilt->recenterTarget = PanTilt::RecenterTargetMode::TrackingTargetForward;
    fixture.setTarget({}, yaw(45.0f));

    math::Vec2 target = fixture.panTilt->getRecenterTarget();
    REQUIRE(target.x == Catch::Approx(-45.0f).margin(1e-3f));
    REQUIRE(target.y == Catch::Approx(0.0f).margin(1e-3f));

    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::TrackingTarget;
    target = fixture.panTilt->getRecenterTarget();
    REQUIRE(target.x == Catch::Approx(0.0f).margin(1e-3f));
    REQUIRE(target.y == Catch::Approx(0.0f).margin(1e-3f));

    const EntityId lookAt = 21;
    fixture.world.add(lookAt, {
        {},
        yaw(-90.0f),
        math::Vec3::one,
    });
    fixture.camera->targets.lookAt = TargetRef::entity(lookAt);
    fixture.camera->targets.customLookAt = true;
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::World;
    fixture.panTilt->recenterTarget = PanTilt::RecenterTargetMode::LookAtTargetForward;
    target = fixture.panTilt->getRecenterTarget();
    REQUIRE(target.x == Catch::Approx(90.0f).margin(1e-3f));
    REQUIRE(target.y == Catch::Approx(0.0f).margin(1e-3f));

    fixture.panTilt->recenterTarget = PanTilt::RecenterTargetMode::AxisCenter;
    fixture.panTilt->panAxis.center = 12.0f;
    fixture.panTilt->tiltAxis.center = -8.0f;
    target = fixture.panTilt->getRecenterTarget();
    REQUIRE(target.x == 12.0f);
    REQUIRE(target.y == -8.0f);
}

TEST_CASE("PanTilt recenters both axes and emits rotation damping bypass", "[unit][pan-tilt]")
{
    PanTiltFixture fixture;
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::World;
    fixture.panTilt->panAxis.recentering.enabled = true;
    fixture.panTilt->panAxis.recentering.wait = 0.0f;
    fixture.panTilt->panAxis.recentering.time = 0.5f;
    fixture.panTilt->tiltAxis.recentering.enabled = true;
    fixture.panTilt->tiltAxis.recentering.wait = 0.0f;
    fixture.panTilt->tiltAxis.recentering.time = 0.5f;
    fixture.panTilt->panAxis.value = 30.0f;
    fixture.panTilt->tiltAxis.value = 20.0f;

    fixture.step();
    fixture.step();
    REQUIRE(fixture.panTilt->panAxis.value < 30.0f);
    REQUIRE(fixture.panTilt->panAxis.value > 0.0f);
    REQUIRE(fixture.panTilt->tiltAxis.value < 20.0f);
    REQUIRE(fixture.panTilt->tiltAxis.value > 0.0f);

    fixture.panTilt->panAxis.value = 15.0f;
    fixture.step();
    REQUIRE(math::angle(
        fixture.camera->state().rotationDampingBypass,
        math::Quat::identity()) > math::Epsilon);
}

TEST_CASE("PanTilt infers axes on force position and transition inheritance", "[unit][pan-tilt]")
{
    PanTiltFixture fixture;
    fixture.panTilt->referenceFrame = PanTilt::ReferenceFrame::World;

    fixture.camera->forceCameraPosition({}, yaw(90.0f));
    REQUIRE(fixture.panTilt->panAxis.value == Catch::Approx(-90.0f).margin(1e-3f));
    REQUIRE(fixture.panTilt->tiltAxis.value == Catch::Approx(0.0f).margin(1e-3f));
    fixture.step();
    REQUIRE(math::approximately(
        fixture.forward(),
        math::rotate(yaw(90.0f), math::Vec3::forward),
        1e-3f));

    fixture.camera->blendHint = BlendHint::InheritPosition;
    const CameraId fromId = fixture.system.createCamera<VirtualCamera>("from");
    VirtualCamera* from = fixture.system.camera(fromId);
    from->forceCameraPosition({}, yaw(-45.0f));
    fixture.camera->onTransitionFromCamera(from, math::Vec3::up, 1.0f / 60.0f);
    REQUIRE(fixture.panTilt->panAxis.value == Catch::Approx(45.0f).margin(1e-3f));
}

TEST_CASE("InputAxisController discovers and drives PanTilt", "[unit][pan-tilt]")
{
    PanTiltFixture fixture;
    InputAxisController controller;
    REQUIRE(controller.synchronize(*fixture.camera) == 2);

    auto* pan = controller.find("Look X (Pan)");
    auto* tilt = controller.find("Look Y (Tilt)");
    REQUIRE(pan != nullptr);
    REQUIRE(tilt != nullptr);
    REQUIRE(pan->hint == InputAxisHint::X);
    REQUIRE(tilt->hint == InputAxisHint::Y);
    REQUIRE(controller.bind("Look X (Pan)", "Horizontal"));
    REQUIRE(controller.bind("Look Y (Tilt)", "Vertical"));

    TestInputSource source;
    source.horizontal = 1.0f;
    source.vertical = -1.0f;
    controller.update(source, 0.1f);
    REQUIRE(fixture.panTilt->panAxis.value > 0.0f);
    REQUIRE(fixture.panTilt->tiltAxis.value < 0.0f);
}


