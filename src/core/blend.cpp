#include <cinerig/core/blend.hpp>

#include <cinerig/core/camera_state_blend.hpp>

namespace cinerig
{

float Blend::blendWeight() const
{
    if (blendCurve.empty() || blendCurve.size() < 2 || isComplete())
        return 1.0f;
    if (duration <= 0.0f)
        return 1.0f;
    return math::clamp01(blendCurve.evaluate(timeInBlend / duration));
}

bool Blend::isComplete() const
{
    return timeInBlend >= duration || !isValid();
}

bool Blend::isValid() const
{
    return (camA != nullptr && camA->isValid()) || (camB != nullptr && camB->isValid());
}

bool Blend::uses(const ICinemachineCamera* cam) const
{
    if (cam == nullptr)
        return false;
    if (cam == camA || cam == camB)
        return true;

    if (const auto* nestedA = dynamic_cast<const NestedBlendSource*>(camA))
    {
        if (nestedA->blend() != nullptr && nestedA->blend()->uses(cam))
            return true;
    }
    if (const auto* nestedB = dynamic_cast<const NestedBlendSource*>(camB))
    {
        if (nestedB->blend() != nullptr && nestedB->blend()->uses(cam))
            return true;
    }
    return false;
}

CameraState Blend::state() const
{
    if (camA == nullptr || !camA->isValid())
    {
        if (camB == nullptr || !camB->isValid())
            return CameraState::Default;
        return camB->state();
    }
    if (camB == nullptr || !camB->isValid())
        return camA->state();

    if (customBlender != nullptr)
        return customBlender->getIntermediateState(*camA, *camB, blendWeight());
    return lerp(camA->state(), camB->state(), blendWeight());
}

void Blend::updateCameraState(math::Vec3 worldUp, float deltaTime)
{
    if (camA != nullptr && camA->isValid())
        camA->updateCameraState(worldUp, deltaTime);
    if (camB != nullptr && camB->isValid())
        camB->updateCameraState(worldUp, deltaTime);
}

void Blend::clearBlend()
{
    camA = nullptr;
    blendCurve = {};
    timeInBlend = 0.0f;
    duration = 0.0f;
    customBlender = nullptr;
}

void Blend::copyFrom(const Blend& src)
{
    camA = src.camA;
    camB = src.camB;
    blendCurve = src.blendCurve;
    timeInBlend = src.timeInBlend;
    duration = src.duration;
    customBlender = src.customBlender;
}

NestedBlendSource::NestedBlendSource(Blend* blend)
    : blend_(blend)
{
}

Blend* NestedBlendSource::blend() const noexcept
{
    return blend_;
}

void NestedBlendSource::setBlend(Blend* blend) noexcept
{
    blend_ = blend;
}

std::string_view NestedBlendSource::name() const
{
    return "mid-blend";
}

const CameraState& NestedBlendSource::state() const
{
    state_ = blend_ != nullptr ? blend_->state() : CameraState::Default;
    return state_;
}

bool NestedBlendSource::isValid() const
{
    return blend_ != nullptr && blend_->isValid();
}

ICinemachineMixer* NestedBlendSource::parentCamera() const
{
    return nullptr;
}

void NestedBlendSource::updateCameraState(math::Vec3 worldUp, float deltaTime)
{
    if (blend_ != nullptr)
        blend_->updateCameraState(worldUp, deltaTime);
}

}
