#include <cinerig/core/camera_state_blend.hpp>

#include <cmath>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

namespace
{

CameraState makeState(math::Vec3 position, math::Quat orientation, math::Vec3 lookAt)
{
    CameraState state = CameraState::Default;
    state.rawPosition = position;
    state.rawOrientation = orientation;
    state.referenceLookAt = lookAt;
    return state;
}

float forwardAlignment(math::Quat orientation, math::Vec3 direction)
{
    return math::dot(
        math::rotate(orientation, math::Vec3::forward),
        math::normalize(direction));
}

}

TEST_CASE("Camera state lerp returns the endpoint states", "[unit][core][blend][state]")
{
    CameraState a = makeState({1.0f, 2.0f, 3.0f}, math::quatFromAxisAngle(math::Vec3::up, 0.3f),
        CameraState::kNoPoint);
    CameraState b = makeState({4.0f, 5.0f, 6.0f}, math::quatFromAxisAngle(math::Vec3::up, 1.1f),
        CameraState::kNoPoint);
    a.referenceUp = {0.0f, 1.0f, 0.0f};
    b.referenceUp = {0.0f, 0.0f, 1.0f};
    a.shotQuality = 0.2f;
    b.shotQuality = 0.8f;
    a.positionCorrection = {1.0f, 0.0f, 0.0f};
    b.positionCorrection = {2.0f, 0.0f, 0.0f};
    a.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, 0.1f);
    b.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, 0.2f);
    a.lens.fieldOfView = 40.0f;
    b.lens.fieldOfView = 80.0f;

    const CameraState atZero = lerp(a, b, 0.0f);
    REQUIRE(atZero.rawPosition.x == a.rawPosition.x);
    REQUIRE(atZero.rawPosition.y == a.rawPosition.y);
    REQUIRE(atZero.rawPosition.z == a.rawPosition.z);
    REQUIRE(math::angle(atZero.rawOrientation, a.rawOrientation) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(math::angle(atZero.orientationCorrection, a.orientationCorrection) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(atZero.shotQuality == a.shotQuality);
    REQUIRE(atZero.lens.fieldOfView == a.lens.fieldOfView);
    REQUIRE_FALSE(atZero.hasLookAt());

    const CameraState atOne = lerp(a, b, 1.0f);
    REQUIRE(atOne.rawPosition.x == b.rawPosition.x);
    REQUIRE(atOne.rawPosition.y == b.rawPosition.y);
    REQUIRE(atOne.rawPosition.z == b.rawPosition.z);
    REQUIRE(math::angle(atOne.rawOrientation, b.rawOrientation) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(atOne.shotQuality == b.shotQuality);
    REQUIRE(atOne.lens.fieldOfView == b.lens.fieldOfView);
    REQUIRE_FALSE(atOne.hasLookAt());

    const CameraState staticHalfway = CameraState::lerp(a, b, 0.5f);
    REQUIRE(staticHalfway.rawPosition.x == Catch::Approx(2.5f));
    REQUIRE(staticHalfway.lens.fieldOfView == Catch::Approx(60.0f));
}

TEST_CASE("Camera state lerp applies the NoPosition, NoOrientation, and NoLens hints", "[unit][core][blend][state]")
{
    CameraState a = makeState({1.0f, 0.0f, 0.0f}, math::quatFromAxisAngle(math::Vec3::up, 0.2f),
        CameraState::kNoPoint);
    CameraState b = makeState({9.0f, 0.0f, 0.0f}, math::quatFromAxisAngle(math::Vec3::up, 1.2f),
        CameraState::kNoPoint);
    a.positionCorrection = {1.0f, 0.0f, 0.0f};
    b.positionCorrection = {2.0f, 0.0f, 0.0f};
    a.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, 0.1f);
    b.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, 0.3f);
    a.lens.fieldOfView = 40.0f;
    b.lens.fieldOfView = 80.0f;

    a.blendHint = BlendHint::NoPosition;
    b.blendHint = BlendHint::NoPosition;
    CameraState result = lerp(a, b, 0.5f);
    REQUIRE(hasBlendHint(result.blendHint, BlendHint::NoPosition));
    REQUIRE(result.rawPosition.x == 0.0f);
    REQUIRE(result.positionCorrection.x == 0.0f);

    a.blendHint = BlendHint::NoPosition;
    b.blendHint = BlendHint::Nothing;
    result = lerp(a, b, 0.5f);
    REQUIRE(result.rawPosition.x == b.rawPosition.x);
    REQUIRE(result.positionCorrection.x == b.positionCorrection.x);

    a.blendHint = BlendHint::Nothing;
    b.blendHint = BlendHint::NoPosition;
    result = lerp(a, b, 0.5f);
    REQUIRE(result.rawPosition.x == a.rawPosition.x);
    REQUIRE(result.positionCorrection.x == a.positionCorrection.x);

    a.blendHint = BlendHint::NoOrientation;
    b.blendHint = BlendHint::NoOrientation;
    result = lerp(a, b, 0.5f);
    REQUIRE(hasBlendHint(result.blendHint, BlendHint::NoOrientation));
    REQUIRE(math::angle(result.rawOrientation, math::Quat::identity()) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(math::angle(result.orientationCorrection, math::Quat::identity()) == Catch::Approx(0.0f).margin(1e-5f));

    a.blendHint = BlendHint::NoLens;
    b.blendHint = BlendHint::NoLens;
    result = lerp(a, b, 0.5f);
    REQUIRE(hasBlendHint(result.blendHint, BlendHint::NoLens));
    REQUIRE(result.lens.fieldOfView == LensSettings::Default.fieldOfView);

    a.blendHint = BlendHint::NoLens;
    b.blendHint = BlendHint::Nothing;
    result = lerp(a, b, 0.5f);
    REQUIRE(result.lens.fieldOfView == b.lens.fieldOfView);

    a.blendHint = BlendHint::Nothing;
    b.blendHint = BlendHint::NoLens;
    result = lerp(a, b, 0.5f);
    REQUIRE(result.lens.fieldOfView == a.lens.fieldOfView);
}

TEST_CASE("Camera state lerp uses spherical and cylindrical position paths", "[unit][core][blend][state]")
{
    CameraState a = makeState({1.0f, 0.0f, 0.0f}, math::Quat::identity(), {0.0f, 0.0f, 0.0f});
    CameraState b = makeState({0.0f, 1.0f, 0.0f}, math::Quat::identity(), {0.0f, 0.0f, 0.0f});
    a.blendHint = BlendHint::SphericalPositionBlend;

    CameraState result = lerp(a, b, 0.5f);
    REQUIRE(hasBlendHint(result.blendHint, BlendHint::SphericalPositionBlend));
    REQUIRE(result.rawPosition.length() == Catch::Approx(1.0f));
    REQUIRE(result.rawPosition.x == Catch::Approx(0.70710677f));
    REQUIRE(result.rawPosition.y == Catch::Approx(0.70710677f));

    a.rawPosition = {1.0f, 2.0f, 0.0f};
    b.rawPosition = {0.0f, 2.0f, 1.0f};
    a.blendHint = BlendHint::CylindricalPositionBlend;
    result = lerp(a, b, 0.5f);
    REQUIRE(hasBlendHint(result.blendHint, BlendHint::CylindricalPositionBlend));
    REQUIRE(result.rawPosition.y == Catch::Approx(2.0f));
    const float horizontalLength = std::sqrt(
        result.rawPosition.x * result.rawPosition.x + result.rawPosition.z * result.rawPosition.z);
    REQUIRE(horizontalLength == Catch::Approx(1.0f));

    a.blendHint = BlendHint::Nothing;
    b.blendHint = BlendHint::Nothing;
    result = lerp(a, b, 0.5f);
    REQUIRE(result.rawPosition.x == Catch::Approx(0.5f));
    REQUIRE(result.rawPosition.y == Catch::Approx(2.0f));
    REQUIRE(result.rawPosition.z == Catch::Approx(0.5f));
}

TEST_CASE("Camera state lerp keeps the look-at target composed through FOV changes", "[unit][core][blend][state]")
{
    CameraState a = makeState({0.0f, 0.0f, 10.0f}, math::Quat::identity(), {0.0f, 0.0f, 0.0f});
    CameraState b = makeState({0.0f, 0.0f, 20.0f}, math::Quat::identity(), {0.0f, 0.0f, 0.0f});
    a.lens.fieldOfView = 40.0f;
    b.lens.fieldOfView = 80.0f;

    const CameraState result = lerp(a, b, 0.5f);
    REQUIRE(result.hasLookAt());
    REQUIRE(result.lens.fieldOfView > 40.0f);
    REQUIRE(result.lens.fieldOfView < 80.0f);
    REQUIRE(result.lens.fieldOfView != Catch::Approx(60.0f));
    REQUIRE(result.referenceLookAt.x == Catch::Approx(0.0f));
    REQUIRE(result.referenceLookAt.z == Catch::Approx(0.0f));
}

TEST_CASE("Camera state lerp supports screen-space aim blending", "[unit][core][blend][state]")
{
    CameraState a = makeState({0.0f, 0.0f, 10.0f}, math::Quat::identity(), {10.0f, 0.0f, 10.0f});
    CameraState b = makeState({0.0f, 0.0f, 10.0f}, math::Quat::identity(), {0.0f, 0.0f, 0.0f});
    a.blendHint = BlendHint::ScreenSpaceAimWhenTargetsDiffer;

    const CameraState result = lerp(a, b, 0.5f);
    REQUIRE(result.hasLookAt());
    REQUIRE(result.referenceLookAt.x > 5.0f);
    REQUIRE(result.referenceLookAt.z > 0.0f);

    a.blendHint = BlendHint::IgnoreLookAtTarget;
    const CameraState ignored = lerp(a, b, 0.5f);
    REQUIRE_FALSE(ignored.hasLookAt());
}

TEST_CASE("Camera state lerp aims the blended orientation at the blended target", "[unit][core][blend][state]")
{
    const math::Vec3 targetA{10.0f, 0.0f, 0.0f};
    const math::Vec3 targetB{0.0f, 0.0f, -10.0f};
    CameraState a = makeState({0.0f, 0.0f, 0.0f}, math::lookRotation(targetA, math::Vec3::up), targetA);
    CameraState b = makeState({0.0f, 0.0f, 0.0f}, math::lookRotation(targetB, math::Vec3::up), targetB);

    const CameraState result = lerp(a, b, 0.5f);
    REQUIRE(result.hasLookAt());
    REQUIRE(forwardAlignment(result.rawOrientation, result.referenceLookAt) == Catch::Approx(1.0f).margin(1e-4f));

    a.blendHint = BlendHint::IgnoreLookAtTarget;
    const CameraState ignored = lerp(a, b, 0.5f);
    REQUIRE_FALSE(ignored.hasLookAt());
    REQUIRE(math::angle(ignored.rawOrientation, math::slerp(a.rawOrientation, b.rawOrientation, 0.5f))
        == Catch::Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Camera state lerp combines custom blendables by identity", "[unit][core][blend][state]")
{
    const auto shared = std::make_shared<int>(7);
    CameraState a;
    CameraState b;
    a.addCustomBlendable(CustomBlendable(shared, 0.8f));
    b.addCustomBlendable(CustomBlendable(shared, 0.4f));

    const CameraState result = lerp(a, b, 0.5f);
    REQUIRE(result.numCustomBlendables() == 1);
    REQUIRE(result.customBlendable(0).weight == Catch::Approx(0.6f));
    REQUIRE(result.customBlendable(0).data.get() == shared.get());

    const auto other = std::make_shared<int>(9);
    b.addCustomBlendable(CustomBlendable(other, 0.4f));
    const CameraState multiple = lerp(a, b, 0.5f);
    REQUIRE(multiple.numCustomBlendables() == 2);
    REQUIRE(multiple.customBlendable(1).weight == Catch::Approx(0.2f));
}

TEST_CASE("Camera state lerp fills four custom blendable slots and overflows", "[unit][core][blend][state]")
{
    CameraState state;
    std::shared_ptr<int> values[5];
    for (int i = 0; i < 5; ++i)
    {
        values[i] = std::make_shared<int>(i);
        state.addCustomBlendable(CustomBlendable(values[i], 1.0f));
    }

    REQUIRE(state.numCustomBlendables() == 5);
    REQUIRE(state.customBlendable(0).weight == 1.0f);
    REQUIRE(state.customBlendable(4).weight == 1.0f);
    REQUIRE(state.customBlendable(5).weight == 0.0f);
}

TEST_CASE("Camera state reports offscreen targets for orthographic and perspective lenses", "[unit][core][blend][state]")
{
    CameraState state = CameraState::Default;
    state.lens.orthographic = true;
    state.lens.orthographicSize = 10.0f;
    state.lens.aspect = 2.0f;
    state.referenceLookAt = {0.0f, 0.0f, -1.0f};
    REQUIRE_FALSE(state.isTargetOffscreen());

    state.referenceLookAt = {0.0f, 11.0f, -1.0f};
    REQUIRE(state.isTargetOffscreen());

    state.referenceLookAt = {21.0f, 0.0f, -1.0f};
    REQUIRE(state.isTargetOffscreen());

    state.lens.orthographic = false;
    state.lens.fieldOfView = 40.0f;
    state.lens.aspect = 1.0f;
    state.referenceLookAt = {0.0f, 0.0f, -10.0f};
    REQUIRE_FALSE(state.isTargetOffscreen());

    state.referenceLookAt = {0.0f, 10.0f, -10.0f};
    REQUIRE(state.isTargetOffscreen());

    state.referenceLookAt = {10.0f, 0.0f, -10.0f};
    REQUIRE(state.isTargetOffscreen());

    state.referenceLookAt = CameraState::kNoPoint;
    REQUIRE_FALSE(state.isTargetOffscreen());
}
