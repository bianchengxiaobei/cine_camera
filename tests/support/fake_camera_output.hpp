#pragma once

#include <vector>

#include <cinerig/engine/camera_output.hpp>

namespace cinerig::testing
{

// Captures every state delivered by the runtime. The inherited lens and aspect
// are configurable to exercise LensSettings::OverrideMode::None inheritance.
class FakeCameraOutput final : public ICameraOutput
{
public:
    void apply(const AppliedCamera& camera) override;
    LensSettings outputLens() const override;
    float aspect() const override;

    void setOutputLens(const LensSettings& lens);
    void setAspect(float value);
    void reset();

    AppliedCamera applied;
    std::vector<AppliedCamera> history;
    int applyCount = 0;
    LensSettings inheritedLens = LensSettings::Default;
    float outputAspect = 1.0f;
};

}
