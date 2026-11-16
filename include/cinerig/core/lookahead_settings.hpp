#pragma once

namespace cinerig
{

struct LookaheadSettings
{
    bool enabled = false;
    float time = 0.0f;
    float smoothing = 0.0f;
    bool ignoreY = false;

    static const LookaheadSettings Default;
};

inline const LookaheadSettings LookaheadSettings::Default{};

}
