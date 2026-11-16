#include "fake_entity_world.hpp"

#include <cmath>
#include <memory>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cinerig/components/confiner2d.hpp>
#include <cinerig/math/damp.hpp>
#include <cinerig/runtime/camera_system.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

std::vector<std::vector<math::Vec2>> square(float halfExtent, bool clockwise = false)
{
    if (clockwise)
    {
        return {{
            {-halfExtent, -halfExtent},
            {-halfExtent, halfExtent},
            {halfExtent, halfExtent},
            {halfExtent, -halfExtent},
        }};
    }
    return {{
        {-halfExtent, -halfExtent},
        {halfExtent, -halfExtent},
        {halfExtent, halfExtent},
        {-halfExtent, halfExtent},
    }};
}

struct Confiner2DFixture
{
    Confiner2DFixture()
    {
        world.add(
            follow,
            Transform{{0.0f, 0.0f, 0.0f}, math::Quat::identity(), math::Vec3::one});
        const CameraId id = system.createCamera<VirtualCamera>("confiner2d");
        camera = system.camera(id);
        camera->targets.tracking = TargetRef::entity(follow);

        auto owned = std::make_unique<Confiner2D>();
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
        state.lens.orthographic = true;
        state.lens.orthographicSize = 0.0f;
        state.lens.aspect = 1.0f;
        return state;
    }

    void body(CameraState& state, float deltaTime = 1.0f / 60.0f)
    {
        confiner->postPipelineStageCallback(*camera, Stage::Body, state, deltaTime);
    }

    void setShape(std::vector<std::vector<math::Vec2>> shape)
    {
        confiner->boundingShape = std::move(shape);
        confiner->invalidateBoundingShapeCache();
    }

    FakeEntityWorld world;
    CameraSystem system{world};
    VirtualCamera* camera = nullptr;
    Confiner2D* confiner = nullptr;
    EntityId follow = 1;
};

} // namespace

TEST_CASE("Confiner2D exposes original defaults and validation", "[unit][confiner2d]")
{
    Confiner2D confiner;
    REQUIRE(confiner.boundingShape.empty());
    REQUIRE(confiner.damping == Catch::Approx(0.5f));
    REQUIRE(confiner.slowingDistance == Catch::Approx(5.0f));
    REQUIRE_FALSE(confiner.oversizeWindow.enabled);
    REQUIRE_FALSE(confiner.isValid());
    REQUIRE_FALSE(confiner.boundingShapeIsBaked());
    REQUIRE(confiner.maxDampTime() == Catch::Approx(1.0f));

    confiner.damping = 2.0f;
    confiner.slowingDistance = 20.0f;
    REQUIRE(confiner.maxDampTime() == Catch::Approx(4.0f));
    confiner.damping = -1.0f;
    confiner.slowingDistance = -2.0f;
    confiner.oversizeWindow.maxWindowSize = -4.0f;
    confiner.oversizeWindow.padding = 200.0f;
    confiner.validate();
    REQUIRE(confiner.damping == 0.0f);
    REQUIRE(confiner.slowingDistance == 0.0f);
    REQUIRE(confiner.oversizeWindow.maxWindowSize == 0.0f);
    REQUIRE(confiner.oversizeWindow.padding == Catch::Approx(100.0f));

    confiner.boundingShape = square(2.0f);
    confiner.damping = 1.0f;
    confiner.slowingDistance = 1.0f;
    confiner.reset();
    REQUIRE(confiner.boundingShape.empty());
    REQUIRE(confiner.damping == Catch::Approx(0.5f));
    REQUIRE(confiner.slowingDistance == Catch::Approx(5.0f));
}

TEST_CASE("Confiner2D calculates orthographic and perspective frustums", "[unit][confiner2d]")
{
    LensSettings lens;
    lens.orthographic = true;
    lens.orthographicSize = 2.5f;
    REQUIRE(Confiner2D::calculateHalfFrustumHeight(lens, 10.0f)
        == Catch::Approx(2.5f));

    lens.orthographic = false;
    lens.fieldOfView = 60.0f;
    REQUIRE(Confiner2D::calculateHalfFrustumHeight(lens, 10.0f)
        == Catch::Approx(10.0f * std::tan(30.0f * math::DegToRad)).margin(1e-4f));
    REQUIRE(Confiner2D::calculateHalfFrustumHeight(lens, -10.0f)
        == Catch::Approx(10.0f * std::tan(30.0f * math::DegToRad)).margin(1e-4f));
}

TEST_CASE("Confiner2D confines independently of winding", "[unit][confiner2d]")
{
    for (const bool clockwise : {false, true})
    {
        Confiner2DFixture fixture;
        fixture.confiner->damping = 0.0f;
        fixture.confiner->slowingDistance = 0.0f;
        fixture.setShape(square(1.0f, clockwise));

        CameraState inside = fixture.makeState({0.5f, -0.25f, 0.0f});
        fixture.body(inside);
        REQUIRE(math::approximately(inside.correctedPosition(), {0.5f, -0.25f, 0.0f}));

        CameraState outside = fixture.makeState({2.0f, 0.0f, 0.0f});
        fixture.body(outside);
        REQUIRE(outside.correctedPosition().x == Catch::Approx(1.0f).margin(1e-4f));
        REQUIRE(outside.correctedPosition().y == Catch::Approx(0.0f).margin(1e-4f));
        REQUIRE(fixture.confiner->cameraWasDisplaced(fixture.camera->cameraId()));
        REQUIRE(fixture.confiner->cameraDisplacementDistance(
                    fixture.camera->cameraId())
            == Catch::Approx(1.0f).margin(1e-4f));
    }
}

TEST_CASE("Confiner2D shrinks confinement by the camera frustum", "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 0.0f;
    fixture.setShape(square(10.0f));

    CameraState state = fixture.makeState({20.0f, 0.0f, 10.0f});
    state.lens.orthographic = false;
    state.lens.fieldOfView = 60.0f;
    fixture.body(state);

    const float halfFrustum = 10.0f * std::tan(30.0f * math::DegToRad);
    REQUIRE(state.correctedPosition().x
        == Catch::Approx(10.0f - halfFrustum).margin(1e-4f));
    REQUIRE(state.correctedPosition().y == Catch::Approx(0.0f).margin(1e-4f));
    REQUIRE(state.correctedPosition().z == Catch::Approx(10.0f).margin(1e-4f));
}

TEST_CASE("Confiner2D slows motion near the edge", "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 1.0f;
    fixture.setShape(square(5.0f));
    fixture.camera->previousStateIsValid = true;

    CameraState first = fixture.makeState({4.5f, 0.0f, 0.0f});
    fixture.body(first);
    REQUIRE(first.correctedPosition().x == Catch::Approx(4.5f).margin(1e-4f));

    CameraState second = fixture.makeState({5.5f, 0.0f, 0.0f});
    fixture.body(second);
    REQUIRE(second.correctedPosition().x == Catch::Approx(4.5875f).margin(1e-4f));
}

TEST_CASE("Confiner2D snaps directly without a previous state", "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 1.0f;
    fixture.setShape(square(5.0f));
    fixture.camera->previousStateIsValid = false;

    CameraState state = fixture.makeState({5.5f, 0.0f, 0.0f});
    fixture.body(state);
    REQUIRE(state.correctedPosition().x == Catch::Approx(5.0f).margin(1e-4f));
}

TEST_CASE("Confiner2D damps corner displacement", "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 1.0f;
    fixture.confiner->slowingDistance = 0.0f;
    fixture.setShape(square(1.0f));
    fixture.camera->previousStateIsValid = true;

    CameraState first = fixture.makeState({2.0f, 2.0f, 0.0f});
    fixture.body(first);
    REQUIRE(math::approximately(first.correctedPosition(), {1.0f, 1.0f, 0.0f}));

    CameraState second = fixture.makeState({0.0f, 2.0f, 0.0f});
    fixture.body(second);
    REQUIRE(second.correctedPosition().x < -0.5f);
    REQUIRE(second.correctedPosition().y == Catch::Approx(1.0f).margin(1e-4f));

    const float expectedDamp = std::exp(math::kLogNegligibleResidual / 60.0f);
    REQUIRE(second.correctedPosition().x
        == Catch::Approx(-expectedDamp).margin(1e-4f));
}

TEST_CASE("Confiner2D applies shape transform deltas without rebaking",
    "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 0.0f;
    fixture.setShape(square(1.0f));

    CameraState first = fixture.makeState({2.0f, 0.0f, 0.0f});
    fixture.body(first);
    REQUIRE(first.correctedPosition().x == Catch::Approx(1.0f).margin(1e-4f));

    fixture.confiner->boundingShapeToWorld = math::Mat4::translation({3.0f, 0.0f, 0.0f});
    CameraState second = fixture.makeState({5.0f, 0.0f, 0.0f});
    fixture.body(second);
    REQUIRE(second.correctedPosition().x == Catch::Approx(4.0f).margin(1e-4f));
}

TEST_CASE("Confiner2D invalidates lens and shape caches", "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 0.0f;
    fixture.setShape(square(5.0f));
    REQUIRE(fixture.confiner->bakeBoundingShape(*fixture.camera, 1.0f));
    REQUIRE(fixture.confiner->boundingShapeIsBaked());

    CameraState enlarged = fixture.makeState({10.0f, 0.0f, 0.0f});
    enlarged.lens.orthographicSize = 2.0f;
    fixture.body(enlarged);
    REQUIRE(enlarged.correctedPosition().x == Catch::Approx(3.0f).margin(1e-4f));

    fixture.confiner->invalidateLensCache();
    CameraState regular = fixture.makeState({10.0f, 0.0f, 0.0f});
    fixture.body(regular);
    REQUIRE(regular.correctedPosition().x == Catch::Approx(5.0f).margin(1e-4f));

    fixture.confiner->invalidateBoundingShapeCache();
    REQUIRE_FALSE(fixture.confiner->boundingShapeIsBaked());
    REQUIRE(fixture.confiner->bakeBoundingShape(*fixture.camera, 1.0f));
    REQUIRE(fixture.confiner->boundingShapeIsBaked());
}

TEST_CASE("Confiner2D ignores missing shapes", "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 0.0f;

    CameraState state = fixture.makeState({4.0f, 2.0f, 0.0f});
    fixture.body(state);
    REQUIRE(math::approximately(state.correctedPosition(), {4.0f, 2.0f, 0.0f}));
    REQUIRE_FALSE(fixture.confiner->boundingShapeIsBaked());
    REQUIRE_FALSE(fixture.confiner->cameraWasDisplaced(fixture.camera->cameraId()));

    fixture.setShape({});
    fixture.body(state);
    REQUIRE(math::approximately(state.correctedPosition(), {4.0f, 2.0f, 0.0f}));
}

TEST_CASE("Confiner2D adjusts cached position for followed target warps",
    "[unit][confiner2d]")
{
    Confiner2DFixture fixture;
    fixture.confiner->damping = 0.0f;
    fixture.confiner->slowingDistance = 1.0f;
    fixture.setShape(square(5.0f));
    fixture.camera->previousStateIsValid = true;

    CameraState first = fixture.makeState({4.5f, 0.0f, 0.0f});
    fixture.body(first);
    fixture.confiner->onTargetObjectWarped(fixture.follow, {1.0f, 0.0f, 0.0f});

    CameraState second = fixture.makeState({6.0f, 0.0f, 0.0f});
    fixture.body(second);
    REQUIRE(second.correctedPosition().x == Catch::Approx(5.0f).margin(1e-4f));
}



