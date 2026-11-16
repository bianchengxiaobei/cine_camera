#include <cinerig/core/camera_state.hpp>
#include <cinerig/core/camera_state_blend.hpp>
#include <cinerig/math/geometry.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

namespace
{

struct LookAtHintCase
{
    BlendHint hintsA;
    BlendHint hintsB;
    bool expectsLookAt;
};

const LookAtHintCase kLookAtHintCases[] = {
    {BlendHint::Nothing, BlendHint::Nothing, true},
    {BlendHint::IgnoreLookAtTarget, BlendHint::Nothing, false},
    {BlendHint::Nothing, BlendHint::IgnoreLookAtTarget, false},
};

} // namespace

TEST_CASE("CameraState lerp ignores the reference look-at when either hint requests it", "[parity][camera-state]")
{
    for (const LookAtHintCase& testCase : kLookAtHintCases)
    {
        CameraState a = CameraState::Default;
        a.referenceLookAt = math::Vec3::forward;
        a.blendHint = testCase.hintsA;

        CameraState b = CameraState::Default;
        b.referenceLookAt = math::Vec3::right;
        b.blendHint = testCase.hintsB;

        const CameraState result = CameraState::lerp(a, b, 0.5f);

        REQUIRE(result.hasLookAt() == testCase.expectsLookAt);
        if (result.hasLookAt())
        {
            REQUIRE(math::approximately(
                result.referenceLookAt,
                math::lerp(a.referenceLookAt, b.referenceLookAt, 0.5f),
                1e-5f));
        }
    }
}

TEST_CASE("CameraState lerp slerps raw orientation when the look-at target is ignored", "[parity][camera-state]")
{
    for (const LookAtHintCase& testCase : kLookAtHintCases)
    {
        CameraState a = CameraState::Default;
        a.rawOrientation = math::Quat::identity();
        a.referenceLookAt = {0.0f, 1.0f, 0.0f};
        a.blendHint = testCase.hintsA;

        CameraState b = CameraState::Default;
        b.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, math::HalfPi * 0.5f);
        b.referenceLookAt = {1.0f, 0.0f, 0.0f};
        b.blendHint = testCase.hintsB;

        const CameraState result = CameraState::lerp(a, b, 0.5f);
        const math::Quat slerped = math::slerp(a.rawOrientation, b.rawOrientation, 0.5f);
        const bool expectsSlerp = !testCase.expectsLookAt;

        if (expectsSlerp)
        {
            REQUIRE(math::angle(result.rawOrientation, slerped) == Catch::Approx(0.0f).margin(1e-5f));
        }
        else
        {
            REQUIRE(result.hasLookAt());
            REQUIRE(math::angle(result.rawOrientation, slerped) > 1e-3f);
        }
    }
}
