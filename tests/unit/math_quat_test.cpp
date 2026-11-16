#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Quaternion axis-angle and vector rotation", "[unit][math]")
{
    const Quat yaw = quatFromAxisAngle(Vec3::up, HalfPi);
    REQUIRE(approximately(rotate(yaw, Vec3::right), Vec3::forward));
    REQUIRE(approximately(rotate(yaw, Vec3::forward), Vec3::left));

    const Quat pitch = quatFromAxisAngle(Vec3::right, HalfPi);
    REQUIRE(approximately(rotate(pitch, Vec3::forward), Vec3::up));

    const Quat roll = quatFromAxisAngle(Vec3::back, HalfPi);
    REQUIRE(approximately(rotate(roll, Vec3::right), Vec3::up));
}

TEST_CASE("Look rotation follows the -Z forward convention", "[unit][math]")
{
    REQUIRE(approximately(lookRotation(Vec3::forward, Vec3::up), Quat::identity()));
    REQUIRE(approximately(rotate(lookRotation(Vec3::forward, Vec3::up), Vec3::forward), Vec3::forward));

    const Quat lookRight = lookRotation(Vec3::right, Vec3::up);
    REQUIRE(approximately(rotate(lookRight, Vec3::forward), Vec3::right));
    REQUIRE(approximately(rotate(lookRight, Vec3::up), Vec3::up));

    const Quat lookUp = lookRotation(Vec3::up, Vec3::up);
    REQUIRE(approximately(rotate(lookUp, Vec3::forward), Vec3::up));
}

TEST_CASE("From-to rotation handles normal and opposite directions", "[unit][math]")
{
    const Quat quarter = fromToRotation(Vec3::forward, Vec3::right);
    REQUIRE(approximately(rotate(quarter, Vec3::forward), Vec3::right));

    const Quat opposite = fromToRotation(Vec3::up, Vec3::down);
    REQUIRE(approximately(rotate(opposite, Vec3::up), Vec3::down));

    const Quat safe = safeFromToRotation(Vec3::forward, Vec3::right, Vec3::up);
    REQUIRE(approximately(rotate(safe, Vec3::forward), Vec3::right));

    const Quat safePitch = safeFromToRotation(Vec3{1.0f, 1.0f, 0.0f}, Vec3{1.0f, -1.0f, 0.0f}, Vec3::forward);
    REQUIRE(approximately(rotate(safePitch, normalize(Vec3{1.0f, 1.0f, 0.0f})), normalize(Vec3{1.0f, -1.0f, 0.0f})));
}

TEST_CASE("Quaternion slerp, inverse, and angle", "[unit][math]")
{
    const Quat identity = Quat::identity();
    const Quat quarter = quatFromAxisAngle(Vec3::up, HalfPi);
    const Quat halfway = slerp(identity, quarter, 0.5f);
    REQUIRE(angle(identity, halfway) == Catch::Approx(HalfPi * 0.5f).margin(1e-4f));
    REQUIRE(angle(halfway, quarter) == Catch::Approx(HalfPi * 0.5f).margin(1e-4f));

    const Quat extrapolated = slerpUnclamped(identity, quarter, 2.0f);
    REQUIRE(angle(identity, extrapolated) == Catch::Approx(Pi).margin(1e-4f));

    const Quat inverseQuarter = inverse(quarter);
    REQUIRE(angle(inverseQuarter * quarter, identity) < 1e-4f);
    REQUIRE(angle(quarter, quarter) == Catch::Approx(0.0f));
    REQUIRE(angle(quarter, -quarter) == Catch::Approx(0.0f));
}

TEST_CASE("Euler conversion is consistent with the Z-X-Y order", "[unit][math]")
{
    const Quat yaw = quatFromEuler(Vec3{0.0f, HalfPi, 0.0f});
    REQUIRE(approximately(rotate(yaw, Vec3::forward), Vec3::left));

    const Vec3 angles{0.2f, -0.3f, 0.4f};
    const Quat original = quatFromEuler(angles);
    const Vec3 extracted = eulerAngles(original);
    const Quat reconstructed = quatFromEuler(extracted);
    REQUIRE(angle(original, reconstructed) < 1e-4f);

    const Quat yawPitchRoll = quatFromYawPitchRoll(0.1f, 0.2f, 0.3f);
    REQUIRE_FALSE(yawPitchRoll.isNaN());

    const Quat gimbal = quatFromEuler(Vec3{HalfPi, 0.3f, 0.4f});
    const Quat gimbalReconstructed = quatFromEuler(eulerAngles(gimbal));
    REQUIRE(angle(gimbal, gimbalReconstructed) < 1e-3f);
}
