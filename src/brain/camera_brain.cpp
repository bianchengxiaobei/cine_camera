#include <cinerig/brain/camera_brain.hpp>

#include <algorithm>
#include <utility>

namespace cinerig
{

CameraBrain::CameraBrain(
    CameraSystem& system,
    ICameraOutput& output,
    const BrainConfig& config)
    : system_(system),
      output_(output),
      config_(config),
      blendLookup_(config.defaultBlend),
      lastConfigBlend_(config.defaultBlend)
{
    blendManager_.setEvents(&events_);
    blendManager_.setLookupBlendFunction(
        [this](const ICinemachineCamera* outgoing, const ICinemachineCamera* incoming)
        {
            return blendLookup_.lookup(outgoing, incoming);
        });
}

void CameraBrain::update(float deltaTime)
{
    updateInternal(UpdateClock::Late, deltaTime);
}

void CameraBrain::fixedUpdate(float fixedDeltaTime)
{
    updateInternal(UpdateClock::Fixed, fixedDeltaTime);
}

void CameraBrain::manualUpdate(float deltaTime, std::uint64_t frame)
{
    lastUpdateFrame_ = frame;
    updateInternal(UpdateClock::Late, deltaTime);
}

void CameraBrain::prepareLiveCamera()
{
    auto* camera = dynamic_cast<VirtualCamera*>(topCameraFromPriorityQueue());
    if (camera != nullptr && blendManager_.activeVirtualCamera() == camera)
        camera->setLive(true);
}

void CameraBrain::processFrame(UpdateClock clock, float deltaTime)
{
    processFrameInternal(clock, effectiveDeltaTime(clock, deltaTime));
}

void CameraBrain::processFrameInternal(UpdateClock clock, float effectiveDelta)
{
    (void)clock;
    const math::Vec3 worldUp = effectiveWorldUp();

    if (config_.defaultBlend.style != lastConfigBlend_.style
        || !math::approximately(config_.defaultBlend.time, lastConfigBlend_.time))
    {
        blendLookup_.setDefaultBlend(config_.defaultBlend);
        lastConfigBlend_ = config_.defaultBlend;
    }
    ICinemachineCamera* topCamera = topCameraFromPriorityQueue();
    blendManager_.updateRootFrame(this, topCamera, worldUp, effectiveDelta);
    blendManager_.computeCurrentBlend();
    bool wasCut = false;
    blendManager_.processActiveCamera(this, worldUp, effectiveDelta, &wasCut);
    if (wasCut && postProcessor_ != nullptr)
        postProcessor_->resetHistory();
    applyToOutput();
}

ICinemachineCamera* CameraBrain::activeVirtualCamera() const
{
    return blendManager_.activeVirtualCamera();
}

bool CameraBrain::isBlending() const
{
    return blendManager_.isBlending();
}

const Blend* CameraBrain::activeBlend() const
{
    return blendManager_.activeBlend();
}

const CameraState& CameraBrain::state() const
{
    return currentState_;
}

int CameraBrain::setCameraOverride(
    int overrideId,
    int priority,
    ICinemachineCamera* camA,
    ICinemachineCamera* camB,
    float weightB,
    float deltaTime)
{
    return blendManager_.setCameraOverride(
        overrideId, priority, camA, camB, weightB, deltaTime);
}

void CameraBrain::releaseCameraOverride(int overrideId)
{
    blendManager_.releaseCameraOverride(overrideId);
}

math::Vec3 CameraBrain::defaultWorldUp() const
{
    return effectiveWorldUp();
}

void CameraBrain::resetState()
{
    blendManager_.reset();
    currentState_ = CameraState::makeDefault();
    lastUpdateFrame_ = 0;
}

void CameraBrain::onCameraDestroyed(const ICinemachineCamera* camera)
{
    if (camera == nullptr)
        return;
    if (blendManager_.isLive(camera) || blendManager_.activeVirtualCamera() == camera)
    {
        blendManager_.reset();
        currentState_ = CameraState::makeDefault();
    }
}

void CameraBrain::applyToOutput()
{
    const bool hasActiveCamera = activeVirtualCamera() != nullptr;
    CameraState state = hasActiveCamera
        ? blendManager_.cameraState()
        : CameraState::makeDefault();

    if (!hasActiveCamera)
    {
        state.lens = output_.outputLens();
        state.blendHint |= BlendHint::NoTransform | BlendHint::NoLens;
    }
    else
    {
        applyLensInheritance(state);
    }

    currentState_ = state;

    AppliedCamera applied;
    applied.state = state;
    applied.aspect = output_.aspect();
    applied.hasCamera = hasActiveCamera;
    output_.apply(applied);
    if (postProcessor_ != nullptr)
        postProcessor_->apply(applied);
    events_.emitCameraUpdated(*this);
}

std::string_view CameraBrain::name() const
{
    return "CameraBrain";
}

bool CameraBrain::isValid() const
{
    return true;
}

ICinemachineMixer* CameraBrain::parentCamera() const
{
    return nullptr;
}

bool CameraBrain::isLiveChild(
    const ICinemachineCamera* child,
    bool dominantChildOnly) const
{
    if (dominantChildOnly)
        return activeVirtualCamera() == child;
    return blendManager_.isLiveChild(child);
}

BrainConfig& CameraBrain::config() noexcept
{
    return config_;
}

const BrainConfig& CameraBrain::config() const noexcept
{
    return config_;
}

void CameraBrain::setConfig(const BrainConfig& config)
{
    config_ = config;
    lastConfigBlend_ = config_.defaultBlend;
    blendLookup_.setDefaultBlend(config_.defaultBlend);
}

BlendLookup& CameraBrain::blendLookup() noexcept
{
    return blendLookup_;
}

const BlendLookup& CameraBrain::blendLookup() const noexcept
{
    return blendLookup_;
}

void CameraBrain::setBlendOverride(BlendLookup::BlendOverride override)
{
    blendLookup_.setOverride(std::move(override));
}

void CameraBrain::setCustomBlender(CameraBlendStack::CustomBlenderFunction customBlender)
{
    blendManager_.setCustomBlenderFunction(std::move(customBlender));
}

void CameraBrain::setPostProcessor(IPostProcessor* processor) noexcept
{
    postProcessor_ = processor;
}

IPostProcessor* CameraBrain::postProcessor() const noexcept
{
    return postProcessor_;
}

BlendManager& CameraBrain::blendManager() noexcept
{
    return blendManager_;
}

const BlendManager& CameraBrain::blendManager() const noexcept
{
    return blendManager_;
}

BrainEvents& CameraBrain::events() noexcept
{
    return events_;
}

const BrainEvents& CameraBrain::events() const noexcept
{
    return events_;
}

void CameraBrain::updateInternal(UpdateClock clock, float deltaTime)
{
    const float effectiveDelta = effectiveDeltaTime(clock, deltaTime);
    prepareLiveCamera();
    system_.updateCameras(
        clock,
        effectiveDelta,
        config_.channelMask,
        effectiveWorldUp(),
        config_.updateMethod == UpdateMethod::Smart);
    processFrameInternal(clock, effectiveDelta);
}

ICinemachineCamera* CameraBrain::topCameraFromPriorityQueue() const
{
    const std::size_t cameraCount = system_.activeCameraCount();
    for (std::size_t index = 0; index < cameraCount; ++index)
    {
        VirtualCamera* camera = system_.activeCamera(index);
        if (camera != nullptr
            && camera->isValid()
            && hasAnyChannel(config_.channelMask, camera->outputChannel))
        {
            return camera;
        }
    }
    return nullptr;
}

math::Vec3 CameraBrain::effectiveWorldUp() const
{
    return config_.useWorldUpOverride ? config_.worldUpOverride : system_.worldUp();
}

float CameraBrain::effectiveDeltaTime(UpdateClock clock, float deltaTime) const
{
    const float override = blendManager_.deltaTimeOverride();
    if (override > 0.0f)
        return override;
    if (config_.ignoreTimeScale)
        return system_.entityWorld().unscaledDeltaTime(clock);
    return deltaTime;
}

void CameraBrain::applyLensInheritance(CameraState& state) const
{
    if (hasBlendHint(state.blendHint, BlendHint::NoLens))
        return;

    const LensSettings inherited = output_.outputLens();
    state.lens.aspect = output_.aspect();

    if (state.lens.modeOverride == LensSettings::OverrideMode::None)
    {
        state.lens.orthographic = inherited.orthographic;
        state.lens.physicalCamera = inherited.physicalCamera;
    }

    if (config_.lensModeOverride.enabled)
    {
        const LensSettings::OverrideMode mode =
            state.lens.modeOverride != LensSettings::OverrideMode::None
            ? state.lens.modeOverride
            : config_.lensModeOverride.defaultMode;

        if (mode != LensSettings::OverrideMode::None)
        {
            state.lens.orthographic = mode == LensSettings::OverrideMode::Orthographic;
            state.lens.physicalCamera = mode == LensSettings::OverrideMode::Physical;
        }
    }
}

} // namespace cinerig

