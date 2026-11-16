#pragma once

#include <cstddef>
#include <span>

#include <cinerig/brain/camera_override_stack.hpp>
#include <cinerig/core/camera_interface.hpp>

namespace cinerig
{

struct TimelineShot
{
    ICinemachineCamera* camera = nullptr;
    float clipTime = 0.0f;
    float clipDuration = 0.0f;
    float weight = 0.0f;
};

class TimelineMixer
{
public:
    explicit TimelineMixer(ICameraOverrideStack& stack, int priority = 0);
    ~TimelineMixer();

    TimelineMixer(const TimelineMixer&) = delete;
    TimelineMixer& operator=(const TimelineMixer&) = delete;
    TimelineMixer(TimelineMixer&&) = delete;
    TimelineMixer& operator=(TimelineMixer&&) = delete;

    int processFrame(
        std::span<const TimelineShot> shots,
        bool playing,
        float deltaTime);
    void release();

    int overrideId() const noexcept;
    int priority() const noexcept;
    bool hasOverride() const noexcept;

private:
    ICameraOverrideStack* stack_ = nullptr;
    int priority_ = 0;
    int overrideId_ = -1;
};

} // namespace cinerig
