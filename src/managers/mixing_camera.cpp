#include <cinerig/managers/mixing_camera.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace cinerig::managers
{
namespace
{

bool containsPointer(
    const std::vector<VirtualCamera*>& children,
    const VirtualCamera* child)
{
    return std::find(children.begin(), children.end(), child) != children.end();
}

} // namespace

MixingCamera::MixingCamera(std::string name)
    : VirtualCameraManagerBase(std::move(name))
{
}

void MixingCamera::reset()
{
    weights.fill(0.0f);
    weights[0] = 1.0f;
    liveChildPercent_ = 0.0f;
    mixLiveChild_ = nullptr;
    state_ = CameraState::makeDefault();
    invalidateCameraCache();
}

void MixingCamera::validate()
{
    for (float& weight : weights)
    {
        if (!std::isfinite(weight) || weight < 0.0f)
            weight = 0.0f;
    }
}

bool MixingCamera::setWeight(int index, float weight) noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= MaxCameras)
        return false;
    weights[static_cast<std::size_t>(index)] = weight;
    return true;
}

float MixingCamera::getWeight(int index) const noexcept
{
    if (index < 0 || static_cast<std::size_t>(index) >= MaxCameras)
        return 0.0f;
    return weights[static_cast<std::size_t>(index)];
}

bool MixingCamera::setWeight(const VirtualCamera& child, float weight) noexcept
{
    const std::vector<VirtualCamera*>& children = childCameras();
    const std::size_t count = std::min(children.size(), MaxCameras);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (children[index] == &child)
            return setWeight(static_cast<int>(index), weight);
    }
    return false;
}

float MixingCamera::getWeight(const VirtualCamera& child) const noexcept
{
    const std::vector<VirtualCamera*>& children = childCameras();
    const std::size_t count = std::min(children.size(), MaxCameras);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (children[index] == &child)
            return weights[index];
    }
    return 0.0f;
}

VirtualCamera* MixingCamera::liveChild() const
{
    return previousStateIsValid ? mixLiveChild_ : nullptr;
}

bool MixingCamera::isLiveChild(
    const ICinemachineCamera* child,
    bool dominantChildOnly) const
{
    if (child == nullptr)
        return false;
    if (dominantChildOnly)
        return liveChild() == child;

    const std::vector<VirtualCamera*>& children = childCameras();
    const std::size_t count = std::min(children.size(), MaxCameras);
    for (std::size_t index = 0; index < count; ++index)
    {
        if (children[index] == child)
        {
            return weights[index] > math::Epsilon
                && children[index]->isEnabled();
        }
    }
    return false;
}

void MixingCamera::internalUpdateCameraState(math::Vec3 worldUp, float deltaTime)
{
    updateTargetCache();
    updateCameraCache();

    if (deltaTime < 0.0f)
        previousStateIsValid = false;
    if (!previousStateIsValid)
    {
        mixLiveChild_ = nullptr;
        clearLiveChildren();
    }

    const std::vector<VirtualCamera*>& children = childCameras();
    const std::size_t count = std::min(children.size(), MaxCameras);

    CameraState mixed = state_;
    bool hasContributor = false;
    float totalWeight = 0.0f;
    float highestWeight = 0.0f;
    VirtualCamera* highestChild = nullptr;
    std::vector<VirtualCamera*> weightedChildren;
    weightedChildren.reserve(count);

    for (std::size_t index = 0; index < count; ++index)
    {
        VirtualCamera* child = children[index];
        if (child == nullptr || !child->isEnabled())
            continue;

        const float weight = weights[index];
        if (!std::isfinite(weight) || weight <= math::Epsilon)
            continue;

        if (!child->previousStateIsValid)
            child->updateCameraState(worldUp, deltaTime);

        totalWeight += weight;
        if (!hasContributor)
        {
            mixed = child->state();
            hasContributor = true;
        }
        else
        {
            mixed = CameraState::lerp(
                mixed,
                child->state(),
                weight / totalWeight);
        }

        if (weight > highestWeight)
        {
            highestWeight = weight;
            highestChild = child;
        }
        weightedChildren.push_back(child);
    }

    for (VirtualCamera* child : liveChildren_)
    {
        if (child != nullptr && !containsPointer(weightedChildren, child))
            child->setLive(false);
    }
    for (VirtualCamera* child : weightedChildren)
        child->setLive(true);
    liveChildren_ = std::move(weightedChildren);

    if (hasContributor)
        state_ = mixed;
    liveChildPercent_ = totalWeight > 0.001f
        ? highestWeight * 100.0f / totalWeight
        : 0.0f;
    mixLiveChild_ = highestChild;

    invokePostPipelineStage(Stage::Finalize, state_, deltaTime);
    pushStateToOwner(state_);
    previousStateIsValid = true;
}

void MixingCamera::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    for (VirtualCamera* child : childCameras())
    {
        if (child != nullptr)
            child->onTransitionFromCamera(from, worldUp, deltaTime);
    }
    VirtualCameraManagerBase::onTransitionFromCamera(from, worldUp, deltaTime);
}

VirtualCamera* MixingCamera::chooseCurrentCamera(math::Vec3, float)
{
    return nullptr;
}

void MixingCamera::invalidateCameraCache()
{
    clearLiveChildren();
    mixLiveChild_ = nullptr;
    liveChildPercent_ = 0.0f;
    VirtualCameraManagerBase::invalidateCameraCache();
}

void MixingCamera::clearLiveChildren()
{
    for (VirtualCamera* child : liveChildren_)
    {
        if (child != nullptr)
            child->setLive(false);
    }
    liveChildren_.clear();
}

} // namespace cinerig::managers
