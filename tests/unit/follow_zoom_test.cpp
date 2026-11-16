#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/components/follow_zoom.hpp>
#include <cinerig/math/damp.hpp>

using namespace cinerig;

namespace
{

float fovForWidth(float width, float distance)
{
    return 2.0f * std::atan(width / (2.0f * distance)) * math::RadToDeg;
}

CameraState makeState(float distance, float lensFov = 40.0f)
{
    CameraState state = CameraState::makeDefault();
    state.rawPosition = {0.0f, 0.0f, distance};
    state.referenceLookAt = math::Vec3::zero;
    state.lens.fieldOfView = lensFov;
    return state;
}

} // namespace

TEST_CASE("FollowZoom exposes the original defaults and validates ranges", "[unit][follow_zoom]")
{
    FollowZoom zoom;
    REQUIRE(zoom.width == 2.0f);
    REQUIRE(zoom.damping == 1.0f);
    REQUIRE(zoom.fovRange.x == 3.0f);
    REQUIRE(zoom.fovRange.y == 60.0f);

    zoom.width = -4.0f;
    zoom.damping = -2.0f;
    zoom.fovRange = {200.0f, 150.0f};
    zoom.validate();
    REQUIRE(zoom.width == 0.0f);
    REQUIRE(zoom.damping == 0.0f);
    REQUIRE(zoom.fovRange.x == 150.0f);
    REQUIRE(zoom.fovRange.y == 150.0f);

    zoom.fovRange = {-5.0f, 500.0f};
    zoom.validate();
    REQUIRE(zoom.fovRange.x == 1.0f);
    REQUIRE(zoom.fovRange.y == 179.0f);

    zoom.damping = 2.5f;
    REQUIRE(zoom.maxDampTime() == 2.5f);
}

TEST_CASE("FollowZoom computes and clamps the FOV for the target distance", "[unit][follow_zoom]")
{
    VirtualCamera camera;
    FollowZoom zoom;
    zoom.damping = 0.0f;

    CameraState state = makeState(10.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.lens.fieldOfView, fovForWidth(2.0f, 10.0f), 1e-4f));

    zoom.width = 100.0f;
    state = makeState(10.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.lens.fieldOfView, 60.0f, 1e-4f));

    zoom.width = 0.0f;
    state = makeState(10.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.lens.fieldOfView, 3.0f, 1e-4f));

    zoom.width = 2.0f;
    state = makeState(1.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.lens.fieldOfView, 60.0f, 1e-4f));

    state = makeState(100.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(math::approximately(state.lens.fieldOfView, 3.0f, 1e-4f));
}

TEST_CASE("FollowZoom writes only at Body and handles a missing look-at point", "[unit][follow_zoom]")
{
    VirtualCamera camera;
    FollowZoom zoom;

    CameraState state = CameraState::makeDefault();
    state.lens.fieldOfView = 50.0f;
    state.rawPosition = {0.0f, 0.0f, 10.0f};

    zoom.postPipelineStageCallback(camera, Stage::Aim, state, 1.0f / 60.0f);
    REQUIRE(state.lens.fieldOfView == 50.0f);

    camera.previousStateIsValid = true;
    zoom.postPipelineStageCallback(camera, Stage::Body, state, 1.0f / 60.0f);
    REQUIRE(state.lens.fieldOfView == 60.0f);
    REQUIRE_FALSE(std::isnan(state.lens.fieldOfView));
}

TEST_CASE("FollowZoom damping uses the previous frame and target attachment", "[unit][follow_zoom]")
{
    VirtualCamera camera;
    FollowZoom zoom;
    zoom.width = 2.0f;

    CameraState first = makeState(10.0f, 40.0f);
    camera.previousStateIsValid = false;
    zoom.postPipelineStageCallback(camera, Stage::Body, first, 1.0f / 60.0f);
    const float previousFov = fovForWidth(2.0f, 10.0f);
    REQUIRE(math::approximately(first.lens.fieldOfView, previousFov, 1e-4f));

    camera.previousStateIsValid = true;
    camera.lookAtTargetAttachment = 0.0f;
    CameraState detached = makeState(20.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, detached, 1.0f / 60.0f);
    REQUIRE(math::approximately(detached.lens.fieldOfView, previousFov, 1e-4f));

    camera.lookAtTargetAttachment = 1.0f;
    CameraState damped = makeState(20.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, damped, 1.0f / 60.0f);

    const float currentWidth = 20.0f * 2.0f
        * std::tan(previousFov * math::DegToRad * 0.5f);
    const float dampedDelta = math::standardDamp(
        2.0f - currentWidth,
        zoom.damping,
        1.0f / 60.0f);
    const float expectedFov = fovForWidth(currentWidth + dampedDelta, 20.0f);
    REQUIRE(math::approximately(damped.lens.fieldOfView, expectedFov, 1e-4f));
    REQUIRE(damped.lens.fieldOfView < previousFov);
    REQUIRE(damped.lens.fieldOfView > fovForWidth(2.0f, 20.0f));
}

TEST_CASE("FollowZoom reset and invalid deltas refresh the zoom cache", "[unit][follow_zoom]")
{
    VirtualCamera camera;
    FollowZoom zoom;
    zoom.width = 2.0f;

    CameraState first = makeState(10.0f, 40.0f);
    camera.previousStateIsValid = false;
    zoom.postPipelineStageCallback(camera, Stage::Body, first, 1.0f / 60.0f);

    camera.previousStateIsValid = true;
    zoom.reset(camera);

    CameraState afterReset = makeState(20.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, afterReset, 1.0f / 60.0f);
    const float resetCurrentWidth = 20.0f * 2.0f
        * std::tan(40.0f * math::DegToRad * 0.5f);
    const float resetDelta = math::standardDamp(
        2.0f - resetCurrentWidth,
        zoom.damping,
        1.0f / 60.0f);
    REQUIRE(math::approximately(
        afterReset.lens.fieldOfView,
        fovForWidth(resetCurrentWidth + resetDelta, 20.0f),
        1e-4f));

    CameraState invalidStep = makeState(20.0f, 33.0f);
    zoom.postPipelineStageCallback(camera, Stage::Aim, invalidStep, -1.0f);

    CameraState afterInvalid = makeState(20.0f, 40.0f);
    zoom.postPipelineStageCallback(camera, Stage::Body, afterInvalid, 1.0f / 60.0f);
    const float invalidCurrentWidth = 20.0f * 2.0f
        * std::tan(33.0f * math::DegToRad * 0.5f);
    const float invalidDelta = math::standardDamp(
        2.0f - invalidCurrentWidth,
        zoom.damping,
        1.0f / 60.0f);
    REQUIRE(math::approximately(
        afterInvalid.lens.fieldOfView,
        fovForWidth(invalidCurrentWidth + invalidDelta, 20.0f),
        1e-4f));
}
