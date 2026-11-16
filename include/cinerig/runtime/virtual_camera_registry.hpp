#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <cinerig/camera/virtual_camera.hpp>

namespace cinerig
{

// Keeps enabled cameras sorted by priority and activation order, and grouped by
// nesting level for leaf-first updates.
class VirtualCameraRegistry
{
public:
    void add(VirtualCamera& camera);
    void remove(VirtualCamera& camera);
    void setEnabled(VirtualCamera& camera, bool enabled);
    void setParent(VirtualCamera& camera, VirtualCamera* parent);
    void clear();

    std::size_t activeCameraCount() const;
    VirtualCamera* activeCamera(std::size_t index) const;
    VirtualCamera* getActiveCamera(std::size_t index) const;

    const std::vector<std::vector<VirtualCamera*>>& camerasByLevel() const;
    const std::vector<std::vector<VirtualCamera*>>& allCamerasSortedByNestingLevel() const;

    int activationSequence() const noexcept;

private:
    void rebuildLevels();
    void ensureSorted() const;
    std::uint64_t prioritySignature() const noexcept;

    mutable std::vector<VirtualCamera*> active_;
    std::vector<std::vector<VirtualCamera*>> levels_;
    mutable bool sorted_ = false;
    mutable std::uint64_t sortedSignature_ = 0;
    int activationSequence_ = 0;
};

}
