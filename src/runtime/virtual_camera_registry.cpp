#include <cinerig/runtime/virtual_camera_registry.hpp>

#include <algorithm>
#include <limits>

namespace cinerig
{

void VirtualCameraRegistry::add(VirtualCamera& camera)
{
    const auto it = std::find(active_.begin(), active_.end(), &camera);
    if (it != active_.end())
        return;

    camera.activationId_ = activationSequence_++;
    active_.push_back(&camera);
    sorted_ = false;
    rebuildLevels();
}

void VirtualCameraRegistry::remove(VirtualCamera& camera)
{
    active_.erase(std::remove(active_.begin(), active_.end(), &camera), active_.end());
    sorted_ = false;
    rebuildLevels();
}

void VirtualCameraRegistry::setEnabled(VirtualCamera& camera, bool enabled)
{
    if (enabled)
        add(camera);
    else
        remove(camera);
}

void VirtualCameraRegistry::setParent(VirtualCamera&, VirtualCamera*)
{
    rebuildLevels();
}

void VirtualCameraRegistry::clear()
{
    active_.clear();
    levels_.clear();
    sorted_ = true;
    sortedSignature_ = 0;
    activationSequence_ = 0;
}

std::size_t VirtualCameraRegistry::activeCameraCount() const
{
    return active_.size();
}

VirtualCamera* VirtualCameraRegistry::activeCamera(std::size_t index) const
{
    if (index >= active_.size())
        return nullptr;

    ensureSorted();
    return active_[index];
}

VirtualCamera* VirtualCameraRegistry::getActiveCamera(std::size_t index) const
{
    return activeCamera(index);
}

const std::vector<std::vector<VirtualCamera*>>& VirtualCameraRegistry::camerasByLevel() const
{
    return levels_;
}

const std::vector<std::vector<VirtualCamera*>>&
VirtualCameraRegistry::allCamerasSortedByNestingLevel() const
{
    return levels_;
}

int VirtualCameraRegistry::activationSequence() const noexcept
{
    return activationSequence_;
}

void VirtualCameraRegistry::ensureSorted() const
{
    const std::uint64_t signature = prioritySignature();
    if (sorted_ && signature == sortedSignature_)
        return;

    std::stable_sort(active_.begin(), active_.end(), [](const VirtualCamera* a, const VirtualCamera* b)
    {
        const int priorityA = a->priority.effectiveValue();
        const int priorityB = b->priority.effectiveValue();
        if (priorityA != priorityB)
            return priorityA > priorityB;
        return a->activationId() > b->activationId();
    });
    sorted_ = true;
    sortedSignature_ = prioritySignature();
}

std::uint64_t VirtualCameraRegistry::prioritySignature() const noexcept
{
    constexpr std::uint64_t kOffsetBasis = 1469598103934665603ull;
    constexpr std::uint64_t kPrime = 1099511628211ull;

    std::uint64_t hash = kOffsetBasis;
    for (const VirtualCamera* camera : active_)
    {
        if (camera == nullptr)
            continue;

        hash ^= static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(camera->priority.effectiveValue()));
        hash *= kPrime;
        hash ^= static_cast<std::uint64_t>(
            static_cast<std::uint32_t>(camera->activationId()));
        hash *= kPrime;
    }
    hash ^= static_cast<std::uint64_t>(active_.size());
    return hash;
}

void VirtualCameraRegistry::rebuildLevels()
{
    levels_.clear();
    for (VirtualCamera* camera : active_)
    {
        int level = 0;
        VirtualCamera* parent = camera->parentVirtualCamera();
        while (parent != nullptr)
        {
            ++level;
            if (level > 64)
                break;
            parent = parent->parentVirtualCamera();
        }

        if (levels_.size() <= static_cast<std::size_t>(level))
            levels_.resize(static_cast<std::size_t>(level) + 1u);
        levels_[static_cast<std::size_t>(level)].push_back(camera);
    }
}

}
