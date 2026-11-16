#include "fake_entity_world.hpp"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/orbital_follow.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct OrbitalFixture
{
    OrbitalFixture()
    {
        world.add(target, Transform{});
        world.add(cameraEntity, Transform{
            {0.0f, 0.0f, 10.0f},
            math::Quat::identity(),
            math::Vec3::one,
        });
        const CameraId id = system.createCamera<VirtualCamera>("orbital");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(target);
        camera->ownerEntity = cameraEntity;
        orbital = &camera->setBody<OrbitalFollow>();
        orbital->trackerSettings.positionDamping = math::Vec3::zero;
        orbital->trackerSettings.rotationDamping = math::Vec3::zero;
        orbital->trackerSettings.quaternionDamping = 0.0f;
    }

    void setTarget(
        math::Vec3 position,
        math::Quat rotation = math::Quat::identity())
    {
        world.set(target, Transform{position, rotation, math::Vec3::one});
    }

    void step(float deltaTime = 1.0f / 60.0f, bool advanceTime = true)
    {
        if (advanceTime)
            time += deltaTime;
        world.setTime(time);
        camera->updateCameraState(math::Vec3::up, deltaTime);
    }

    math::Vec3 position() const
    {
        return camera->state().rawPosition;
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    OrbitalFollow* orbital = nullptr;
    EntityId target = 1;
    EntityId cameraEntity = 2;
    float time = 0.0f;
};

math::Quat yaw(float degrees)
{
    return math::quatFromAxisAngle(math::Vec3::up, degrees * math::DegToRad);
}

math::Quat pitchYaw(float verticalDegrees, float horizontalDegrees)
{
    return math::quatFromEuler({
        verticalDegrees * math::DegToRad,
        horizontalDegrees * math::DegToRad,
        0.0f,
    });
}

math::Vec3 sphereOffset(float radius, float verticalDegrees, float horizontalDegrees)
{
    return math::rotate(
        pitchYaw(verticalDegrees, horizontalDegrees),
        math::Vec3{0.0f, 0.0f, -radius});
}

} // namespace

TEST_CASE("OrbitSplineCache reports changes and interpolates the three rings", "[unit][orbital-follow]")
{
    OrbitSplineCache cache;
    const OrbitRigSettings settings;
    REQUIRE(cache.settingsChanged(settings));
    cache.update(settings);
    REQUIRE_FALSE(cache.settingsChanged(settings));
    REQUIRE(cache.isValid());

    REQUIRE(math::approximately(
        cache.splineValue(0.0f),
        {0.0f, 0.1f, -2.5f, -1.0f},
        1e-3f));
    REQUIRE(math::approximately(
        cache.splineValue(0.5f),
        {0.0f, 2.25f, -4.0f, 0.0f},
        1e-3f));
    REQUIRE(math::approximately(
        cache.splineValue(1.0f),
        {0.0f, 5.0f, -2.0f, 1.0f},
        1e-3f));

    OrbitRigSettings changed = settings;
    changed.top.radius = 3.0f;
    REQUIRE(cache.settingsChanged(changed));
    cache.update(changed);
    REQUIRE(cache.splineValue(1.0f).z == Catch::Approx(-3.0f));
}

TEST_CASE("OrbitalFollow exposes the original defaults and body settings", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    REQUIRE(fixture.orbital->orbitStyle == OrbitalFollow::OrbitStyle::Sphere);
    REQUIRE(fixture.orbital->radius == 10.0f);
    REQUIRE(fixture.orbital->horizontalAxis.range.x == -180.0f);
    REQUIRE(fixture.orbital->horizontalAxis.range.y == 180.0f);
    REQUIRE(fixture.orbital->horizontalAxis.wrap);
    REQUIRE(fixture.orbital->verticalAxis.value == 17.5f);
    REQUIRE(fixture.orbital->verticalAxis.range.x == -10.0f);
    REQUIRE(fixture.orbital->verticalAxis.range.y == 45.0f);
    REQUIRE(fixture.orbital->radialAxis.value == 1.0f);
    REQUIRE(fixture.orbital->radialAxis.range.x == 1.0f);
    REQUIRE(fixture.orbital->radialAxis.range.y == 1.0f);
    REQUIRE(fixture.orbital->stage() == Stage::Body);
    REQUIRE_FALSE(fixture.orbital->bodyAppliesAfterAim());
    OrbitalFollow defaults;
    REQUIRE(defaults.maxDampTime() == 1.0f);
    REQUIRE(fixture.orbital->maxDampTime() == 0.0f);
    REQUIRE(fixture.orbital->isValid());
}

TEST_CASE("OrbitalFollow places the camera on the sphere", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.setTarget({1.0f, 2.0f, 3.0f});
    fixture.step();

    const math::Vec3 expected = fixture.world.transform(fixture.target).position
        + sphereOffset(10.0f, 17.5f, 0.0f);
    REQUIRE(math::approximately(fixture.position(), expected, 1e-3f));

    fixture.orbital->verticalAxis.value = 0.0f;
    fixture.orbital->horizontalAxis.value = 90.0f;
    fixture.orbital->radius = 5.0f;
    fixture.step();

    const math::Vec3 side = fixture.world.transform(fixture.target).position
        + sphereOffset(5.0f, 0.0f, 90.0f);
    REQUIRE(math::approximately(fixture.position(), side, 1e-3f));

    fixture.orbital->radialAxis.range = {1.0f, 2.0f};
    fixture.orbital->radialAxis.value = 2.0f;
    fixture.step();
    const math::Vec3 zoomed = fixture.world.transform(fixture.target).position
        + sphereOffset(10.0f, 0.0f, 90.0f);
    REQUIRE(math::approximately(fixture.position(), zoomed, 1e-3f));
}

TEST_CASE("OrbitalFollow three-ring placement follows the cached surface", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.orbital->orbitStyle = OrbitalFollow::OrbitStyle::ThreeRing;
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.orbital->horizontalAxis.value = 0.0f;

    fixture.orbital->verticalAxis.value = fixture.orbital->verticalAxis.range.x;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {0.0f, 0.1f, -2.5f},
        1e-3f));

    fixture.orbital->verticalAxis.value = fixture.orbital->verticalAxis.center;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {0.0f, 2.25f, -4.0f},
        1e-3f));

    fixture.orbital->verticalAxis.value = fixture.orbital->verticalAxis.range.y;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {0.0f, 5.0f, -2.0f},
        1e-3f));

    fixture.orbital->orbits.center.radius = 8.0f;
    fixture.orbital->verticalAxis.value = fixture.orbital->verticalAxis.center;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {0.0f, 2.25f, -8.0f},
        1e-3f));

    fixture.orbital->horizontalAxis.value = 90.0f;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {-8.0f, 2.25f, 0.0f},
        1e-3f));
}

TEST_CASE("OrbitalFollow respects binding modes and target offsets", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.setTarget({0.0f, 0.0f, 0.0f}, yaw(90.0f));
    fixture.orbital->verticalAxis.value = 0.0f;
    fixture.orbital->horizontalAxis.value = 0.0f;
    fixture.orbital->radius = 5.0f;

    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {0.0f, 0.0f, -5.0f},
        1e-3f));

    fixture.orbital->trackerSettings.bindingMode = BindingMode::LockToTarget;
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {-5.0f, 0.0f, 0.0f},
        1e-3f));

    fixture.orbital->trackerSettings.bindingMode = BindingMode::LockToTarget;
    fixture.orbital->targetOffset = {0.0f, 0.0f, -1.0f};
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        {-6.0f, 0.0f, 0.0f},
        1e-3f));

    fixture.orbital->trackerSettings.bindingMode = BindingMode::LazyFollow;
    fixture.orbital->targetOffset = math::Vec3::zero;
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.step();
    REQUIRE(fixture.orbital->horizontalAxis.value == 0.0f);
    REQUIRE(fixture.position().z < 0.0f);
}

TEST_CASE("OrbitalFollow updates horizontal recentering from the tracking target", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.setTarget({0.0f, 0.0f, 0.0f}, yaw(45.0f));
    fixture.orbital->horizontalAxis.value = 90.0f;
    fixture.orbital->horizontalAxis.recentering.enabled = true;
    fixture.orbital->horizontalAxis.recentering.wait = 0.0f;
    fixture.orbital->horizontalAxis.recentering.time = 0.5f;
    fixture.orbital->verticalAxis.recentering.enabled = false;

    fixture.step(1.0f / 60.0f);
    REQUIRE(fixture.orbital->horizontalAxis.value == 90.0f);

    fixture.step(1.0f / 60.0f);
    REQUIRE(math::approximately(fixture.orbital->horizontalAxis.center, 45.0f, 1e-3f));
    REQUIRE(fixture.orbital->horizontalAxis.value < 90.0f);
    REQUIRE(fixture.orbital->horizontalAxis.value > 45.0f);
}

TEST_CASE("OrbitalFollow handles target warps and force-position history", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.orbital->verticalAxis.value = 20.0f;
    fixture.orbital->horizontalAxis.value = 45.0f;
    fixture.orbital->radius = 5.0f;
    fixture.step();

    const math::Vec3 beforeWarp = fixture.position();
    fixture.setTarget({10.0f, 0.0f, 0.0f});
    fixture.camera->onTargetObjectWarped(fixture.target, {10.0f, 0.0f, 0.0f});
    fixture.step();
    REQUIRE(math::approximately(
        fixture.position(),
        beforeWarp + math::Vec3{10.0f, 0.0f, 0.0f},
        1e-3f));

    const float horizontal = fixture.orbital->horizontalAxis.value;
    const float vertical = fixture.orbital->verticalAxis.value;
    const math::Vec3 forced = fixture.position();
    fixture.camera->forceCameraPosition(forced, fixture.camera->state().rawOrientation);
    fixture.step();
    REQUIRE(math::approximately(fixture.position(), forced, 1e-2f));
    REQUIRE(fixture.orbital->horizontalAxis.value == Catch::Approx(horizontal).margin(1e-2f));
    REQUIRE(fixture.orbital->verticalAxis.value == Catch::Approx(vertical).margin(1e-2f));
}

TEST_CASE("OrbitalFollow three-ring force position preserves its axes", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.orbital->orbitStyle = OrbitalFollow::OrbitStyle::ThreeRing;
    fixture.orbital->verticalAxis.range = {-10.0f, 45.0f};
    fixture.orbital->verticalAxis.center = 17.5f;
    fixture.orbital->horizontalAxis.range = {0.0f, 90.0f};
    fixture.orbital->verticalAxis.value = 25.0f;
    fixture.orbital->horizontalAxis.value = 35.0f;
    fixture.setTarget({1.0f, 2.0f, 3.0f});
    fixture.step();

    const float horizontal = fixture.orbital->horizontalAxis.value;
    const float vertical = fixture.orbital->verticalAxis.value;
    const math::Vec3 forced = fixture.position();
    fixture.camera->forceCameraPosition(forced, fixture.camera->state().rawOrientation);
    fixture.step();

    REQUIRE(math::approximately(fixture.position(), forced, 5e-2f));
    REQUIRE(fixture.orbital->horizontalAxis.value == Catch::Approx(horizontal).margin(5.0f));
    REQUIRE(fixture.orbital->verticalAxis.value == Catch::Approx(vertical).margin(5.0f));
}

TEST_CASE("OrbitalFollow InheritPosition keeps the incoming pose and infers axes", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.camera->blendHint = BlendHint::InheritPosition;
    fixture.step();

    const math::Vec3 inherited = sphereOffset(10.0f, 35.0f, 25.0f);
    VirtualCamera outgoing("outgoing");
    outgoing.forceCameraPosition(inherited, math::Quat::identity());

    REQUIRE(fixture.orbital->onTransitionFromCamera(
        &outgoing,
        math::Vec3::up,
        1.0f / 60.0f));
    fixture.step();

    REQUIRE(math::approximately(fixture.position(), inherited, 1e-2f));
}

TEST_CASE("OrbitalFollow snaps on negative time and reports invalid targets", "[unit][orbital-follow]")
{
    OrbitalFixture fixture;
    fixture.setTarget({0.0f, 0.0f, 0.0f});
    fixture.orbital->trackerSettings.positionDamping = math::Vec3{1.0f};
    fixture.step();

    fixture.setTarget({10.0f, 0.0f, 0.0f});
    fixture.step(-1.0f);
    const math::Vec3 expected = fixture.world.transform(fixture.target).position
        + sphereOffset(10.0f, 17.5f, 0.0f);
    REQUIRE(math::approximately(fixture.position(), expected, 1e-3f));

    fixture.world.remove(fixture.target);
    fixture.step();
    REQUIRE_FALSE(fixture.orbital->isValid());
    REQUIRE_FALSE(std::isnan(fixture.position().x));
}
