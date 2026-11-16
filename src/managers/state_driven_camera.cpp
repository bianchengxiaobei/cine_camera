#include <cinerig/managers/state_driven_camera.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace cinerig::managers
{

void StateDrivenCamera::Instruction::validate()
{
    if (!std::isfinite(activateAfter) || activateAfter < 0.0f)
        activateAfter = 0.0f;
    if (!std::isfinite(minDuration) || minDuration < 0.0f)
        minDuration = 0.0f;
}

StateDrivenCamera::StateDrivenCamera(std::string name)
    : VirtualCameraManagerBase(std::move(name))
{
}

void StateDrivenCamera::reset()
{
    animatedTarget = 0;
    layerIndex = 0;
    animationState = nullptr;
    instructions.clear();
    parentHashes.clear();
    defaultBlend = {BlendStyle::EaseInOut, 0.5f};
    customBlends = BlendLookupTable{};
    activationTime_ = 0.0f;
    activeInstruction_ = 0;
    pendingActivationTime_ = 0.0f;
    pendingInstruction_ = 0;
    invalidateCameraCache();
}

void StateDrivenCamera::validate()
{
    for (Instruction& instruction : instructions)
        instruction.validate();
}

void StateDrivenCamera::cancelWait()
{
    if (pendingActivationTime_ == 0.0f
        || pendingInstruction_ < 0
        || static_cast<std::size_t>(pendingInstruction_) >= instructions.size())
    {
        return;
    }

    activeInstruction_ = pendingInstruction_;
    activationTime_ = currentTime();
    pendingActivationTime_ = 0.0f;
}

int StateDrivenCamera::activeInstruction() const noexcept
{
    return activationTime_ != 0.0f ? activeInstruction_ : -1;
}

int StateDrivenCamera::pendingInstruction() const noexcept
{
    return pendingActivationTime_ != 0.0f ? pendingInstruction_ : -1;
}

bool StateDrivenCamera::isWaiting() const noexcept
{
    return pendingActivationTime_ != 0.0f;
}

VirtualCamera* StateDrivenCamera::chooseCurrentCamera(math::Vec3, float)
{
    if (!previousStateIsValid)
        resetSelectionState();

    const std::vector<VirtualCamera*>& children = childCameras();
    if (children.empty())
    {
        activationTime_ = 0.0f;
        return nullptr;
    }

    VirtualCamera* fallback = children.front();
    IEntityWorld* world = entityWorld();
    if (animatedTarget == 0
        || animationState == nullptr
        || layerIndex < 0
        || world == nullptr
        || !world->exists(animatedTarget))
    {
        activationTime_ = 0.0f;
        return fallback;
    }

    if (activeInstruction_ < 0
        || static_cast<std::size_t>(activeInstruction_) >= instructions.size())
    {
        activeInstruction_ = 0;
        activationTime_ = 0.0f;
    }
    if (!previousStateIsValid
        || pendingInstruction_ < 0
        || static_cast<std::size_t>(pendingInstruction_) >= instructions.size())
    {
        pendingInstruction_ = 0;
        pendingActivationTime_ = 0.0f;
    }

    int hash = animationState->isInTransition(animatedTarget, layerIndex)
        ? animationState->nextStateHash(animatedTarget, layerIndex)
        : animationState->currentStateHash(animatedTarget, layerIndex);
    hash = resolveHash(hash);

    const int newInstruction = chooseInstructionForHash(hash);
    const float now = currentTime();
    if (newInstruction >= 0)
    {
        if (activationTime_ == 0.0f)
        {
            activeInstruction_ = newInstruction;
            activationTime_ = now;
            pendingActivationTime_ = 0.0f;
        }
        else if (activeInstruction_ != newInstruction
            && (pendingActivationTime_ == 0.0f
                || pendingInstruction_ != newInstruction))
        {
            pendingInstruction_ = newInstruction;
            pendingActivationTime_ = now;
        }
    }

    if (pendingActivationTime_ != 0.0f)
    {
        const Instruction& pending =
            instructions[static_cast<std::size_t>(pendingInstruction_)];
        const Instruction& active =
            instructions[static_cast<std::size_t>(activeInstruction_)];
        if ((now - pendingActivationTime_) > pending.activateAfter
            && (now - activationTime_) > active.minDuration)
        {
            activeInstruction_ = pendingInstruction_;
            activationTime_ = now;
            pendingActivationTime_ = 0.0f;
        }
    }

    if (activationTime_ != 0.0f)
    {
        VirtualCamera* active =
            instructions[static_cast<std::size_t>(activeInstruction_)].camera;
        if (active != nullptr
            && std::find(children.begin(), children.end(), active)
                != children.end())
        {
            return active;
        }
    }
    return fallback;
}

void StateDrivenCamera::resetSelectionState()
{
    activationTime_ = 0.0f;
    pendingActivationTime_ = 0.0f;
}

bool StateDrivenCamera::hasInstructionForHash(int hash) const
{
    for (const Instruction& instruction : instructions)
    {
        if (instruction.fullHash == hash)
            return true;
    }
    return false;
}

int StateDrivenCamera::lookupParent(int hash) const
{
    int parent = 0;
    for (const ParentHash& entry : parentHashes)
    {
        if (entry.hash == hash)
            parent = entry.parentHash;
    }
    return parent;
}

int StateDrivenCamera::resolveHash(int hash) const
{
    std::size_t steps = 0;
    while (hash != 0 && !hasInstructionForHash(hash))
    {
        if (steps++ > parentHashes.size())
            return 0;

        const int parent = lookupParent(hash);
        if (parent == hash)
            return 0;
        hash = parent;
    }
    return hash;
}

int StateDrivenCamera::chooseInstructionForHash(int hash) const
{
    int bestInstruction = -1;
    int bestPriority = std::numeric_limits<int>::min();
    const std::vector<VirtualCamera*>& children = childCameras();
    for (std::size_t index = 0; index < instructions.size(); ++index)
    {
        const Instruction& instruction = instructions[index];
        if (instruction.fullHash != hash)
            continue;

        VirtualCamera* camera = instruction.camera;
        if (camera == nullptr
            || std::find(children.begin(), children.end(), camera)
                == children.end()
            || !camera->isEnabled())
        {
            continue;
        }

        const int candidatePriority = camera->priority.effectiveValue();
        if (candidatePriority > bestPriority)
        {
            bestInstruction = static_cast<int>(index);
            bestPriority = candidatePriority;
        }
    }
    return bestInstruction;
}

} // namespace cinerig::managers
