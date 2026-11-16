#include <cinerig/camera/virtual_camera.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <cinerig/math/damp.hpp>
#include <cinerig/runtime/camera_system.hpp>

namespace cinerig
{

namespace
{

float attachedDamp(
    float initial,
    float dampTime,
    float deltaTime,
    float attachment)
{
    const float clampedAttachment = math::clamp01(attachment);
    const float effectiveDampTime = math::lerp(
        std::max(1.0f, dampTime),
        dampTime,
        clampedAttachment);
    const float effectiveDeltaTime = math::lerp(
        0.0f,
        deltaTime,
        clampedAttachment);
    return math::standardDamp(initial, effectiveDampTime, effectiveDeltaTime);
}

} // namespace

VirtualCamera::VirtualCamera(std::string name)
    : name_(std::move(name))
{
    pipeline_.setOwner(this);
}

VirtualCamera::~VirtualCamera()
{
    for (auto& extension : extensions_)
    {
        if (extension)
            extension->setVirtualCamera(nullptr);
    }
    extensions_.clear();
    pipeline_.clear();
}

std::string_view VirtualCamera::name() const
{
    return name_;
}

const CameraState& VirtualCamera::state() const
{
    return state_;
}

bool VirtualCamera::isValid() const
{
    return valid_;
}

ICinemachineMixer* VirtualCamera::parentCamera() const
{
    return dynamic_cast<ICinemachineMixer*>(parent_);
}

void VirtualCamera::updateCameraState(math::Vec3 worldUp, float deltaTime)
{
    internalUpdateCameraState(worldUp, deltaTime);
}

void VirtualCamera::onCameraActivated(const ActivationEventParams& event)
{
    onTransitionFromCamera(event.outgoingCamera, event.worldUp, event.deltaTime);
}

void VirtualCamera::internalUpdateCameraState(math::Vec3 worldUp, float deltaTime)
{
    updateTargetCache();

    followTargetAttachment = 1.0f;
    lookAtTargetAttachment = 1.0f;
    if (deltaTime < 0.0f)
        previousStateIsValid = false;

    state_ = pullStateFromVirtualCamera(worldUp);
    if (const auto lookAt = lookAtTargetPosition())
        state_.referenceLookAt = *lookAt;
    else
        state_.referenceLookAt = CameraState::kNoPoint;
    state_.blendHint = blendHint;

    invokePrePipelineMutate(state_, deltaTime);
    invokePipeline(state_, deltaTime);
    pushStateToOwner(state_);

    previousStateIsValid = true;
}

void VirtualCamera::onTransitionFromCamera(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    if (!enabled_)
        previousStateIsValid = false;

    invokeOnTransitionExtensions(from, worldUp, deltaTime);

    if (from != nullptr && !isLive() && hasBlendHint(blendHint, BlendHint::InheritPosition))
    {
        const CameraState& fromState = from->state();
        forceCameraPosition(fromState.finalPosition(), fromState.finalOrientation());
    }

    for (int index = 0; index < kStageCount; ++index)
    {
        CameraComponent* component = pipeline_.component(index);
        if (component != nullptr)
            component->onTransitionFromCamera(from, worldUp, deltaTime);
    }

    internalUpdateCameraState(worldUp, deltaTime);
}

void VirtualCamera::forceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    propagateForceCameraPosition(pos, rot);
    state_.rawPosition = pos;
    state_.rawOrientation = rot;
    pushStateToOwner(state_);
}

void VirtualCamera::onTargetObjectWarped(EntityId target, math::Vec3 positionDelta)
{
    const bool warpedFollow = isEntityTarget(effectiveFollowTarget(), target);
    const bool warpedLookAt = isEntityTarget(effectiveLookAtTarget(), target);

    if (warpedFollow)
    {
        state_.rawPosition += positionDelta;
        pushStateToOwner(state_);
    }
    if (warpedLookAt && state_.hasLookAt())
        state_.referenceLookAt += positionDelta;

    propagateTargetWarped(target, positionDelta);
}

float VirtualCamera::detachedFollowTargetDamp(
    float initial,
    float dampTime,
    float deltaTime) const
{
    return attachedDamp(initial, dampTime, deltaTime, followTargetAttachment);
}

math::Vec3 VirtualCamera::detachedFollowTargetDamp(
    math::Vec3 initial,
    float dampTime,
    float deltaTime) const
{
    return {
        attachedDamp(initial.x, dampTime, deltaTime, followTargetAttachment),
        attachedDamp(initial.y, dampTime, deltaTime, followTargetAttachment),
        attachedDamp(initial.z, dampTime, deltaTime, followTargetAttachment),
    };
}

math::Vec3 VirtualCamera::detachedFollowTargetDamp(
    math::Vec3 initial,
    math::Vec3 dampTime,
    float deltaTime) const
{
    return {
        attachedDamp(initial.x, dampTime.x, deltaTime, followTargetAttachment),
        attachedDamp(initial.y, dampTime.y, deltaTime, followTargetAttachment),
        attachedDamp(initial.z, dampTime.z, deltaTime, followTargetAttachment),
    };
}

float VirtualCamera::detachedLookAtTargetDamp(
    float initial,
    float dampTime,
    float deltaTime) const
{
    return attachedDamp(initial, dampTime, deltaTime, lookAtTargetAttachment);
}

math::Vec3 VirtualCamera::detachedLookAtTargetDamp(
    math::Vec3 initial,
    float dampTime,
    float deltaTime) const
{
    return {
        attachedDamp(initial.x, dampTime, deltaTime, lookAtTargetAttachment),
        attachedDamp(initial.y, dampTime, deltaTime, lookAtTargetAttachment),
        attachedDamp(initial.z, dampTime, deltaTime, lookAtTargetAttachment),
    };
}

math::Vec3 VirtualCamera::detachedLookAtTargetDamp(
    math::Vec3 initial,
    math::Vec3 dampTime,
    float deltaTime) const
{
    return {
        attachedDamp(initial.x, dampTime.x, deltaTime, lookAtTargetAttachment),
        attachedDamp(initial.y, dampTime.y, deltaTime, lookAtTargetAttachment),
        attachedDamp(initial.z, dampTime.z, deltaTime, lookAtTargetAttachment),
    };
}

float VirtualCamera::maxDampTime() const
{
    float maxValue = 0.0f;
    for (int index = 0; index < kStageCount; ++index)
    {
        const CameraComponent* component = pipeline_.component(index);
        if (component != nullptr)
            maxValue = std::max(maxValue, component->maxDampTime());
    }
    for (const auto& extension : extensions_)
    {
        if (extension && extension->isEnabled())
            maxValue = std::max(maxValue, extension->maxDampTime());
    }
    return maxValue;
}

bool VirtualCamera::isEnabled() const noexcept
{
    return enabled_;
}

void VirtualCamera::setEnabled(bool enabled)
{
    if (enabled_ == enabled)
        return;
    enabled_ = enabled;
    if (system_ != nullptr)
        system_->handleCameraEnabled(*this, enabled);
}

void VirtualCamera::setLive(bool live) noexcept
{
    live_ = live;
}

bool VirtualCamera::isLive() const noexcept
{
    return live_;
}

float VirtualCamera::currentTime() const noexcept
{
    return system_ == nullptr ? 0.0f : system_->entityWorld().time();
}

const IPhysicsWorld* VirtualCamera::physicsWorld() const noexcept
{
    return system_ == nullptr ? nullptr : &system_->physicsWorld();
}

VirtualCamera* VirtualCamera::parentVirtualCamera() const noexcept
{
    return parent_;
}

void VirtualCamera::setParent(VirtualCamera* parent)
{
    if (parent_ == parent || parent == this)
        return;
    for (VirtualCamera* ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent_)
    {
        if (ancestor == this)
            return;
    }
    parent_ = parent;
    if (system_ != nullptr)
        system_->handleCameraParentChanged(*this);
}

int VirtualCamera::activationId() const noexcept
{
    return activationId_;
}

CameraId VirtualCamera::cameraId() const noexcept
{
    return cameraId_;
}

TargetRef VirtualCamera::effectiveFollowTarget() const
{
    if (targets.tracking.isValid())
        return targets.tracking;
    return parent_ != nullptr ? parent_->effectiveFollowTarget() : TargetRef{};
}

TargetRef VirtualCamera::effectiveLookAtTarget() const
{
    const TargetRef local = targets.effectiveLookAt();
    if (local.isValid())
        return local;
    return parent_ != nullptr ? parent_->effectiveLookAtTarget() : TargetRef{};
}

const TargetGroup* VirtualCamera::lookAtTargetAsGroup() const
{
    const TargetRef target = effectiveLookAtTarget();
    if (system_ == nullptr || target.kind != TargetRef::Kind::Group)
        return nullptr;
    return system_->targetGroup(target.id);
}

const TargetGroup* VirtualCamera::followTargetAsGroup() const
{
    const TargetRef target = effectiveFollowTarget();
    if (system_ == nullptr || target.kind != TargetRef::Kind::Group)
        return nullptr;
    return system_->targetGroup(target.id);
}

bool VirtualCamera::isEntityTarget(TargetRef target, EntityId entity) const
{
    return target.kind == TargetRef::Kind::Entity && target.id == entity;
}

std::optional<math::Vec3> VirtualCamera::targetPosition(TargetRef target) const
{
    if (!target.isValid() || system_ == nullptr)
        return std::nullopt;

    switch (target.kind)
    {
    case TargetRef::Kind::Entity:
    {
        const IEntityWorld* world = entityWorld();
        if (world == nullptr || !world->exists(target.id))
            return std::nullopt;
        return world->transform(target.id).position;
    }
    case TargetRef::Kind::Camera:
    {
        const VirtualCamera* camera = system_->camera(target.id);
        if (camera == nullptr)
            return std::nullopt;
        return camera->state().finalPosition();
    }
    case TargetRef::Kind::Group:
    {
        const TargetGroup* group = system_->targetGroup(target.id);
        return group != nullptr ? groupPosition(*group) : std::nullopt;
    }
    case TargetRef::Kind::None:
        break;
    }
    return std::nullopt;
}

std::optional<math::Quat> VirtualCamera::targetRotation(TargetRef target) const
{
    if (!target.isValid() || system_ == nullptr)
        return std::nullopt;

    switch (target.kind)
    {
    case TargetRef::Kind::Entity:
    {
        const IEntityWorld* world = entityWorld();
        if (world == nullptr || !world->exists(target.id))
            return std::nullopt;
        return world->transform(target.id).rotation;
    }
    case TargetRef::Kind::Camera:
    {
        const VirtualCamera* camera = system_->camera(target.id);
        if (camera == nullptr)
            return std::nullopt;
        return camera->state().finalOrientation();
    }
    case TargetRef::Kind::Group:
    {
        const TargetGroup* group = system_->targetGroup(target.id);
        return group != nullptr ? groupRotation(*group) : std::nullopt;
    }
    case TargetRef::Kind::None:
        break;
    }
    return std::nullopt;
}

std::optional<math::Vec3> VirtualCamera::followTargetPosition() const
{
    return targetPosition(effectiveFollowTarget());
}

std::optional<math::Quat> VirtualCamera::followTargetRotation() const
{
    return targetRotation(effectiveFollowTarget());
}

std::optional<math::Vec3> VirtualCamera::lookAtTargetPosition() const
{
    return targetPosition(effectiveLookAtTarget());
}

std::optional<math::Quat> VirtualCamera::lookAtTargetRotation() const
{
    return targetRotation(effectiveLookAtTarget());
}

CameraComponent* VirtualCamera::component(Stage stage) const noexcept
{
    return pipeline_.component(stage);
}

Pipeline& VirtualCamera::pipeline() noexcept
{
    return pipeline_;
}

const Pipeline& VirtualCamera::pipeline() const noexcept
{
    return pipeline_;
}

void VirtualCamera::getInputAxisOwners(std::vector<IInputAxisOwner*>& owners) const
{
    for (int index = 0; index < kStageCount; ++index)
    {
        auto* owner = dynamic_cast<IInputAxisOwner*>(pipeline_.component(index));
        if (owner != nullptr)
            owners.push_back(owner);
    }
}

void VirtualCamera::addExtension(std::unique_ptr<CameraExtension> extension)
{
    if (!extension)
        return;
    extension->setVirtualCamera(this);
    extensions_.push_back(std::move(extension));
}

void VirtualCamera::removeExtension(CameraExtension* extension)
{
    const auto it = std::find_if(
        extensions_.begin(),
        extensions_.end(),
        [extension](const std::unique_ptr<CameraExtension>& owned)
        {
            return owned.get() == extension;
        });
    if (it == extensions_.end())
        return;
    (*it)->setVirtualCamera(nullptr);
    extensions_.erase(it);
}

std::size_t VirtualCamera::extensionCount() const noexcept
{
    return extensions_.size();
}

CameraExtension* VirtualCamera::extension(std::size_t index) const noexcept
{
    return index < extensions_.size() ? extensions_[index].get() : nullptr;
}

void VirtualCamera::updateTargetCache()
{
}

void VirtualCamera::invokePrePipelineMutate(CameraState& state, float deltaTime)
{
    invokePrePipelineMutateFor(*this, state, deltaTime);
}

void VirtualCamera::invokePostPipelineStage(Stage stage, CameraState& state, float deltaTime)
{
    invokePostPipelineStageFor(*this, stage, state, deltaTime);
}

CameraState VirtualCamera::pullStateFromVirtualCamera(math::Vec3 worldUp)
{
    CameraState state = CameraState::makeDefault();
    state.lens = lens;
    state.referenceUp = worldUp;

    const IEntityWorld* world = entityWorld();
    if (world != nullptr && ownerEntity != 0 && world->exists(ownerEntity))
    {
        const Transform transform = world->transform(ownerEntity);
        state.rawPosition = transform.position;
        state.rawOrientation = transform.rotation;
    }
    return state;
}

void VirtualCamera::pushStateToOwner(const CameraState& state)
{
    IEntityWorld* world = entityWorld();
    if (world == nullptr || ownerEntity == 0 || !world->exists(ownerEntity))
        return;

    Transform transform = world->transform(ownerEntity);
    transform.position = state.rawPosition;
    transform.rotation = state.rawOrientation;
    world->trySetTransform(ownerEntity, transform);
}

void VirtualCamera::invokePipeline(CameraState& state, float deltaTime)
{
    pipeline_.prePipelineMutate(state, deltaTime);

    CameraComponent* postAimBody = nullptr;
    for (int index = 0; index < kStageCount; ++index)
    {
        const auto stage = static_cast<Stage>(index);
        CameraComponent* component = pipeline_.component(index);

        if (component != nullptr && component->isValid())
        {
            if (stage == Stage::Body && component->bodyAppliesAfterAim())
                postAimBody = component;
            else
                component->mutateCameraState(state, deltaTime);
        }

        invokePostPipelineStage(stage, state, deltaTime);

        if (stage == Stage::Aim && postAimBody != nullptr)
        {
            postAimBody->mutateCameraState(state, deltaTime);
            invokePostPipelineStage(Stage::Body, state, deltaTime);
        }
    }
}

void VirtualCamera::invokePrePipelineMutateFor(
    VirtualCamera& vcam,
    CameraState& state,
    float deltaTime)
{
    for (auto& extension : extensions_)
    {
        if (extension && extension->isEnabled())
            extension->prePipelineMutateCameraStateCallback(vcam, state, deltaTime);
    }
    if (parent_ != nullptr)
        parent_->invokePrePipelineMutateFor(vcam, state, deltaTime);
}

void VirtualCamera::invokePostPipelineStageFor(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    for (auto& extension : extensions_)
    {
        if (extension && extension->isEnabled())
            extension->postPipelineStageCallback(vcam, stage, state, deltaTime);
    }
    if (parent_ != nullptr)
        parent_->invokePostPipelineStageFor(vcam, stage, state, deltaTime);
}

bool VirtualCamera::invokeOnTransitionExtensions(
    const ICinemachineCamera* from,
    math::Vec3 worldUp,
    float deltaTime)
{
    bool forceUpdate = false;
    for (auto& extension : extensions_)
    {
        if (extension && extension->isEnabled()
            && extension->onTransitionFromCamera(from, worldUp, deltaTime))
        {
            forceUpdate = true;
        }
    }
    return forceUpdate;
}

void VirtualCamera::propagateForceCameraPosition(math::Vec3 pos, math::Quat rot)
{
    for (int index = 0; index < kStageCount; ++index)
    {
        CameraComponent* component = pipeline_.component(index);
        if (component != nullptr)
            component->forceCameraPosition(pos, rot);
    }
    for (auto& extension : extensions_)
    {
        if (extension && extension->isEnabled())
            extension->forceCameraPosition(pos, rot);
    }
    for (VirtualCamera* ancestor = parent_; ancestor != nullptr; ancestor = ancestor->parent_)
    {
        for (auto& extension : ancestor->extensions_)
        {
            if (extension && extension->isEnabled())
                extension->forceCameraPosition(pos, rot);
        }
    }
}

void VirtualCamera::propagateTargetWarped(EntityId target, math::Vec3 positionDelta)
{
    for (int index = 0; index < kStageCount; ++index)
    {
        CameraComponent* component = pipeline_.component(index);
        if (component != nullptr)
            component->onTargetObjectWarped(target, positionDelta);
    }
    for (auto& extension : extensions_)
    {
        if (extension && extension->isEnabled())
            extension->onTargetObjectWarped(target, positionDelta);
    }
    for (VirtualCamera* ancestor = parent_; ancestor != nullptr; ancestor = ancestor->parent_)
    {
        for (auto& extension : ancestor->extensions_)
        {
            if (extension && extension->isEnabled())
                extension->onTargetObjectWarped(target, positionDelta);
        }
    }
}

std::optional<math::Vec3> VirtualCamera::groupPosition(const TargetGroup& group) const
{
    if (!group.isValid() || group.isEmpty())
        return std::nullopt;
    return group.groupPosition();
}

std::optional<math::Quat> VirtualCamera::groupRotation(const TargetGroup& group) const
{
    if (!group.isValid() || group.isEmpty())
        return std::nullopt;
    return group.groupRotation();
}

IEntityWorld* VirtualCamera::entityWorld() const noexcept
{
    return system_ != nullptr ? &system_->entityWorld() : nullptr;
}

} // namespace cinerig



