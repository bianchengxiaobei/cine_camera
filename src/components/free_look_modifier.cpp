#include <cinerig/components/free_look_modifier.hpp>

#include <algorithm>
#include <cmath>

#include <cinerig/math/quat.hpp>

namespace cinerig
{

namespace
{

void copyCameraMode(LensSettings& target, const LensSettings& source)
{
    target.modeOverride = source.modeOverride;
    if (target.modeOverride == LensSettings::OverrideMode::None)
    {
        target.orthographic = source.orthographic;
        target.physicalCamera = source.physicalCamera;
    }
    target.aspect = source.aspect;
}

float blendValue(float center, float top, float bottom, float modifierValue)
{
    return modifierValue >= 0.0f
        ? math::lerp(center, top, modifierValue)
        : math::lerp(bottom, center, modifierValue + 1.0f);
}

math::Vec3 blendValue(math::Vec3 center, math::Vec3 top, math::Vec3 bottom, float modifierValue)
{
    return modifierValue >= 0.0f
        ? math::lerp(center, top, modifierValue)
        : math::lerp(bottom, center, modifierValue + 1.0f);
}

math::Vec3 maxZero(math::Vec3 value)
{
    return {
        std::max(0.0f, value.x),
        std::max(0.0f, value.y),
        std::max(0.0f, value.z),
    };
}

} // namespace

void FreeLookModifier::TiltModifier::validate()
{
    tilt.top = math::clamp(tilt.top, -30.0f, 30.0f);
    tilt.bottom = math::clamp(tilt.bottom, -30.0f, 30.0f);
}

void FreeLookModifier::TiltModifier::reset(VirtualCamera& vcam)
{
    (void)vcam;
    tilt.top = -5.0f;
    tilt.bottom = 5.0f;
}

void FreeLookModifier::TiltModifier::afterPipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)deltaTime;

    const float angle = modifierValue > 0.0f
        ? math::lerp(0.0f, tilt.top, modifierValue)
        : math::lerp(tilt.bottom, 0.0f, modifierValue + 1.0f);
    const math::Quat tilted = state.rawOrientation
        * math::quatFromAxisAngle(math::Vec3::right, angle * math::DegToRad);
    state.orientationCorrection =
        math::inverse(state.correctedOrientation()) * tilted;
}

void FreeLookModifier::LensModifier::validate()
{
    lens.top.validate();
    lens.bottom.validate();
}

void FreeLookModifier::LensModifier::reset(VirtualCamera& vcam)
{
    lens.top = vcam.state().lens;
    lens.bottom = vcam.state().lens;
    copyCameraMode(lens.top, vcam.state().lens);
    copyCameraMode(lens.bottom, vcam.state().lens);
}

void FreeLookModifier::LensModifier::beforePipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)deltaTime;

    copyCameraMode(lens.top, state.lens);
    copyCameraMode(lens.bottom, state.lens);
    state.lens = modifierValue >= 0.0f
        ? LensSettings::lerp(state.lens, lens.top, modifierValue)
        : LensSettings::lerp(state.lens, lens.bottom, -modifierValue);
}

void FreeLookModifier::PositionDampingModifier::validate()
{
    damping.top = maxZero(damping.top);
    damping.bottom = maxZero(damping.bottom);
}

void FreeLookModifier::PositionDampingModifier::reset(VirtualCamera& vcam)
{
    (void)vcam;
    if (cachedComponent_ != nullptr)
        damping.top = damping.bottom = cachedComponent_->getPositionDamping();
}

void FreeLookModifier::PositionDampingModifier::beforePipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;

    if (cachedComponent_ == nullptr)
        return;

    centerDamping_ = cachedComponent_->getPositionDamping();
    cachedComponent_->setPositionDamping(blendValue(
        centerDamping_,
        damping.top,
        damping.bottom,
        modifierValue));
}

void FreeLookModifier::PositionDampingModifier::afterPipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;
    (void)modifierValue;

    if (cachedComponent_ != nullptr)
        cachedComponent_->setPositionDamping(centerDamping_);
}

void FreeLookModifier::CompositionModifier::validate()
{
    composition.top.validate();
    composition.bottom.validate();
}

void FreeLookModifier::CompositionModifier::reset(VirtualCamera& vcam)
{
    (void)vcam;
    if (cachedComponent_ != nullptr)
        composition.top = composition.bottom = cachedComponent_->getComposition();
}

void FreeLookModifier::CompositionModifier::beforePipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;

    if (cachedComponent_ == nullptr)
        return;

    centerComposition_ = cachedComponent_->getComposition();
    cachedComponent_->setComposition(modifierValue >= 0.0f
        ? ScreenComposerSettings::lerp(centerComposition_, composition.top, modifierValue)
        : ScreenComposerSettings::lerp(composition.bottom, centerComposition_, modifierValue + 1.0f));
}

void FreeLookModifier::CompositionModifier::afterPipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;
    (void)modifierValue;

    if (cachedComponent_ != nullptr)
        cachedComponent_->setComposition(centerComposition_);
}

void FreeLookModifier::DistanceModifier::validate()
{
    distance.top = std::max(0.0f, distance.top);
    distance.bottom = std::max(0.0f, distance.bottom);
}

void FreeLookModifier::DistanceModifier::reset(VirtualCamera& vcam)
{
    (void)vcam;
    if (cachedComponent_ != nullptr)
        distance.top = distance.bottom = cachedComponent_->getDistance();
}

void FreeLookModifier::DistanceModifier::beforePipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;

    if (cachedComponent_ == nullptr)
        return;

    centerDistance_ = cachedComponent_->getDistance();
    cachedComponent_->setDistance(blendValue(
        centerDistance_,
        distance.top,
        distance.bottom,
        modifierValue));
}

void FreeLookModifier::DistanceModifier::afterPipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;
    (void)modifierValue;

    if (cachedComponent_ != nullptr)
        cachedComponent_->setDistance(centerDistance_);
}

void FreeLookModifier::NoiseModifier::reset(VirtualCamera& vcam)
{
    (void)vcam;
    if (cachedComponent_ == nullptr)
        return;

    const auto value = cachedComponent_->getNoiseAmplitudeFrequency();
    noise.top = noise.bottom = {value.first, value.second};
}

void FreeLookModifier::NoiseModifier::beforePipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;

    if (cachedComponent_ == nullptr)
        return;

    centerNoise_ = cachedComponent_->getNoiseAmplitudeFrequency();
    cachedComponent_->setNoiseAmplitudeFrequency({
        blendValue(centerNoise_.first, noise.top.amplitude, noise.bottom.amplitude, modifierValue),
        blendValue(centerNoise_.second, noise.top.frequency, noise.bottom.frequency, modifierValue),
    });
}

void FreeLookModifier::NoiseModifier::afterPipeline(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime,
    float modifierValue)
{
    (void)vcam;
    (void)state;
    (void)deltaTime;
    (void)modifierValue;

    if (cachedComponent_ != nullptr)
        cachedComponent_->setNoiseAmplitudeFrequency(centerNoise_);
}

void FreeLookModifier::validate()
{
    easing = math::clamp01(easing);
    for (auto& modifier : modifiers)
    {
        if (modifier)
            modifier->validate();
    }
}

void FreeLookModifier::refreshCache(VirtualCamera& vcam)
{
    valueSource_ = detail::findModifierInterface<IModifierValueSource>(vcam);
    for (auto& modifier : modifiers)
    {
        if (modifier)
            modifier->refreshCache(vcam);
    }
}

void FreeLookModifier::reset(VirtualCamera& vcam)
{
    refreshCache(vcam);
    for (auto& modifier : modifiers)
    {
        if (modifier)
            modifier->reset(vcam);
    }
}

float FreeLookModifier::applyEasing(float value) const
{
    const float t = math::clamp01(value);
    const float amount = math::clamp01(easing);
    return 2.0f * t + 2.0f * t * t * t - 3.0f * t * t
        + amount * (t * t * t - t);
}

void FreeLookModifier::prePipelineMutateCameraStateCallback(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime)
{
    if (virtualCamera() != &vcam)
        return;

    refreshCache(vcam);
    if (valueSource_ == nullptr)
    {
        currentValue_ = 0.0f;
        return;
    }

    const float value = valueSource_->getNormalizedModifierValue();
    const float sign = value < 0.0f ? -1.0f : 1.0f;
    currentValue_ = sign * applyEasing(std::abs(value));

    for (auto& modifier : modifiers)
    {
        if (modifier)
            modifier->beforePipeline(vcam, state, deltaTime, currentValue_);
    }
}

void FreeLookModifier::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    if (virtualCamera() != &vcam || stage != Stage::Finalize || valueSource_ == nullptr)
        return;

    for (auto& modifier : modifiers)
    {
        if (modifier)
            modifier->afterPipeline(vcam, state, deltaTime, currentValue_);
    }
}

} // namespace cinerig
