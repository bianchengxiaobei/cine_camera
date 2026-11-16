#include "fake_entity_world.hpp"
#include "fake_physics_world.hpp"

#include <memory>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/confiner3d.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

struct ConfinerFixture
{
    ConfinerFixture()
    {
        world.add(
            follow,
            Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("confiner");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(follow);

        auto owned = std::make_unique<Confiner3D>();
        confiner = owned.get();
        camera->addExtension(std::move(owned));
    }

    CameraState makeState(math::Vec3 cameraPosition) const
    {
        CameraState state = CameraState::makeDefault();
        state.rawPosition = cameraPosition;
        state.rawOrientation = math::Quat::identity();
        state.referenceLookAt = {0.0f, 0.0f, 0.0f};
        state.referenceUp = math::Vec3::up;
        return state;
    }

    void body(CameraState& state, float deltaTime = 1.0f / 60.0f)
    {
        confiner->postPipelineStageCallback(*camera, Stage::Body, state, deltaTime);
    }

    void addVolume(EntityId id = 10, math::Vec3 extents = {5.0f, 5.0f, 5.0f})
    {
        physics.boundsById[id] = Bounds{{0.0f, 0.0f, 0.0f}, extents};
    }

    FakeEntityWorld world;
    FakePhysicsWorld physics;
    CameraSystem system{world, &physics};
    VirtualCamera* camera = nullptr;
    Confiner3D* confiner = nullptr;
    EntityId follow = 1;
    EntityId volume = 10;
};

} // namespace

TEST_CASE("Confiner3D exposes original defaults and validation", "[unit][confiner3d]")
{
    Confiner3D confiner;
    REQUIRE(confiner.boundingVolume == 0);
    REQUIRE(confiner.slowingDistance == 0.0f);
    REQUIRE_FALSE(confiner.isValid());
    REQUIRE(confiner.maxDampTime() == 0.0f);

    confiner.boundingVolume = 4;
    confiner.slowingDistance = 2.5f;
    REQUIRE(confiner.isValid());
    REQUIRE(confiner.maxDampTime() == Catch::Approx(0.5f));
    confiner.slowingDistance = -1.0f;
    confiner.validate();
    REQUIRE(confiner.slowingDistance == 0.0f);

    confiner.reset();
    REQUIRE(confiner.boundingVolume == 0);
    REQUIRE(confiner.slowingDistance == 0.0f);
}

TEST_CASE("Confiner3D leaves points inside the volume unchanged", "[unit][confiner3d]")
{
    ConfinerFixture fixture;
    fixture.addVolume();
    fixture.confiner->boundingVolume = fixture.volume;

    CameraState state = fixture.makeState({3.0f, 2.0f, -1.0f});
    fixture.body(state);

    REQUIRE(math::approximately(state.correctedPosition(), {3.0f, 2.0f, -1.0f}));
    REQUIRE_FALSE(fixture.confiner->cameraWasDisplaced(fixture.camera->cameraId()));
}

TEST_CASE("Confiner3D clamps points outside the volume", "[unit][confiner3d]")
{
    ConfinerFixture fixture;
    fixture.addVolume();
    fixture.confiner->boundingVolume = fixture.volume;

    CameraState state = fixture.makeState({7.0f, 0.0f, 0.0f});
    fixture.body(state);

    REQUIRE(state.correctedPosition().x == Catch::Approx(5.0f).margin(1e-4f));
    REQUIRE(state.correctedPosition().y == Catch::Approx(0.0f));
    REQUIRE(fixture.confiner->cameraWasDisplaced(fixture.camera->cameraId()));
    REQUIRE(fixture.confiner->cameraDisplacementDistance(fixture.camera->cameraId())
        == Catch::Approx(2.0f).margin(1e-4f));
}

TEST_CASE("Confiner3D handles missing volumes", "[unit][confiner3d]")
{
    ConfinerFixture fixture;
    CameraState state = fixture.makeState({7.0f, 0.0f, 0.0f});
    fixture.body(state);
    REQUIRE(math::approximately(state.correctedPosition(), {7.0f, 0.0f, 0.0f}));
    REQUIRE(fixture.physics.boundsCalls == 0);

    fixture.confiner->boundingVolume = 99;
    state = fixture.makeState({7.0f, 0.0f, 0.0f});
    fixture.body(state);
    REQUIRE(math::approximately(state.correctedPosition(), {7.0f, 0.0f, 0.0f}));
    REQUIRE(fixture.physics.boundsCalls == 1);
}

TEST_CASE("Confiner3D slows motion near the volume edge", "[unit][confiner3d]")
{
    ConfinerFixture fixture;
    fixture.addVolume();
    fixture.confiner->boundingVolume = fixture.volume;
    fixture.confiner->slowingDistance = 1.0f;
    fixture.camera->previousStateIsValid = true;

    CameraState first = fixture.makeState({4.5f, 0.0f, 0.0f});
    fixture.body(first);
    REQUIRE(first.correctedPosition().x == Catch::Approx(4.5f).margin(1e-4f));

    CameraState second = fixture.makeState({5.5f, 0.0f, 0.0f});
    fixture.body(second);
    REQUIRE(second.correctedPosition().x == Catch::Approx(4.5875f).margin(1e-4f));
}

TEST_CASE("Confiner3D snaps directly without a previous state", "[unit][confiner3d]")
{
    ConfinerFixture fixture;
    fixture.addVolume();
    fixture.confiner->boundingVolume = fixture.volume;
    fixture.confiner->slowingDistance = 1.0f;
    fixture.camera->previousStateIsValid = false;

    CameraState state = fixture.makeState({5.5f, 0.0f, 0.0f});
    fixture.body(state);
    REQUIRE(state.correctedPosition().x == Catch::Approx(5.0f).margin(1e-4f));
}

TEST_CASE("Confiner3D adjusts cached position for followed target warps", "[unit][confiner3d]")
{
    ConfinerFixture fixture;
    fixture.addVolume();
    fixture.confiner->boundingVolume = fixture.volume;
    fixture.confiner->slowingDistance = 1.0f;
    fixture.camera->previousStateIsValid = true;

    CameraState first = fixture.makeState({4.5f, 0.0f, 0.0f});
    fixture.body(first);
    fixture.confiner->onTargetObjectWarped(fixture.follow, {1.0f, 0.0f, 0.0f});

    CameraState second = fixture.makeState({6.0f, 0.0f, 0.0f});
    fixture.body(second);
    REQUIRE(second.correctedPosition().x == Catch::Approx(5.0f).margin(1e-4f));
}

