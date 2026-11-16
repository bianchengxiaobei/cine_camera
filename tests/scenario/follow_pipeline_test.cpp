#include "fake_camera_output.hpp"
#include "fake_entity_world.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/follow.hpp>
#include <cinerig/components/rotation_composer.hpp>
#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

TEST_CASE("Follow and composer drive a virtual camera through the runtime", "[scenario][follow]")
{
    FakeEntityWorld world;
    world.setFrame(UpdateClock::Late, 1);
    world.setDeltaTime(UpdateClock::Late, 1.0f / 60.0f);
    world.add(1, Transform{{2.0f, 1.0f, 3.0f}, math::Quat::identity(), math::Vec3::one});

    FakeCameraOutput output;
    output.setAspect(16.0f / 9.0f);
    CameraSystem system(world);
    const BrainId brainId = system.createBrain(output, BrainConfig{});
    const CameraId cameraId = system.createCamera<VirtualCamera>("follow");
    auto* camera = system.camera(cameraId);
    camera->targets.tracking = TargetRef::entity(1);
    camera->lens.fieldOfView = 60.0f;

    auto& follow = camera->setBody<Follow>();
    follow.followOffset = {0.0f, 2.0f, -6.0f};
    follow.trackerSettings.positionDamping = math::Vec3::zero;
    auto& composer = camera->setAim<RotationComposer>();
    composer.damping = math::Vec2::zero;

    system.lateUpdate(1.0f / 60.0f);

    REQUIRE(system.brain(brainId)->activeVirtualCamera() == camera);
    REQUIRE(output.applyCount == 1);
    REQUIRE(math::approximately(output.applied.state.rawPosition, {2.0f, 3.0f, -3.0f}));
    const math::Vec3 forward = output.applied.state.rawOrientation * math::Vec3::forward;
    REQUIRE(math::dot(forward, math::Vec3{0.0f, -2.0f, 6.0f}.normalized())
        == Catch::Approx(1.0f).margin(1e-4f));
}
