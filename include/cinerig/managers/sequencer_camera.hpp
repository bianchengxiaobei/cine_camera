#pragma once

#include <string>
#include <vector>

#include <cinerig/camera/virtual_camera_manager_base.hpp>

namespace cinerig::managers
{

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

class SequencerCamera final : public VirtualCameraManagerBase
{
public:
    struct Instruction
    {
        VirtualCamera* camera = nullptr;
        BlendDefinition blend{BlendStyle::Cut, 0.0f};
        float hold = 0.0f;

        void validate();
    };

    bool loop = false;
    std::vector<Instruction> instructions;

    explicit SequencerCamera(std::string name = "SequencerCamera");

    void reset();
    void validate();
    int currentInstruction() const noexcept;

    void onTransitionFromCamera(
        const ICinemachineCamera* from,
        math::Vec3 worldUp,
        float deltaTime) override;

protected:
    VirtualCamera* chooseCurrentCamera(math::Vec3 worldUp, float deltaTime) override;
    BlendDefinition lookupBlend(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming) const override;

private:
    void advanceCurrentInstruction(float deltaTime);
    void clearSequenceState();

    float activationTime_ = -1.0f;
    int currentInstruction_ = 0;
};

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

} // namespace cinerig::managers
