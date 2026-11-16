#include "brain_test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/follow.hpp>
#include <cinerig/components/hard_lock_to_target.hpp>
#include <cinerig/components/orbital_follow.hpp>
#include <cinerig/components/position_composer.hpp>
#include <cinerig/components/third_person_follow.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct PositionParityFixture
{
    PositionParityFixture()
    {
        rig.world.add(target, Transform{});
        camera = rig.addCamera("camera", 10, 0.0f);
        rig.camera(camera).targets.tracking = TargetRef::entity(target);
    }

    void moveTarget(math::Vec3 position)
    {
        rig.world.set(target, Transform{position, math::Quat::identity()});
    }

    BrainTestRig rig;
    CameraId camera = 0;
    EntityId target = 900;
};

} // namespace

TEST_CASE("A camera without a body does not follow its target", "[parity][camera-position]")
{
    PositionParityFixture fixture;
    fixture.moveTarget({10.0f, 0.0f, 0.0f});
    fixture.rig.step(0.1f);

    REQUIRE(math::approximately(
        fixture.rig.output.applied.state.rawPosition,
        {0.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("Follow with zero offset and damping lands on the target", "[parity][camera-position]")
{
    PositionParityFixture fixture;
    auto& follow = fixture.rig.camera(fixture.camera).setBody<Follow>();
    follow.followOffset = math::Vec3::zero;
    follow.trackerSettings.positionDamping = math::Vec3::zero;

    fixture.moveTarget({10.0f, 0.0f, 0.0f});
    fixture.rig.step(0.1f);

    REQUIRE(math::approximately(
        fixture.rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("HardLockToTarget lands on the target", "[parity][camera-position]")
{
    PositionParityFixture fixture;
    fixture.rig.camera(fixture.camera).setBody<HardLockToTarget>();

    fixture.moveTarget({10.0f, 0.0f, 0.0f});
    fixture.rig.step(0.1f);

    REQUIRE(math::approximately(
        fixture.rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("OrbitalFollow with zero radius lands on the target", "[parity][camera-position]")
{
    PositionParityFixture fixture;
    auto& orbital = fixture.rig.camera(fixture.camera).setBody<OrbitalFollow>();
    orbital.radius = 0.0f;
    orbital.trackerSettings.positionDamping = math::Vec3::zero;
    orbital.trackerSettings.rotationDamping = math::Vec3::zero;
    orbital.trackerSettings.quaternionDamping = 0.0f;

    fixture.moveTarget({10.0f, 0.0f, 0.0f});
    fixture.rig.step(0.1f);

    REQUIRE(math::approximately(
        fixture.rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 0.0f},
        1e-4f));
}

TEST_CASE("PositionComposer places the camera at the configured distance", "[parity][camera-position]")
{
    PositionParityFixture fixture;
    auto& composer = fixture.rig.camera(fixture.camera).setBody<PositionComposer>();
    composer.damping = math::Vec3::zero;
    composer.cameraDistance = 1.0f;

    fixture.moveTarget({10.0f, 0.0f, 0.0f});
    fixture.rig.step(0.1f);

    REQUIRE(math::approximately(
        fixture.rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 1.0f},
        1e-4f));
}

TEST_CASE("ThirdPersonFollow with a zero rig lands on the target", "[parity][camera-position]")
{
    PositionParityFixture fixture;
    auto& follow = fixture.rig.camera(fixture.camera).setBody<ThirdPersonFollow>();
    follow.damping = math::Vec3::zero;
    follow.shoulderOffset = math::Vec3::zero;
    follow.verticalArmLength = 0.0f;
    follow.cameraDistance = 0.0f;

    fixture.moveTarget({10.0f, 0.0f, 0.0f});
    fixture.rig.step(0.1f);

    REQUIRE(math::approximately(
        fixture.rig.output.applied.state.rawPosition,
        {10.0f, 0.0f, 0.0f},
        1e-4f));
}
