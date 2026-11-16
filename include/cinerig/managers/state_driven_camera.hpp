#pragma once

#include <string>
#include <vector>

#include <cinerig/camera/virtual_camera_manager_base.hpp>
#include <cinerig/engine/animation_state.hpp>

namespace cinerig::managers
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

class StateDrivenCamera final : public VirtualCameraManagerBase
{
public:
    struct Instruction
    {
        int fullHash = 0;
        VirtualCamera* camera = nullptr;
        float activateAfter = 0.0f;
        float minDuration = 0.0f;

        void validate();
    };

    struct ParentHash
    {
        int hash = 0;
        int parentHash = 0;
    };

    EntityId animatedTarget = 0;
    int layerIndex = 0;
    IAnimationStateProvider* animationState = nullptr;
    std::vector<Instruction> instructions;
    std::vector<ParentHash> parentHashes;

    explicit StateDrivenCamera(std::string name = "StateDrivenCamera");

    void reset();
    void validate();
    void cancelWait();

    int activeInstruction() const noexcept;
    int pendingInstruction() const noexcept;
    bool isWaiting() const noexcept;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;

private:
    void resetSelectionState();
    bool hasInstructionForHash(int hash) const;
    int lookupParent(int hash) const;
    int resolveHash(int hash) const;
    int chooseInstructionForHash(int hash) const;

    float activationTime_ = 0.0f;
    int activeInstruction_ = 0;
    float pendingActivationTime_ = 0.0f;
    int pendingInstruction_ = 0;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace cinerig::managers
