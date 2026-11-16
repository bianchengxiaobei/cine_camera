#include <cinerig/brain/timeline_mixer.hpp>

#include <cstddef>
#include <utility>

namespace cinerig
{

TimelineMixer::TimelineMixer(ICameraOverrideStack& stack, int priority)
    : stack_(&stack),
      priority_(priority)
{
}

TimelineMixer::~TimelineMixer()
{
    release();
}

int TimelineMixer::processFrame(
    std::span<const TimelineShot> shots,
    bool playing,
    float deltaTime)
{
    const auto isActive = [playing](const TimelineShot& shot)
    {
        return playing
            && shot.camera != nullptr
            && shot.camera->isValid()
            && shot.weight > 0.0f;
    };

    int activeInputs = 0;
    std::size_t indexA = 0;
    std::size_t indexB = 0;
    bool hasA = false;
    bool hasB = false;
    bool incomingIsA = false;
    float weightB = 1.0f;

    for (std::size_t index = 0; index < shots.size(); ++index)
    {
        const TimelineShot& shot = shots[index];
        if (!isActive(shot))
            continue;

        indexA = indexB;
        hasA = hasB;
        indexB = index;
        hasB = true;
        weightB = shot.weight;

        if (++activeInputs == 2)
        {
            const TimelineShot& current = shots[indexB];
            const TimelineShot& previous = shots[indexA];
            incomingIsA = current.clipTime >= previous.clipTime;
            if (current.clipTime == previous.clipTime)
                incomingIsA = current.clipDuration < previous.clipDuration;
            break;
        }
    }

    if (activeInputs == 1
        && weightB < 1.0f
        && shots[indexB].clipTime > shots[indexB].clipDuration * 0.5f)
    {
        incomingIsA = true;
    }

    if (incomingIsA)
    {
        std::swap(indexA, indexB);
        std::swap(hasA, hasB);
        weightB = 1.0f - weightB;
    }

    ICinemachineCamera* camA = hasA ? shots[indexA].camera : nullptr;
    ICinemachineCamera* camB = hasB ? shots[indexB].camera : nullptr;

    overrideId_ = stack_->setCameraOverride(
        overrideId_,
        priority_,
        camA,
        camB,
        weightB,
        deltaTime);
    return overrideId_;
}

void TimelineMixer::release()
{
    if (overrideId_ < 0)
        return;
    stack_->releaseCameraOverride(overrideId_);
    overrideId_ = -1;
}

int TimelineMixer::overrideId() const noexcept
{
    return overrideId_;
}

int TimelineMixer::priority() const noexcept
{
    return priority_;
}

bool TimelineMixer::hasOverride() const noexcept
{
    return overrideId_ >= 0;
}

} // namespace cinerig
