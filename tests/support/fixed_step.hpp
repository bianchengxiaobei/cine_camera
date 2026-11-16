#pragma once

namespace cinerig::testing
{

// Manual clock for deterministic scenario tests. Advance it explicitly; do not
// derive dt or frame numbers from system time.
struct FixedStep
{
    float dt = 1.0f / 60.0f;
    int frame = 0;
    float time = 0.0f;

    void advance(int steps = 1)
    {
        if (steps <= 0)
            return;
        frame += steps;
        time += dt * static_cast<float>(steps);
    }

    void reset()
    {
        frame = 0;
        time = 0.0f;
    }
};

}
