#pragma once

#include <cinerig/core/camera_interface.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class ICameraOverrideStack
{
public:
    virtual ~ICameraOverrideStack() = default;

    virtual int setCameraOverride(
        int overrideId,
        int priority,
        ICinemachineCamera* camA,
        ICinemachineCamera* camB,
        float weightB,
        float deltaTime) = 0;

    virtual void releaseCameraOverride(int overrideId) = 0;
    virtual math::Vec3 defaultWorldUp() const = 0;
};

} // namespace cinerig
