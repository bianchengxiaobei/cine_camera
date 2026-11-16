#include "sdl_camera_output.hpp"
#include "sdl_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/components/follow.hpp>
#include <cinerig/components/rotation_composer.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::examples;

namespace
{

CameraId createFollowCamera(
    CameraSystem& system,
    SdlWorld& world,
    const char* name,
    EntityId target,
    math::Vec3 targetPosition,
    math::Vec3 offset,
    float fieldOfView,
    int priority)
{
    const CameraId id = system.createCamera<VirtualCamera>(name);
    VirtualCamera* camera = system.camera(id);
    REQUIRE(camera != nullptr);

    camera->targets.tracking = TargetRef::entity(target);
    camera->lens.fieldOfView = fieldOfView;
    camera->lens.nearClipPlane = 0.1f;
    camera->lens.farClipPlane = 120.0f;
    camera->lens.aspect = 16.0f / 9.0f;
    camera->priority = PrioritySettings(priority);
    camera->standbyUpdate = StandbyUpdateMode::Always;

    auto& follow = camera->setBody<Follow>();
    follow.followOffset = offset;
    follow.trackerSettings.positionDamping = math::Vec3::zero;

    auto& composer = camera->setAim<RotationComposer>();
    composer.damping = math::Vec2::zero;

    Transform transform;
    transform.position = targetPosition + offset;
    transform.rotation = math::lookRotation(targetPosition - transform.position, world.worldUp());
    camera->ownerEntity = world.createEntity(transform);
    return id;
}

} // namespace

TEST_CASE("Blend example switches two follow cameras with EaseInOut", "[examples][blend]")
{
    SdlWorld world;
    world.setViewportSize({1280.0f, 720.0f});
    world.setDeltaTime(UpdateClock::Late, 1.0f / 60.0f);

    const math::Vec3 targetPosition{0.0f, 1.2f, 0.0f};
    const EntityId target = world.createEntity({
        targetPosition,
        math::Quat::identity(),
        math::Vec3::one,
    });

    SdlCameraOutput output;
    output.setViewportSize(world.viewportSize());

    CameraSystem system(world);
    system.setUnitTestMode(true);
    BrainConfig config;
    config.defaultBlend = {BlendStyle::EaseInOut, 2.0f};
    const BrainId brainId = system.createBrain(output, config);
    const CameraBrain* brain = system.brain(brainId);
    REQUIRE(brain != nullptr);

    const CameraId wideId = createFollowCamera(
        system, world, "Wide", target, targetPosition, {0.0f, 3.0f, -12.0f}, 55.0f, 10);
    const CameraId closeId = createFollowCamera(
        system, world, "Close", target, targetPosition, {1.5f, 1.8f, -5.0f}, 35.0f, 0);

    VirtualCamera* wide = system.camera(wideId);
    VirtualCamera* close = system.camera(closeId);
    REQUIRE(wide != nullptr);
    REQUIRE(close != nullptr);
    output.setDefaultLens(wide->lens);
    output.setAspect(16.0f / 9.0f);

    std::uint64_t frame = 0;
    auto step = [&](float deltaTime)
    {
        ++frame;
        world.setFrame(UpdateClock::Late, frame);
        wide->lens.aspect = 16.0f / 9.0f;
        close->lens.aspect = 16.0f / 9.0f;
        system.lateUpdate(deltaTime);
    };

    step(0.0f);
    REQUIRE(brain->activeVirtualCamera() == wide);
    REQUIRE_FALSE(brain->isBlending());
    REQUIRE(math::approximately(output.state().finalPosition(), targetPosition + math::Vec3{0.0f, 3.0f, -12.0f}, 1e-3f));

    wide->priority = PrioritySettings(0);
    close->priority = PrioritySettings(20);
    step(1.0f);

    REQUIRE(brain->activeVirtualCamera() == close);
    REQUIRE(brain->isBlending());
    const Blend* blend = brain->activeBlend();
    REQUIRE(blend != nullptr);
    REQUIRE(blend->blendWeight() == Catch::Approx(0.5f));
    REQUIRE(output.outputLens().fieldOfView > 35.0f);
    REQUIRE(output.outputLens().fieldOfView < 55.0f);
    const math::Vec3 midpoint = (targetPosition + math::Vec3{0.0f, 3.0f, -12.0f})
        + ((targetPosition + math::Vec3{1.5f, 1.8f, -5.0f})
            - (targetPosition + math::Vec3{0.0f, 3.0f, -12.0f})) * 0.5f;
    REQUIRE(math::approximately(output.state().finalPosition(), midpoint, 1e-3f));

    step(1.0f);
    REQUIRE_FALSE(brain->isBlending());
    REQUIRE(brain->activeVirtualCamera() == close);
    REQUIRE(math::approximately(output.state().finalPosition(), targetPosition + math::Vec3{1.5f, 1.8f, -5.0f}, 1e-3f));
    REQUIRE(output.outputLens().fieldOfView == Catch::Approx(35.0f));
}
