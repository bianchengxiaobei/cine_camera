#include <cinerig/managers/sequencer_camera.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace cinerig::managers
{

void SequencerCamera::Instruction::validate()
{
    if (!std::isfinite(hold) || hold < 0.0f)
        hold = 0.0f;
}

SequencerCamera::SequencerCamera(std::string name)
    : VirtualCameraManagerBase(std::move(name))
{
}

void SequencerCamera::reset()
{
    loop = false;
    instructions.clear();
    defaultBlend = {BlendStyle::EaseInOut, 0.5f};
    customBlends = BlendLookupTable{};
    clearSequenceState();
    invalidateCameraCache();
}

void SequencerCamera::validate()
{
    for (Instruction& instruction : instructions)
        instruction.validate();
}

int SequencerCamera::currentInstruction() const noexcept
{
    return currentInstruction_;
}

void SequencerCamera::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    activationTime_ = currentTime();
    currentInstruction_ = 0;
    VirtualCameraManagerBase::onTransitionFromCamera(from, worldUp, deltaTime);
}

VirtualCamera* SequencerCamera::chooseCurrentCamera(math::Vec3, float deltaTime)
{
    if (!previousStateIsValid)
        currentInstruction_ = -1;

    advanceCurrentInstruction(deltaTime);

    if (currentInstruction_ < 0
        || static_cast<std::size_t>(currentInstruction_) >= instructions.size())
    {
        return nullptr;
    }

    VirtualCamera* camera =
        instructions[static_cast<std::size_t>(currentInstruction_)].camera;
    const std::vector<VirtualCamera*>& children = childCameras();
    if (camera == nullptr
        || std::find(children.begin(), children.end(), camera) == children.end())
    {
        return nullptr;
    }
    return camera;
}

BlendDefinition SequencerCamera::lookupBlend(
    const ICinemachineCamera* outgoing,
    const ICinemachineCamera* incoming) const
{
    if (currentInstruction_ >= 0
        && static_cast<std::size_t>(currentInstruction_) < instructions.size())
    {
        return instructions[static_cast<std::size_t>(currentInstruction_)].blend;
    }
    return VirtualCameraManagerBase::lookupBlend(outgoing, incoming);
}

void SequencerCamera::advanceCurrentInstruction(float deltaTime)
{
    if (childCameras().empty() || instructions.empty() || activationTime_ < 0.0f)
    {
        activationTime_ = -1.0f;
        currentInstruction_ = -1;
        return;
    }

    const float now = currentTime();
    if (currentInstruction_ < 0 || deltaTime < 0.0f)
    {
        activationTime_ = now;
        currentInstruction_ = 0;
    }

    const int lastInstruction = static_cast<int>(instructions.size()) - 1;
    if (currentInstruction_ > lastInstruction)
    {
        activationTime_ = now;
        currentInstruction_ = lastInstruction;
    }

    const Instruction& current =
        instructions[static_cast<std::size_t>(currentInstruction_)];
    const float holdTime = current.hold + current.blend.blendTime();
    const float minHold = (currentInstruction_ < lastInstruction || loop)
        ? 0.0f
        : std::numeric_limits<float>::max();
    if ((now - activationTime_) > std::max(minHold, holdTime))
    {
        activationTime_ = now;
        ++currentInstruction_;
        if (loop && currentInstruction_ == static_cast<int>(instructions.size()))
            currentInstruction_ = 0;
    }
}

void SequencerCamera::clearSequenceState()
{
    activationTime_ = -1.0f;
    currentInstruction_ = 0;
}

} // namespace cinerig::managers
