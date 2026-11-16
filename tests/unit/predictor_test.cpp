#include <cinerig/math/math.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Position predictor starts empty and learns uniform motion", "[unit][math]")
{
    PositionPredictor predictor;
    predictor.smoothing = 0.3f;
    REQUIRE(predictor.isEmpty());
    REQUIRE(approximately(predictor.predictDelta(1.0f), Vec3{}));

    const float deltaTime = 1.0f / 60.0f;
    Vec3 position{};
    for (int frame = 0; frame < 600; ++frame)
    {
        predictor.addPosition(position, deltaTime);
        position += Vec3{1.0f, 0.0f, 0.0f} * deltaTime;
    }

    REQUIRE_FALSE(predictor.isEmpty());
    REQUIRE(predictor.predictDelta(0.5f).x == Catch::Approx(0.5f).margin(0.02f));
    REQUIRE(std::abs(predictor.predictDelta(0.5f).y) < 0.01f);
}

TEST_CASE("Position predictor handles static, reverse, warp, and rotation", "[unit][math]")
{
    PositionPredictor predictor;
    predictor.smoothing = 0.2f;

    predictor.addPosition(Vec3{1.0f, 2.0f, 3.0f}, 0.0f);
    predictor.addPosition(Vec3{1.0f, 2.0f, 3.0f}, 1.0f);
    REQUIRE(predictor.predictDelta(1.0f).length() < 0.01f);

    const float deltaTime = 1.0f / 60.0f;
    Vec3 position{};
    for (int frame = 0; frame < 300; ++frame)
    {
        predictor.addPosition(position, deltaTime);
        position += Vec3{1.0f, 0.0f, 0.0f} * deltaTime;
    }
    const Vec3 velocityBeforeWarp = predictor.predictDelta(1.0f);
    const Vec3 positionBeforeWarp = predictor.currentPosition();
    predictor.applyTransformDelta(Vec3{10.0f, 0.0f, 0.0f});
    REQUIRE(approximately(predictor.currentPosition(), positionBeforeWarp + Vec3{10.0f, 0.0f, 0.0f}, 0.01f));
    REQUIRE(approximately(predictor.predictDelta(1.0f), velocityBeforeWarp, 0.01f));

    predictor.applyRotationDelta(quatFromAxisAngle(Vec3::up, HalfPi));
    REQUIRE(predictor.predictDelta(1.0f).z < -0.5f);

    Vec3 reversePosition{};
    for (int frame = 0; frame < 300; ++frame)
    {
        predictor.addPosition(reversePosition, deltaTime);
        reversePosition -= Vec3{1.0f, 0.0f, 0.0f} * deltaTime;
    }
    REQUIRE(predictor.predictDelta(1.0f).x < -0.5f);
}

TEST_CASE("Position predictor reset clears learned velocity", "[unit][math]")
{
    PositionPredictor predictor;
    predictor.smoothing = 0.1f;
    predictor.addPosition(Vec3{}, 0.0f);
    predictor.addPosition(Vec3{1.0f, 0.0f, 0.0f}, 1.0f);
    REQUIRE_FALSE(predictor.isEmpty());
    predictor.reset();
    REQUIRE(predictor.isEmpty());
    REQUIRE(approximately(predictor.predictDelta(1.0f), Vec3{}));
}
