#include <cinerig/core/camera_state.hpp>

#include <cmath>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig;

static_assert(static_cast<std::uint32_t>(CameraState::BlendHints::NoPosition) == (1u << 16));
static_assert(static_cast<std::uint32_t>(CameraState::BlendHints::NoOrientation) == (1u << 17));
static_assert(static_cast<std::uint32_t>(CameraState::BlendHints::NoLens) == (1u << 18));

TEST_CASE("Camera state defaults match the Unity state contract", "[unit][core][state]")
{
    const CameraState& state = CameraState::Default;

    REQUIRE(LensSettings::areEqual(state.lens, LensSettings::Default));
    REQUIRE(state.referenceUp.x == 0.0f);
    REQUIRE(state.referenceUp.y == 1.0f);
    REQUIRE(state.referenceUp.z == 0.0f);
    REQUIRE_FALSE(state.hasLookAt());
    REQUIRE(std::isnan(state.referenceLookAt.x));
    REQUIRE(std::isnan(state.referenceLookAt.y));
    REQUIRE(std::isnan(state.referenceLookAt.z));
    REQUIRE(state.rawPosition.x == 0.0f);
    REQUIRE(state.rawPosition.y == 0.0f);
    REQUIRE(state.rawPosition.z == 0.0f);
    REQUIRE(math::angle(state.rawOrientation, math::Quat::identity()) == 0.0f);
    REQUIRE(math::angle(state.rotationDampingBypass, math::Quat::identity()) == 0.0f);
    REQUIRE(state.shotQuality == 1.0f);
    REQUIRE(state.positionCorrection.x == 0.0f);
    REQUIRE(state.positionCorrection.y == 0.0f);
    REQUIRE(state.positionCorrection.z == 0.0f);
    REQUIRE(math::angle(state.orientationCorrection, math::Quat::identity()) == 0.0f);
    REQUIRE(state.blendHint == BlendHint::Nothing);

    const CameraState made = CameraState::makeDefault();
    REQUIRE(LensSettings::areEqual(made.lens, state.lens));
    REQUIRE(made.referenceLookAt.x != made.referenceLookAt.x);
    REQUIRE(made.shotQuality == state.shotQuality);
}

TEST_CASE("Camera state look-at and correction helpers", "[unit][core][state]")
{
    CameraState state = CameraState::Default;
    state.referenceLookAt = {1.0f, 2.0f, 3.0f};
    REQUIRE(state.hasLookAt());

    state.rawPosition = {1.0f, 2.0f, 3.0f};
    state.positionCorrection = {0.5f, -1.0f, 2.0f};
    REQUIRE(state.correctedPosition().x == Catch::Approx(1.5f));
    REQUIRE(state.correctedPosition().y == Catch::Approx(1.0f));
    REQUIRE(state.correctedPosition().z == Catch::Approx(5.0f));
    REQUIRE(state.finalPosition().x == state.correctedPosition().x);
    REQUIRE(state.finalPosition().y == state.correctedPosition().y);
    REQUIRE(state.finalPosition().z == state.correctedPosition().z);

    state.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, math::HalfPi);
    state.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, math::HalfPi);
    const math::Quat expected = state.rawOrientation * state.orientationCorrection;
    REQUIRE(math::angle(state.correctedOrientation(), expected) == Catch::Approx(0.0f).margin(1e-5f));
    REQUIRE(math::angle(state.finalOrientation(), expected) == Catch::Approx(0.0f).margin(1e-5f));
}

TEST_CASE("Camera state applies Dutch after the orientation correction", "[unit][core][state]")
{
    CameraState state = CameraState::Default;
    state.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, math::HalfPi);
    state.orientationCorrection = math::quatFromAxisAngle(math::Vec3::right, math::HalfPi);
    state.lens.dutch = 90.0f;

    const math::Quat expected = state.rawOrientation
        * state.orientationCorrection
        * math::quatFromAxisAngle(math::Vec3::forward, math::HalfPi);
    REQUIRE(math::angle(state.finalOrientation(), expected) == Catch::Approx(0.0f).margin(1e-5f));

    CameraState simple = CameraState::Default;
    simple.lens.dutch = 90.0f;
    const math::Vec3 rotatedUp = math::rotate(simple.finalOrientation(), math::Vec3::up);
    REQUIRE(math::approximately(rotatedUp, math::Vec3::right));
}

TEST_CASE("Camera state blend hints compose as flags", "[unit][core][state]")
{
    using Hints = CameraState::BlendHints;

    REQUIRE(Hints::NoTransform == (Hints::NoPosition | Hints::NoOrientation));
    REQUIRE(hasBlendHint(Hints::NoTransform, Hints::NoPosition));
    REQUIRE(hasBlendHint(Hints::NoTransform, Hints::NoOrientation));
    REQUIRE_FALSE(hasBlendHint(Hints::NoTransform, Hints::NoLens));

    Hints hints = Hints::Nothing;
    hints |= Hints::SphericalPositionBlend;
    hints |= Hints::InheritPosition;
    REQUIRE(hasBlendHint(hints, Hints::SphericalPositionBlend));
    REQUIRE(hasBlendHint(hints, Hints::InheritPosition));
    REQUIRE_FALSE(hasBlendHint(hints, Hints::CylindricalPositionBlend));
}
