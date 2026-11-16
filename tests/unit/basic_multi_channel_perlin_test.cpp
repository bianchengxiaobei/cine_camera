#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <utility>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/components/basic_multi_channel_perlin.hpp>

using namespace cinerig;

namespace
{

NoiseProfile::Params constantChannel(float value)
{
    return {0.0f, value * 2.0f, true};
}

NoiseProfile::TransformParams transformChannel(
    float x,
    float y,
    float z)
{
    return {
        constantChannel(x),
        constantChannel(y),
        constantChannel(z),
    };
}

NoiseProfile makeProfile(math::Vec3 position, math::Vec3 orientation)
{
    NoiseProfile profile;
    if (!math::almostZero(position))
        profile.positionNoise.push_back(transformChannel(position.x, position.y, position.z));
    if (!math::almostZero(orientation))
        profile.orientationNoise.push_back(
            transformChannel(orientation.x, orientation.y, orientation.z));
    return profile;
}

} // namespace

TEST_CASE("BasicMultiChannelPerlin exposes the original defaults", "[unit][noise][perlin]")
{
    VirtualCamera camera;
    BasicMultiChannelPerlin& noise =
        camera.setNoise<BasicMultiChannelPerlin>();

    REQUIRE(noise.stage() == Stage::Noise);
    REQUIRE_FALSE(noise.isValid());
    REQUIRE(noise.amplitudeGain == 1.0f);
    REQUIRE(noise.frequencyGain == 1.0f);
    REQUIRE(math::almostZero(noise.pivotOffset));
    REQUIRE(math::almostZero(noise.noiseOffsets));
    REQUIRE(camera.component(Stage::Noise) == &noise);

    CameraState state = CameraState::makeDefault();
    noise.mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::almostZero(state.positionCorrection));
    REQUIRE(math::approximately(state.orientationCorrection, math::Quat::identity()));

    noise.noiseProfile = NoiseProfile{};
    REQUIRE(noise.isValid());
}

TEST_CASE("BasicMultiChannelPerlin accumulates scaled noise time", "[unit][noise][perlin]")
{
    VirtualCamera camera;
    BasicMultiChannelPerlin& noise =
        camera.setNoise<BasicMultiChannelPerlin>();

    NoiseProfile profile;
    NoiseProfile::TransformParams position;
    position.x = {1.0f, 2.0f, true};
    profile.positionNoise.push_back(position);

    noise.noiseProfile = profile;
    noise.noiseOffsets = {0.25f, 0.0f, 0.0f};
    noise.frequencyGain = 2.0f;
    noise.amplitudeGain = 2.0f;

    CameraState state = CameraState::makeDefault();
    noise.mutateCameraState(state, 0.1f);
    REQUIRE(math::approximately(noise.noiseTime(), 0.2f, 1e-5f));
    REQUIRE(math::approximately(
        state.positionCorrection.x,
        2.0f * std::cos((0.2f + 0.25f) * math::TwoPi),
        1e-4f));

    noise.amplitudeGain = 1.0f;
    state = CameraState::makeDefault();
    noise.mutateCameraState(state, 0.1f);
    REQUIRE(math::approximately(noise.noiseTime(), 0.4f, 1e-5f));
    REQUIRE(math::approximately(
        state.positionCorrection.x,
        std::cos((0.4f + 0.25f) * math::TwoPi),
        1e-4f));
}

TEST_CASE("BasicMultiChannelPerlin resets on invalid input and missing profiles", "[unit][noise][perlin]")
{
    VirtualCamera camera;
    BasicMultiChannelPerlin& noise =
        camera.setNoise<BasicMultiChannelPerlin>();
    noise.noiseProfile = makeProfile({1.0f, 0.0f, 0.0f}, {});
    noise.noiseOffsets = {1.0f, 2.0f, 3.0f};

    CameraState state = CameraState::makeDefault();
    noise.mutateCameraState(state, 0.1f);
    REQUIRE(math::approximately(state.positionCorrection.x, 1.0f, 1e-4f));

    state = CameraState::makeDefault();
    noise.mutateCameraState(state, -0.1f);
    REQUIRE(math::almostZero(state.positionCorrection));
    REQUIRE(std::abs(noise.noiseTime() - 0.1f) < 1e-6f);

    state = CameraState::makeDefault();
    noise.mutateCameraState(state, 0.0f);
    REQUIRE(math::approximately(noise.noiseTime(), 0.0f, 1e-6f));
    REQUIRE(math::approximately(state.positionCorrection.x, 1.0f, 1e-4f));

    noise.noiseProfile.reset();
    state = CameraState::makeDefault();
    noise.mutateCameraState(state, 0.1f);
    REQUIRE_FALSE(noise.isValid());
    REQUIRE(math::almostZero(state.positionCorrection));
}

TEST_CASE("BasicMultiChannelPerlin maps Unity camera axes to CineRig", "[unit][noise][perlin]")
{
    VirtualCamera camera;
    BasicMultiChannelPerlin& noise =
        camera.setNoise<BasicMultiChannelPerlin>();
    noise.noiseProfile = makeProfile({1.0f, 2.0f, 3.0f}, {});
    noise.noiseOffsets = {1.0f, 2.0f, 3.0f};

    CameraState state = CameraState::makeDefault();
    noise.mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::approximately(
        state.positionCorrection,
        {1.0f, 2.0f, -3.0f},
        1e-4f));
}

TEST_CASE("BasicMultiChannelPerlin converts orientation noise to CineRig axes", "[unit][noise][perlin]")
{
    VirtualCamera camera;
    BasicMultiChannelPerlin& noise =
        camera.setNoise<BasicMultiChannelPerlin>();
    noise.noiseOffsets = {1.0f, 2.0f, 3.0f};

    noise.noiseProfile = makeProfile({}, {90.0f, 0.0f, 0.0f});
    CameraState pitch = CameraState::makeDefault();
    noise.mutateCameraState(pitch, 1.0f / 60.0f);
    REQUIRE(math::rotate(pitch.correctedOrientation(), math::Vec3::forward).y < -0.99f);

    noise.noiseProfile = makeProfile({}, {0.0f, 90.0f, 0.0f});
    CameraState yaw = CameraState::makeDefault();
    noise.mutateCameraState(yaw, 1.0f / 60.0f);
    REQUIRE(math::rotate(yaw.correctedOrientation(), math::Vec3::forward).x > 0.99f);

    noise.noiseProfile = makeProfile({}, {0.0f, 0.0f, 90.0f});
    CameraState roll = CameraState::makeDefault();
    noise.mutateCameraState(roll, 1.0f / 60.0f);
    REQUIRE(math::rotate(roll.correctedOrientation(), math::Vec3::right).y > 0.99f);
}

TEST_CASE("BasicMultiChannelPerlin compensates rotation around a pivot", "[unit][noise][perlin]")
{
    VirtualCamera camera;
    BasicMultiChannelPerlin& noise =
        camera.setNoise<BasicMultiChannelPerlin>();
    noise.noiseProfile = makeProfile({}, {90.0f, 0.0f, 0.0f});
    noise.noiseOffsets = {1.0f, 2.0f, 3.0f};
    noise.pivotOffset = {0.0f, 0.0f, -2.0f};

    CameraState state = CameraState::makeDefault();
    noise.mutateCameraState(state, 1.0f / 60.0f);
    REQUIRE(math::approximately(
        state.positionCorrection,
        {0.0f, 2.0f, -2.0f},
        1e-4f));

    const math::Vec3 pivot{0.0f, 0.0f, -2.0f};
    const math::Vec3 pivotDisplacement =
        pivot - math::rotate(state.orientationCorrection, pivot);
    REQUIRE(math::approximately(
        state.positionCorrection,
        pivotDisplacement,
        1e-4f));
}

TEST_CASE("BasicMultiChannelPerlin re-seeds deterministically", "[unit][noise][perlin]")
{
    BasicMultiChannelPerlin noise;
    noise.seed = 123u;
    noise.reSeed();
    const math::Vec3 first = noise.noiseOffsets;
    const std::uint32_t advancedSeed = noise.seed;

    REQUIRE_FALSE(math::almostZero(first));
    REQUIRE(std::abs(first.x) <= 1000.0f);
    REQUIRE(std::abs(first.y) <= 1000.0f);
    REQUIRE(std::abs(first.z) <= 1000.0f);

    noise.seed = 123u;
    noise.reSeed();
    REQUIRE(math::approximately(noise.noiseOffsets, first, 1e-6f));

    noise.seed = advancedSeed;
    noise.reSeed();
    REQUIRE_FALSE(math::approximately(noise.noiseOffsets, first, 1e-6f));
}

TEST_CASE("BasicMultiChannelPerlin exposes the noise modifier interface", "[unit][noise][perlin]")
{
    BasicMultiChannelPerlin noise;
    REQUIRE(noise.getNoiseAmplitudeFrequency() == std::pair<float, float>{1.0f, 1.0f});

    noise.setNoiseAmplitudeFrequency({2.5f, 0.25f});
    REQUIRE(noise.amplitudeGain == 2.5f);
    REQUIRE(noise.frequencyGain == 0.25f);
    REQUIRE(noise.getNoiseAmplitudeFrequency() == std::pair<float, float>{2.5f, 0.25f});
}

