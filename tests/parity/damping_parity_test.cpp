#include <cinerig/math/damp.hpp>

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace cinerig::math;

TEST_CASE("Standard damping is strictly monotonic over the reference damp time", "[parity][damping]")
{
    constexpr float initial = 100.0f;
    constexpr float dampTime = 1.0f;
    constexpr int iterations = 10;

    float previous = standardDamp(initial, dampTime, 0.0f);
    REQUIRE(previous == 0.0f);
    REQUIRE(previous < initial);

    float time = 0.0f;
    for (int i = 0; i < iterations; ++i)
    {
        time += dampTime / static_cast<float>(iterations);
        if (i == iterations - 1)
            time = dampTime;

        const float current = standardDamp(initial, dampTime, time);
        REQUIRE(current > previous);
        previous = current;
    }

    REQUIRE(previous == Catch::Approx(initial * (1.0f - kNegligibleResidual)).margin(1e-4f));
}

TEST_CASE("Vector standard damping stays aligned and strictly decreasing", "[parity][damping]")
{
    const float initialValues[] = {
        7.0f, 5.0f, 3.0f, 2.0f, 1.0f, 1.0f / 2.0f, 1.0f / 3.0f, 1.0f / 5.0f, 1.0f / 7.0f,
    };
    const float deltaTimes[] = {
        0.0069444445f, 0.008333334f, 0.016666668f, 0.033333335f, 0.1f,
    };
    const float negligibleResidual = Vec3{0.01f, 0.01f, 0.01f}.length();

    int violations = 0;
    int iterations = 0;

    for (float initialValue : initialValues)
    {
        for (float dampTime = 0.1f; dampTime <= 2.0f + 1e-4f; dampTime += 0.1f)
        {
            for (float deltaTime : deltaTimes)
            {
                Vec3 vectorToDamp = Vec3::one * initialValue;
                Vec3 previousDelta = vectorToDamp;

                for (int step = 0; vectorToDamp.length() > negligibleResidual && step < 100000; ++step)
                {
                    const Vec3 delta = standardDamp(vectorToDamp, dampTime, deltaTime);
                    if (delta.length() >= previousDelta.length())
                        ++violations;
                    if (!approximately(delta.normalized(), previousDelta.normalized(), 1e-4f))
                        ++violations;

                    vectorToDamp -= delta;
                    previousDelta = delta;
                    ++iterations;
                }
            }
        }
    }

    REQUIRE(iterations > 0);
    REQUIRE(violations == 0);
}
