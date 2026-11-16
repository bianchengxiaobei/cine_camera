#include <cmath>
#include <cstdint>

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

TEST_CASE("Follow example scene keeps the camera at the target offset", "[examples][follow]")
{
    SdlWorld world;
    world.setViewportSize({1280.0f, 720.0f});
    world.setDeltaTime(UpdateClock::Late, 1.0f / 60.0f);

    const EntityId target = world.createEntity({
        {0.0f, 1.2f, 0.0f},
        math::Quat::identity(),
        math::Vec3::one,
    });

    SdlCameraOutput output;
    output.setViewportSize({1280.0f, 720.0f});

    CameraSystem system(world);
    BrainConfig brainConfig;
    brainConfig.defaultBlend = {BlendStyle::Cut, 0.0f};
    const BrainId brainId = system.createBrain(output, brainConfig);

    const CameraId cameraId = system.createCamera<VirtualCamera>("Follow Camera");
    auto* camera = system.camera(cameraId);
    camera->targets.tracking = TargetRef::entity(target);
    camera->lens.fieldOfView = 55.0f;
    camera->lens.aspect = 16.0f / 9.0f;

    auto& follow = camera->setBody<Follow>();
    follow.followOffset = {0.0f, 2.0f, -6.0f};
    follow.trackerSettings.positionDamping = math::Vec3::zero;
    auto& composer = camera->setAim<RotationComposer>();
    composer.damping = math::Vec2::zero;

    for (int frame = 1; frame <= 60; ++frame)
    {
        const math::Vec3 targetPosition{
            3.0f * std::sin(static_cast<float>(frame) * 0.05f),
            1.2f,
            1.5f * std::sin(static_cast<float>(frame) * 0.03f),
        };
        world.setTransform(target, {
            targetPosition,
            math::Quat::identity(),
            math::Vec3::one,
        });
        world.setFrame(UpdateClock::Late, static_cast<std::uint64_t>(frame));
        system.lateUpdate(1.0f / 60.0f);

        REQUIRE(output.hasCamera());
        REQUIRE(system.brain(brainId)->activeVirtualCamera() == camera);
        const math::Vec3 cameraPosition = output.state().finalPosition();
        REQUIRE(math::approximately(cameraPosition, targetPosition + follow.followOffset, 1e-3f));
    }
}
