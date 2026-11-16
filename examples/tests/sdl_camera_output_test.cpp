#include <catch2/catch_all.hpp>

#include <cmath>

#include <cinerig/cinerig.hpp>

#include "sdl_camera_output.hpp"
#include "sdl_world.hpp"

using namespace cinerig;
using namespace cinerig::examples;

namespace
{

AppliedCamera makeApplied(const CameraState& state, float aspect = 1.0f)
{
    AppliedCamera applied;
    applied.state = state;
    applied.aspect = aspect;
    applied.hasCamera = true;
    return applied;
}

}

TEST_CASE("SdlCameraOutput builds an OpenGL view matrix", "[examples][camera-output]")
{
    CameraState state = CameraState::makeDefault();
    state.rawPosition = {0.0f, 0.0f, 5.0f};
    state.rawOrientation = math::Quat::identity();

    SdlCameraOutput output;
    output.apply(makeApplied(state));

    REQUIRE(output.hasCamera());
    REQUIRE(math::approximately(output.position(), math::Vec3(0.0f, 0.0f, 5.0f)));

    const math::Mat4 view = output.viewMatrix();
    const math::Vec3 origin = view.transformPoint(math::Vec3::zero);
    REQUIRE(origin.x == Catch::Approx(0.0f));
    REQUIRE(origin.y == Catch::Approx(0.0f));
    REQUIRE(origin.z == Catch::Approx(-5.0f));

    const math::Vec3 cameraOrigin = view.transformPoint(math::Vec3(0.0f, 0.0f, 5.0f));
    REQUIRE(cameraOrigin.x == Catch::Approx(0.0f));
    REQUIRE(cameraOrigin.y == Catch::Approx(0.0f));
    REQUIRE(cameraOrigin.z == Catch::Approx(0.0f));
}

TEST_CASE("SdlCameraOutput perspective projection has correct clip range", "[examples][camera-output]")
{
    CameraState state = CameraState::makeDefault();
    state.lens.fieldOfView = 60.0f;
    state.lens.nearClipPlane = 0.1f;
    state.lens.farClipPlane = 100.0f;
    state.lens.orthographic = false;

    SdlCameraOutput output;
    output.apply(makeApplied(state, 1.5f));

    const math::Mat4 projection = output.projectionMatrix();
    const math::Vec3 nearPoint = projection.transformPoint({0.0f, 0.0f, -0.1f});
    const math::Vec3 farPoint = projection.transformPoint({0.0f, 0.0f, -100.0f});

    REQUIRE(nearPoint.z == Catch::Approx(-1.0f));
    REQUIRE(farPoint.z == Catch::Approx(1.0f));
    REQUIRE(projection.at(0, 0) == Catch::Approx(projection.at(1, 1) / 1.5f));
    REQUIRE(output.outputLens().aspect == Catch::Approx(1.5f));
}

TEST_CASE("SdlCameraOutput orthographic projection uses half height and aspect", "[examples][camera-output]")
{
    CameraState state = CameraState::makeDefault();
    state.lens.orthographic = true;
    state.lens.orthographicSize = 2.0f;
    state.lens.nearClipPlane = 0.1f;
    state.lens.farClipPlane = 100.0f;

    SdlCameraOutput output;
    output.apply(makeApplied(state, 2.0f));

    const math::Mat4 projection = output.projectionMatrix();
    const math::Vec3 edge = projection.transformPoint({4.0f, 2.0f, -0.1f});
    REQUIRE(edge.x == Catch::Approx(1.0f));
    REQUIRE(edge.y == Catch::Approx(1.0f));
    REQUIRE(edge.z == Catch::Approx(-1.0f));

    output.setProjectionMode(LensSettings::OverrideMode::Perspective);
    REQUIRE(output.projectionMatrix().at(3, 2) == Catch::Approx(-1.0f));
}

TEST_CASE("SdlCameraOutput applies Dutch through final orientation", "[examples][camera-output]")
{
    CameraState state = CameraState::makeDefault();
    state.rawOrientation = math::Quat::identity();
    state.lens.dutch = 90.0f;

    SdlCameraOutput output;
    output.apply(makeApplied(state));

    const math::Mat4 view = output.viewMatrix();
    const math::Vec3 cameraUpInWorld = output.orientation() * math::Vec3::up;
    const math::Vec3 cameraSpaceUp = view.transformVector(cameraUpInWorld);

    REQUIRE(cameraSpaceUp.x == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(cameraSpaceUp.y == Catch::Approx(1.0f));
    REQUIRE(cameraSpaceUp.z == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("SdlCameraOutput honors NoTransform and NoLens hints", "[examples][camera-output]")
{
    SdlCameraOutput output;

    CameraState first = CameraState::makeDefault();
    first.rawPosition = {1.0f, 2.0f, 3.0f};
    first.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, 0.5f);
    first.lens.fieldOfView = 55.0f;
    output.apply(makeApplied(first));

    CameraState second = CameraState::makeDefault();
    second.rawPosition = {9.0f, 9.0f, 9.0f};
    second.rawOrientation = math::Quat::identity();
    second.lens.fieldOfView = 90.0f;
    second.blendHint = BlendHint::NoTransform | BlendHint::NoLens;
    output.apply(makeApplied(second));

    REQUIRE(math::approximately(output.position(), math::Vec3(1.0f, 2.0f, 3.0f)));
    REQUIRE(output.outputLens().fieldOfView == Catch::Approx(55.0f));
}

TEST_CASE("SdlCameraOutput keeps the previous frame without an active camera", "[examples][camera-output]")
{
    SdlCameraOutput output;
    CameraState state = CameraState::makeDefault();
    state.rawPosition = {3.0f, 4.0f, 5.0f};
    output.apply(makeApplied(state));

    const math::Mat4 view = output.viewMatrix();
    const math::Mat4 projection = output.projectionMatrix();

    AppliedCamera inactive;
    inactive.hasCamera = false;
    output.apply(inactive);

    REQUIRE_FALSE(output.hasCamera());
    REQUIRE(math::approximately(output.position(), math::Vec3(3.0f, 4.0f, 5.0f)));
    REQUIRE(output.viewMatrix().m == view.m);
    REQUIRE(output.projectionMatrix().m == projection.m);
}

TEST_CASE("SdlWorld and SdlCameraOutput drive a CameraSystem brain", "[examples][camera-output]")
{
    SdlWorld world;
    world.setViewportSize({800.0f, 600.0f});

    const EntityId targetEntity = world.createEntity({
        {0.0f, 1.0f, 0.0f},
        math::Quat::identity(),
        math::Vec3::one,
    });
    const math::Vec3 cameraPosition{0.0f, 3.0f, 8.0f};
    Transform cameraTransform;
    cameraTransform.position = cameraPosition;
    cameraTransform.rotation =
        math::lookRotation(math::Vec3{0.0f, 1.0f, 0.0f} - cameraPosition, world.worldUp());
    const EntityId cameraEntity = world.createEntity(cameraTransform);

    SdlCameraOutput output;
    output.setViewportSize(world.viewportSize());

    CameraSystem system(world);
    BrainConfig config;
    config.defaultBlend = {BlendStyle::Cut, 0.0f};
    const BrainId brainId = system.createBrain(output, config);

    const CameraId cameraId = system.createCamera<ExternalCamera>();
    auto* camera = static_cast<ExternalCamera*>(system.camera(cameraId));
    REQUIRE(camera != nullptr);
    camera->cameraEntity = cameraEntity;
    camera->lookAtTarget = TargetRef::entity(targetEntity);
    camera->output = &output;
    camera->priority.setValue(10);

    world.setDeltaTime(UpdateClock::Late, 1.0f / 60.0f);
    world.setFrame(UpdateClock::Late, 1);
    system.lateUpdate(1.0f / 60.0f);

    REQUIRE(output.hasCamera());
    REQUIRE(math::approximately(output.position(), cameraPosition));
    REQUIRE(output.aspect() == Catch::Approx(800.0f / 600.0f));

    const CameraBrain* brain = system.brain(brainId);
    REQUIRE(brain != nullptr);
    REQUIRE(brain->activeVirtualCamera() == camera);
    REQUIRE_FALSE(brain->isBlending());
}
