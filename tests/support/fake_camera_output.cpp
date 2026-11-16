#include "fake_camera_output.hpp"

namespace cinerig::testing
{

void FakeCameraOutput::apply(const AppliedCamera& camera)
{
    applied = camera;
    history.push_back(camera);
    ++applyCount;
}

LensSettings FakeCameraOutput::outputLens() const
{
    return inheritedLens;
}

float FakeCameraOutput::aspect() const
{
    return outputAspect;
}

void FakeCameraOutput::setOutputLens(const LensSettings& lens)
{
    inheritedLens = lens;
}

void FakeCameraOutput::setAspect(float value)
{
    outputAspect = value;
}

void FakeCameraOutput::reset()
{
    applied = AppliedCamera{};
    history.clear();
    applyCount = 0;
    inheritedLens = LensSettings::Default;
    outputAspect = 1.0f;
}

}
