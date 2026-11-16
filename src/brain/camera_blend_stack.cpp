#include <cinerig/brain/camera_blend_stack.hpp>

#include <algorithm>
#include <utility>

namespace cinerig
{

namespace
{

void updateBlendCameraStates(
    Blend& blend,
    math::Vec3 worldUp,
    float deltaTime,
    const ICinemachineCamera* excluded)
{
    const auto updateCamera = [&](ICinemachineCamera* camera)
    {
        if (camera == nullptr || camera == excluded || !camera->isValid())
            return;

        if (auto* nested = dynamic_cast<NestedBlendSource*>(camera);
            nested != nullptr && nested->blend() != nullptr)
        {
            updateBlendCameraStates(*nested->blend(), worldUp, deltaTime, excluded);
            return;
        }
        camera->updateCameraState(worldUp, deltaTime);
    };

    updateCamera(blend.camA);
    updateCamera(blend.camB);
}

} // namespace

SnapshotBlendSource::SnapshotBlendSource(
    const ICinemachineCamera* source,
    float remainingTimeInBlend)
    : remainingTimeInBlend(remainingTimeInBlend)
{
    takeSnapshot(source);
}

void SnapshotBlendSource::takeSnapshot(const ICinemachineCamera* source)
{
    state_ = source != nullptr ? source->state() : CameraState::makeDefault();
    state_.blendHint &= ~BlendHint::FreezeWhenBlendingOut;
    name_ = source != nullptr ? std::string("*") + std::string(source->name()) : std::string("(null)");
}

std::string_view SnapshotBlendSource::name() const
{
    return name_;
}

const CameraState& SnapshotBlendSource::state() const
{
    return state_;
}

bool SnapshotBlendSource::isValid() const
{
    return true;
}

ICinemachineMixer* SnapshotBlendSource::parentCamera() const
{
    return nullptr;
}

FrozenBlendSource::FrozenBlendSource(Blend* blend)
    : NestedBlendSource(blend)
{
}

ICinemachineCamera* CameraBlendStack::Frame::getSnapshotIfAppropriate(
    ICinemachineCamera* camera,
    float weight)
{
    if (camera == nullptr
        || !hasBlendHint(camera->state().blendHint, BlendHint::FreezeWhenBlendingOut))
    {
        clearSnapshot();
        return camera;
    }

    if (snapshotSource != camera || snapshotBlendWeight > weight)
    {
        snapshot.takeSnapshot(camera);
        snapshotSource = camera;
        snapshotBlendWeight = weight;
    }
    return &snapshot;
}

void CameraBlendStack::Frame::clearSnapshot()
{
    snapshot.takeSnapshot(nullptr);
    snapshotSource = nullptr;
    snapshotBlendWeight = 0.0f;
}

CameraBlendStack::CameraBlendStack() = default;

void CameraBlendStack::setEvents(BrainEvents* events) noexcept
{
    events_ = events;
}

void CameraBlendStack::setLookupBlendFunction(LookupBlendFunction lookup)
{
    lookupBlend_ = std::move(lookup);
}

void CameraBlendStack::setCustomBlenderFunction(CustomBlenderFunction customBlender)
{
    customBlender_ = std::move(customBlender);
}

void CameraBlendStack::updateRootFrame(
    ICinemachineMixer* context,
    ICinemachineCamera* activeCamera,
    math::Vec3 worldUp,
    float deltaTime)
{
    ensureRootFrame();
    Frame& frame = *frames_.front();
    ICinemachineCamera* outgoingCamera = frame.source.camB;

    if (activeCamera != outgoingCamera)
    {
        bool backingOutOfBlend = false;
        float normalizedBlendPosition = 0.0f;
        float duration = 0.0f;

        if (lookupBlend_
            && activeCamera != nullptr && activeCamera->isValid()
            && outgoingCamera != nullptr && outgoingCamera->isValid()
            && deltaTime >= 0.0f)
        {
            const BlendDefinition blend = lookupBlend_(outgoingCamera, activeCamera);
            if (blend.blendTime() > math::Epsilon)
            {
                backingOutOfBlend =
                    frame.source.camA == activeCamera && frame.source.camB == outgoingCamera;
                if (backingOutOfBlend && frame.blend.duration > math::Epsilon)
                {
                    normalizedBlendPosition = frame.midBlendNormalizedStartPoint
                        + (1.0f - frame.midBlendNormalizedStartPoint)
                            * frame.blend.timeInBlend / frame.blend.duration;
                }

                frame.source.camA = outgoingCamera;
                frame.source.blendCurve = blend.curve();
                duration = blend.blendTime();
            }
        }

        frame.source.camB = activeCamera;
        frame.source.duration = duration;
        frame.source.timeInBlend = 0.0f;
        frame.source.customBlender = duration > math::Epsilon && customBlender_
            ? customBlender_(outgoingCamera, activeCamera)
            : nullptr;

        ICinemachineCamera* actualCamA = nullptr;
        if (frame.blend.isComplete())
        {
            actualCamA = frame.getSnapshotIfAppropriate(outgoingCamera, 0.0f);
        }
        else
        {
            bool snapshot = hasBlendHint(
                frame.blend.state().blendHint, BlendHint::FreezeWhenBlendingOut);

            if (!snapshot
                && activeCamera != nullptr
                && hasBlendHint(activeCamera->state().blendHint, BlendHint::InheritPosition)
                && frame.blend.uses(activeCamera))
            {
                snapshot = true;
            }

            if (!snapshot)
            {
                auto* nested = dynamic_cast<NestedBlendSource*>(frame.blend.camA);
                if (nested != nullptr && nested->blend() != nullptr)
                {
                    auto* nestedNested =
                        dynamic_cast<NestedBlendSource*>(nested->blend()->camA);
                    if (nestedNested != nullptr && nestedNested->blend() != nullptr)
                    {
                        nestedNested->blend()->camA =
                            makeSnapshot(nestedNested->blend()->camA, 0.0f);
                    }
                }
            }

            if (backingOutOfBlend)
            {
                duration *= normalizedBlendPosition;
                normalizedBlendPosition = 1.0f - normalizedBlendPosition;
            }

            if (snapshot)
            {
                actualCamA = makeSnapshot(
                    &frame.self, frame.blend.duration - frame.blend.timeInBlend);
            }
            else
            {
                actualCamA = makeNestedBlend(frame.blend, backingOutOfBlend);
            }
        }

        frame.blend.camA = outgoingCamera;
        frame.blend.camB = activeCamera;
        frame.blend.blendCurve = frame.source.blendCurve;
        frame.blend.duration = duration;
        frame.blend.timeInBlend = 0.0f;
        frame.blend.customBlender = frame.source.customBlender;
        frame.midBlendNormalizedStartPoint = normalizedBlendPosition;

        if (duration > math::Epsilon && events_ != nullptr)
            events_->emitBlendCreated(BlendEventParams{context, &frame.blend});

        frame.blend.camA = actualCamA;
        frame.blend.camB = activeCamera;
    }

    if (advanceBlend(frame.blend, deltaTime, false))
    {
        frame.source.clearBlend();
        frame.clearSnapshot();
    }
    updateBlendCameraStates(frame.blend, worldUp, deltaTime, activeCamera);
}

void CameraBlendStack::computeCurrentBlend()
{
    ensureRootFrame();

    std::size_t lastActive = 0;
    for (std::size_t index = 1; index < frames_.size(); ++index)
    {
        Frame& frame = *frames_[index];
        if (!frame.active())
            continue;

        frame.blend.copyFrom(frame.source);
        if (frame.source.camA != nullptr)
        {
            frame.blend.camA = frame.getSnapshotIfAppropriate(
                frame.source.camA, frame.source.timeInBlend);
        }

        if (!frame.blend.isComplete())
        {
            if (frame.blend.camA == nullptr)
            {
                frame.blend.camA = frame.getSnapshotIfAppropriate(
                    &frames_[lastActive]->self, frame.blend.timeInBlend);
                frame.blend.customBlender = customBlender_
                    ? customBlender_(frame.blend.camA, frame.blend.camB)
                    : nullptr;
            }
            if (frame.blend.camB == nullptr)
            {
                frame.blend.camB = &frames_[lastActive]->self;
                frame.blend.customBlender = customBlender_
                    ? customBlender_(frame.blend.camA, frame.blend.camB)
                    : nullptr;
            }
        }

        lastActive = index;
    }

    currentBlend_.copyFrom(frames_[lastActive]->blend);
}

int CameraBlendStack::setCameraOverride(
    int overrideId,
    int priority,
    ICinemachineCamera* camA,
    ICinemachineCamera* camB,
    float weightB,
    float deltaTime)
{
    ensureRootFrame();
    if (overrideId < 0)
        overrideId = ++nextFrameId_;

    std::size_t frameIndex = 0;
    for (std::size_t index = frames_.size(); index-- > 1;)
    {
        if (frames_[index]->id == overrideId)
        {
            frameIndex = index;
            break;
        }
    }

    if (frameIndex == 0)
    {
        auto frame = std::make_unique<Frame>();
        frame->id = overrideId;
        frame->priority = priority;
        frame->source.duration = 1.0f;
        frame->source.blendCurve = math::Curve::linear();

        std::size_t index = 1;
        for (; index < frames_.size(); ++index)
        {
            if (frames_[index]->priority > priority)
                break;
        }
        frameIndex = index;
        frames_.insert(frames_.begin() + static_cast<std::ptrdiff_t>(index), std::move(frame));
    }

    Frame& frame = *frames_[frameIndex];
    frame.priority = priority;
    frame.deltaTimeOverride = deltaTime;
    frame.overrideWeight = weightB;
    frame.source.timeInBlend = weightB;

    if (frame.source.camA != camA || frame.source.camB != camB)
    {
        frame.source.customBlender = customBlender_
            ? customBlender_(camA, camB)
            : nullptr;
        frame.source.camA = camA;
        frame.source.camB = camB;
    }
    return overrideId;
}

void CameraBlendStack::releaseCameraOverride(int overrideId)
{
    for (std::size_t index = frames_.size(); index-- > 1;)
    {
        if (frames_[index]->id == overrideId)
        {
            frames_.erase(frames_.begin() + static_cast<std::ptrdiff_t>(index));
            return;
        }
    }
}

float CameraBlendStack::deltaTimeOverride() const
{
    for (std::size_t index = frames_.size(); index-- > 1;)
    {
        if (frames_[index]->active())
            return frames_[index]->deltaTimeOverride;
    }
    return -1.0f;
}

void CameraBlendStack::resetRootFrame()
{
    ensureRootFrame();
    Frame& frame = *frames_.front();
    frame.blend.clearBlend();
    frame.blend.camB = nullptr;
    frame.source.clearBlend();
    frame.source.camB = nullptr;
    frame.clearSnapshot();
    frame.midBlendNormalizedStartPoint = 0.0f;
    currentBlend_.clearBlend();
    currentBlend_.camB = nullptr;
}

void CameraBlendStack::reset()
{
    frames_.clear();
    nextFrameId_ = 0;
    currentBlend_.clearBlend();
    currentBlend_.camB = nullptr;
    resetTransientSources();
}

bool CameraBlendStack::isInitialized() const noexcept
{
    return !frames_.empty();
}

std::size_t CameraBlendStack::overrideCount() const noexcept
{
    return frames_.empty() ? 0 : frames_.size() - 1;
}

float CameraBlendStack::overridePriority(int overrideId) const
{
    for (std::size_t index = 1; index < frames_.size(); ++index)
    {
        if (frames_[index]->id == overrideId)
            return static_cast<float>(frames_[index]->priority);
    }
    return 0.0f;
}

float CameraBlendStack::overrideWeight(int overrideId) const
{
    for (std::size_t index = 1; index < frames_.size(); ++index)
    {
        if (frames_[index]->id == overrideId)
            return frames_[index]->overrideWeight;
    }
    return 0.0f;
}

const Blend& CameraBlendStack::currentBlend() const noexcept
{
    return currentBlend_;
}

Blend& CameraBlendStack::currentBlend() noexcept
{
    return currentBlend_;
}

void CameraBlendStack::ensureRootFrame()
{
    if (frames_.empty())
        frames_.push_back(std::make_unique<Frame>());
}

void CameraBlendStack::resetTransientSources()
{
    ownedSnapshots_.clear();
    ownedNestedBlends_.clear();
    ownedFrozenBlends_.clear();
    ownedBlends_.clear();
}

SnapshotBlendSource* CameraBlendStack::makeSnapshot(
    const ICinemachineCamera* source,
    float remainingTime)
{
    ownedSnapshots_.push_back(
        std::make_unique<SnapshotBlendSource>(source, remainingTime));
    return ownedSnapshots_.back().get();
}

NestedBlendSource* CameraBlendStack::makeNestedBlend(const Blend& source, bool frozen)
{
    ownedBlends_.push_back(std::make_unique<Blend>());
    ownedBlends_.back()->copyFrom(source);
    if (frozen)
    {
        ownedFrozenBlends_.push_back(
            std::make_unique<FrozenBlendSource>(ownedBlends_.back().get()));
        return ownedFrozenBlends_.back().get();
    }

    ownedNestedBlends_.push_back(
        std::make_unique<NestedBlendSource>(ownedBlends_.back().get()));
    return ownedNestedBlends_.back().get();
}

bool CameraBlendStack::advanceBlend(Blend& blend, float deltaTime, bool frozen)
{
    if (blend.camA == nullptr)
        return false;

    if (!frozen)
        blend.timeInBlend += deltaTime >= 0.0f ? deltaTime : blend.duration;

    if (blend.isComplete())
    {
        blend.clearBlend();
        return true;
    }

    auto* nested = dynamic_cast<NestedBlendSource*>(blend.camA);
    if (nested != nullptr && nested->blend() != nullptr)
    {
        advanceBlend(
            *nested->blend(),
            deltaTime,
            dynamic_cast<FrozenBlendSource*>(nested) != nullptr);
    }
    return false;
}

} // namespace cinerig
