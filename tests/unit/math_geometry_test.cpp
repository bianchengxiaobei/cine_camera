#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Mat3 and Mat4 transform primitives", "[unit][math]")
{
    const Quat rotation = quatFromEuler(Vec3{0.2f, -0.3f, 0.4f});
    const Mat3 matrix = Mat3::fromQuat(rotation);
    const Quat reconstructed = quatFromBasis(matrix.column(0), matrix.column(1), matrix.column(2));
    REQUIRE(angle(rotation, reconstructed) < 1e-4f);

    const Mat3 identity = matrix * inverse(matrix);
    REQUIRE(approximately(identity.column(0), Vec3{1.0f, 0.0f, 0.0f}));
    REQUIRE(approximately(identity.column(1), Vec3{0.0f, 1.0f, 0.0f}));
    REQUIRE(approximately(identity.column(2), Vec3{0.0f, 0.0f, 1.0f}));

    const Mat4 transform = Mat4::translation(Vec3{1.0f, 2.0f, 3.0f}) * Mat4::rotation(rotation);
    const Vec3 point{0.5f, -1.0f, 2.0f};
    const Vec3 transformed = transform.transformPoint(point);
    const Vec3 restored = inverseAffine(transform).transformPoint(transformed);
    REQUIRE(approximately(restored, point, 0.001f));
    REQUIRE(approximately(transform.transformVector(Vec3::forward), rotate(rotation, Vec3::forward)));
}

TEST_CASE("Rect, bounds, ray, and plane helpers", "[unit][math]")
{
    const Rect rect{0.0f, 0.0f, 10.0f, 10.0f};
    REQUIRE(rect.contains(Vec2{5.0f, 5.0f}));
    REQUIRE_FALSE(rect.contains(Vec2{11.0f, 5.0f}));
    const Rect inflated = rect.inflated(Vec2{1.0f, 1.0f});
    REQUIRE(inflated.xMin() == Catch::Approx(-1.0f));
    REQUIRE(inflated.width == Catch::Approx(12.0f));

    Bounds bounds;
    bounds.encapsulate(Vec3{1.0f, 2.0f, 3.0f});
    bounds.encapsulate(Vec3{-1.0f, 0.0f, 1.0f});
    REQUIRE(bounds.contains(Vec3{0.0f, 1.0f, 2.0f}));
    REQUIRE(bounds.center.x == Catch::Approx(0.0f));
    REQUIRE(bounds.size().x == Catch::Approx(2.0f));

    const BoundingSphere sphere{Vec3{}, 2.0f};
    REQUIRE(sphere.contains(Vec3{1.0f, 0.0f, 0.0f}));
    REQUIRE_FALSE(sphere.contains(Vec3{3.0f, 0.0f, 0.0f}));

    const Ray ray{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 0.0f, -1.0f}};
    REQUIRE(approximately(ray.pointAt(2.0f), Vec3{0.0f, 0.0f, -2.0f}));

    const Plane plane{Vec3::up, -1.0f};
    REQUIRE(plane.signedDistance(Vec3{0.0f, 3.0f, 0.0f}) == Catch::Approx(2.0f));
    REQUIRE(approximately(plane.project(Vec3{4.0f, 3.0f, 0.0f}), Vec3{4.0f, 1.0f, 0.0f}));
    float distance = 0.0f;
    REQUIRE(plane.raycast(Ray{Vec3{0.0f, 3.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f}}, distance));
    REQUIRE(distance == Catch::Approx(2.0f));
}

TEST_CASE("Camera rotation helpers round-trip", "[unit][math]")
{
    const Quat orientation = lookRotation(Vec3{1.0f, 0.0f, 0.0f}, Vec3::up);
    const Vec3 lookDirection{1.0f, 0.0f, 0.0f};
    const Vec2 rotation = getCameraRotationToTarget(orientation, lookDirection, Vec3::up);
    const Quat reconstructed = applyCameraRotation(orientation, rotation, Vec3::up);
    REQUIRE(angle(orientation, reconstructed) < 1e-3f);

    const Quat offsetOrientation = quatFromAxisAngle(Vec3::up, 0.4f) * orientation;
    const Vec2 offsetRotation = getCameraRotationToTarget(offsetOrientation, lookDirection, Vec3::up);
    const Quat offsetCorrected = applyCameraRotation(offsetOrientation, offsetRotation, Vec3::up);
    REQUIRE(angle(rotate(offsetCorrected, Vec3::forward), lookDirection) < 1e-3f);
}
