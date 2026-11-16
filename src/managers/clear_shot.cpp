#include <cinerig/managers/clear_shot.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace cinerig::managers
{

ClearShot::ClearShot(std::string name)
    : VirtualCameraManagerBase(std::move(name)),
      random_(randomSeed)
{
}

void ClearShot::reset()
{
    activateAfter = 0.0f;
    minDuration = 0.0f;
    randomizeChoice = false;
    randomSeed = 1;
    defaultBlend = {BlendStyle::EaseInOut, 0.5f};
    customBlends = BlendLookupTable{};
    invalidateCameraCache();
}

void ClearShot::validate()
{
    if (!std::isfinite(activateAfter) || activateAfter < 0.0f)
        activateAfter = 0.0f;
    if (!std::isfinite(minDuration) || minDuration < 0.0f)
        minDuration = 0.0f;
}

void ClearShot::resetRandomization()
{
    randomizedChildren_.clear();
    randomizeNow_ = true;
    random_.seed(randomSeed);
}

void ClearShot::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    if (randomizeChoice && !isBlending())
        randomizedChildren_.clear();
    VirtualCameraManagerBase::onTransitionFromCamera(from, worldUp, deltaTime);
}

VirtualCamera* ClearShot::chooseCurrentCamera(math::Vec3 worldUp, float deltaTime)
{
    if (!previousStateIsValid)
    {
        activationTime_ = 0.0f;
        pendingActivationTime_ = 0.0f;
        pendingCamera_ = nullptr;
        randomizedChildren_.clear();
    }

    VirtualCamera* live = liveChild();
    const std::vector<VirtualCamera*>& children = childCameras();
    if (children.empty())
    {
        activationTime_ = 0.0f;
        return nullptr;
    }

    std::vector<VirtualCamera*> candidates;
    if (randomizeChoice)
    {
        if (randomizedChildren_.size() != children.size())
        {
            randomizedChildren_ = children;
            if (randomizedChildren_.size() > 1)
                std::shuffle(randomizedChildren_.begin(), randomizedChildren_.end(), random_);
        }
        candidates = randomizedChildren_;
    }
    else
    {
        randomizedChildren_.clear();
        candidates = children;
    }

    if (live != nullptr && (!live->isValid() || !live->isEnabled()))
        live = nullptr;

    VirtualCamera* best = live;
    for (VirtualCamera* camera : candidates)
    {
        if (camera == nullptr || !camera->isEnabled())
            continue;

        if (!camera->previousStateIsValid)
            camera->updateCameraState(worldUp, deltaTime);

        const float quality = camera->state().shotQuality;
        const float bestQuality = best != nullptr
            ? best->state().shotQuality
            : 0.0f;
        const int candidatePriority = camera->priority.effectiveValue();
        const int bestPriority = best != nullptr
            ? best->priority.effectiveValue()
            : 0;
        if (best == nullptr
            || quality > bestQuality
            || (quality == bestQuality && candidatePriority > bestPriority)
            || (randomizeChoice && randomizeNow_ && camera != live
                && quality == bestQuality && candidatePriority == bestPriority))
        {
            best = camera;
        }
    }
    randomizeNow_ = false;

    const float now = currentTime();
    if (activationTime_ != 0.0f)
    {
        if (live == best)
        {
            pendingActivationTime_ = 0.0f;
            pendingCamera_ = nullptr;
            return best;
        }

        if (previousStateIsValid)
        {
            if (pendingActivationTime_ != 0.0f && pendingCamera_ == best)
            {
                if ((now - pendingActivationTime_) > activateAfter
                    && (now - activationTime_) > minDuration)
                {
                    randomizedChildren_.clear();
                    activationTime_ = now;
                    pendingActivationTime_ = 0.0f;
                    pendingCamera_ = nullptr;
                    return best;
                }
                return live;
            }
        }
    }

    pendingActivationTime_ = 0.0f;
    pendingCamera_ = nullptr;

    if (previousStateIsValid && activationTime_ > 0.0f)
    {
        if (activateAfter > 0.0f
            || (now - activationTime_) < minDuration)
        {
            pendingCamera_ = best;
            pendingActivationTime_ = now;
            return live;
        }
    }

    randomizedChildren_.clear();
    activationTime_ = now;
    return best;
}

void ClearShot::invalidateCameraCache()
{
    clearSelectionState();
    VirtualCameraManagerBase::invalidateCameraCache();
}

void ClearShot::clearSelectionState()
{
    activationTime_ = 0.0f;
    pendingActivationTime_ = 0.0f;
    pendingCamera_ = nullptr;
    randomizeNow_ = false;
    randomizedChildren_.clear();
    random_.seed(randomSeed);
}

} // namespace cinerig::managers
