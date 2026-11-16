#pragma once

#include <cinerig/core/camera_state.hpp>

namespace cinerig
{

// Final data delivered to the host camera after blending. state already
// contains corrections, Dutch, and the final lens settings.
struct AppliedCamera
{
    CameraState state{};
    float aspect = 1.0f;
    bool hasCamera = false;
};

// Non-owning output adapter. The host owns the actual camera; CineRig calls
// apply() at most once per output frame, after blending has completed.
class ICameraOutput
{
public:
    virtual ~ICameraOutput() = default;

    // When hasCamera is false the adapter should keep its previous camera or
    // use its default camera. When true, state.finalPosition(),
    // state.finalOrientation(), and state.lens are ready to use directly.
    virtual void apply(const AppliedCamera& camera) = 0;

    // Used when a lens has OverrideMode::None. The returned value describes
    // the current host camera and is not owned by the caller.
    virtual LensSettings outputLens() const = 0;

    // Current output aspect ratio (width / height).
    virtual float aspect() const = 0;
};

}
