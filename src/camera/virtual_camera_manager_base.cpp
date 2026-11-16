#include <cinerig/camera/virtual_camera_manager_base.hpp>

#include <algorithm>
#include <utility>

namespace cinerig
{

VirtualCameraManagerBase::VirtualCameraManagerBase(std::string name)
    : VirtualCamera(std::move(name))
{
    blendManager_.setLookupBlendFunction(
        [this](const ICinemachineCamera* outgoing, const ICinemachineCamera* incoming)
        {
            return lookupBlend(outgoing, incoming);
        });
}

VirtualCameraManagerBase::~VirtualCameraManagerBase() = default;

bool VirtualCameraManagerBase::addChild(VirtualCamera& child)
{
    if (&child == this || containsChild(&child))
        return false;

    VirtualCamera* currentParent = child.parentVirtualCamera();
    if (currentParent != nullptr && currentParent != this)
        return false;
    if (wouldCreateCycle(child))
        return false;

    child.setParent(this);
    childCameras_.push_back(&child);
    invalidateCameraCache();
    return true;
}

bool VirtualCameraManagerBase::removeChild(VirtualCamera& child)
{
    const auto it = std::find(childCameras_.begin(), childCameras_.end(), &child);
    if (it == childCameras_.end())
        return false;

    if (child.parentVirtualCamera() == this)
        child.setParent(nullptr);
    childCameras_.erase(it);
    invalidateCameraCache();
    return true;
}

const std::vector<VirtualCamera*>& VirtualCameraManagerBase::childCameras() const noexcept
{
    return childCameras_;
}

VirtualCamera* VirtualCameraManagerBase::liveChild() const
{
    if (!previousStateIsValid)
        return nullptr;
    return dynamic_cast<VirtualCamera*>(blendManager_.activeVirtualCamera());
}

bool VirtualCameraManagerBase::isBlending() const
{
    return blendManager_.isBlending();
}

const Blend* VirtualCameraManagerBase::activeBlend() const
{
    return previousStateIsValid ? blendManager_.activeBlend() : nullptr;
}

void VirtualCameraManagerBase::invalidateCameraCache()
{
    cacheValid_ = false;
    previousStateIsValid = false;
    propagatePreviousStateInvalid();
    blendManager_.reset();
}

void VirtualCameraManagerBase::internalUpdateCameraState(math::Vec3 worldUp, float deltaTime)
{
    updateTargetCache();
    updateCameraCache();

    if (deltaTime < 0.0f)
        previousStateIsValid = false;
    if (!previousStateIsValid)
    {
        propagatePreviousStateInvalid();
        resetLiveChild();
    }
    else if (auto* current =
                 dynamic_cast<VirtualCamera*>(blendManager_.activeVirtualCamera());
             current != nullptr && !current->previousStateIsValid)
    {
        resetLiveChild();
    }

    VirtualCamera* best = chooseCurrentCamera(worldUp, deltaTime);
    if (best != nullptr && !containsChild(best))
        best = nullptr;

    if (best != nullptr && !best->isEnabled())
    {
        best->previousStateIsValid = false;
        best->setEnabled(true);
        best->updateCameraState(worldUp, deltaTime);
    }

    setLiveChild(best, worldUp, deltaTime);

    if (transitioningFrom_ != nullptr && !isBlending())
    {
        VirtualCamera* incoming =
            dynamic_cast<VirtualCamera*>(blendManager_.activeVirtualCamera());
        if (incoming != nullptr)
        {
            ActivationEventParams event;
            event.origin = this;
            event.outgoingCamera = const_cast<ICinemachineCamera*>(transitioningFrom_);
            event.incomingCamera = incoming;
            event.isCut = false;
            event.worldUp = worldUp;
            event.deltaTime = deltaTime;
            incoming->onCameraActivated(event);
        }
    }

    finalizeCameraState(deltaTime);
    transitioningFrom_ = nullptr;
    previousStateIsValid = true;
}

void VirtualCameraManagerBase::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    transitioningFrom_ = from;
    VirtualCamera::onTransitionFromCamera(from, worldUp, deltaTime);
    transitioningFrom_ = nullptr;
}

void VirtualCameraManagerBase::onTargetObjectWarped(
    EntityId target,
    math::Vec3 positionDelta)
{
    updateCameraCache();
    for (VirtualCamera* child : childCameras_)
    {
        if (child != nullptr)
            child->onTargetObjectWarped(target, positionDelta);
    }
    VirtualCamera::onTargetObjectWarped(target, positionDelta);
}

void VirtualCameraManagerBase::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    updateCameraCache();
    for (VirtualCamera* child : childCameras_)
    {
        if (child == nullptr)
            continue;
        child->forceCameraPosition(pos, rot);
        child->previousStateIsValid = true;
    }
    VirtualCamera::forceCameraPosition(pos, rot);
    previousStateIsValid = true;
}

void VirtualCameraManagerBase::onCameraActivated(const ActivationEventParams& event)
{
    if (event.incomingCamera == this)
        VirtualCamera::onCameraActivated(event);
}

TargetRef VirtualCameraManagerBase::effectiveFollowTarget() const
{
    if (defaultTarget.enabled && defaultTarget.target.tracking.isValid())
        return defaultTarget.target.tracking;
    if (targets.tracking.isValid())
        return targets.tracking;
    const VirtualCamera* parent = parentVirtualCamera();
    return parent != nullptr ? parent->effectiveFollowTarget() : TargetRef{};
}

TargetRef VirtualCameraManagerBase::effectiveLookAtTarget() const
{
    if (defaultTarget.enabled)
    {
        const TargetRef local = defaultTarget.target.effectiveLookAt();
        if (local.isValid())
            return local;
    }
    const TargetRef local = targets.effectiveLookAt();
    if (local.isValid())
        return local;
    const VirtualCamera* parent = parentVirtualCamera();
    return parent != nullptr ? parent->effectiveLookAtTarget() : TargetRef{};
}

bool VirtualCameraManagerBase::isLiveChild(
    const ICinemachineCamera* child,
    bool dominantChildOnly) const
{
    if (child == nullptr)
        return false;

    if (blendManager_.isLive(child))
    {
        if (!dominantChildOnly)
            return true;
        return blendManager_.activeVirtualCamera() == child;
    }

    for (const VirtualCamera* camera : childCameras_)
    {
        if (camera == nullptr || !blendManager_.isLive(camera))
            continue;
        if (dominantChildOnly && blendManager_.activeVirtualCamera() != camera)
            continue;
        const auto* mixer = dynamic_cast<const ICinemachineMixer*>(camera);
        if (mixer != nullptr && mixer->isLiveChild(child, dominantChildOnly))
            return true;
    }
    return false;
}

BlendDefinition VirtualCameraManagerBase::lookupBlend(
    const ICinemachineCamera* outgoing,
    const ICinemachineCamera* incoming) const
{
    BlendLookupTable lookup = customBlends;
    lookup.setDefaultBlend(defaultBlend);
    return lookup.lookup(outgoing, incoming);
}

void VirtualCameraManagerBase::setLiveChild(
    VirtualCamera* camera,
    math::Vec3 worldUp,
    float deltaTime)
{
    blendManager_.updateRootFrame(this, camera, worldUp, deltaTime);
    blendManager_.computeCurrentBlend();
    blendManager_.processActiveCamera(this, worldUp, deltaTime);
}

void VirtualCameraManagerBase::resetLiveChild()
{
    blendManager_.resetRootFrame();
}

void VirtualCameraManagerBase::finalizeCameraState(float deltaTime)
{
    state_ = blendManager_.cameraState();
    invokePostPipelineStage(Stage::Finalize, state_, deltaTime);
    pushStateToOwner(state_);
}

void VirtualCameraManagerBase::updateCameraCache()
{
    if (cacheValid_)
        return;
    cacheValid_ = true;
    pruneDetachedChildren();
}

bool VirtualCameraManagerBase::wouldCreateCycle(const VirtualCamera& child) const
{
    if (&child == this)
        return true;
    for (const VirtualCamera* ancestor = this; ancestor != nullptr;
         ancestor = ancestor->parentVirtualCamera())
    {
        if (ancestor == &child)
            return true;
    }
    return false;
}

bool VirtualCameraManagerBase::containsChild(const VirtualCamera* child) const noexcept
{
    return std::find(childCameras_.begin(), childCameras_.end(), child)
        != childCameras_.end();
}

void VirtualCameraManagerBase::propagatePreviousStateInvalid()
{
    for (VirtualCamera* child : childCameras_)
    {
        if (child == nullptr)
            continue;
        if (auto* childManager = dynamic_cast<VirtualCameraManagerBase*>(child))
            childManager->invalidateCameraCache();
        else
            child->previousStateIsValid = false;
    }
}

void VirtualCameraManagerBase::pruneDetachedChildren()
{
    const auto previousSize = childCameras_.size();
    childCameras_.erase(
        std::remove_if(
            childCameras_.begin(),
            childCameras_.end(),
            [this](const VirtualCamera* child)
            {
                return child == nullptr || child->parentVirtualCamera() != this;
            }),
        childCameras_.end());
    if (childCameras_.size() != previousSize)
    {
        previousStateIsValid = false;
        propagatePreviousStateInvalid();
        blendManager_.reset();
    }
}

} // namespace cinerig
