#include <cinerig/runtime/camera_system.hpp>

#include <algorithm>
#include <utility>

#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/camera/virtual_camera_manager_base.hpp>

namespace cinerig
{

CameraSystem::BrainEntry::~BrainEntry() = default;

CameraSystem::CameraSystem(
    IEntityWorld& world,
    IPhysicsWorld* physics,
    ISplineProvider* splines)
    : world_(world),
      physics_(physics),
      splines_(splines),
      updateTracker_(world),
      updateManager_(registry_, world, updateTracker_),
      impulseManager_(world)
{
}

CameraSystem::~CameraSystem()
{
    brains_.clear();
    for (auto& [id, camera] : cameras_)
    {
        (void)id;
        if (camera)
        {
            camera->valid_ = false;
            camera->system_ = nullptr;
        }
    }
    cameras_.clear();
    targetGroups_.clear();
    registry_.clear();
    updateManager_.reset();
    updateTracker_.reset();
    impulseManager_.clear();
}

CameraId CameraSystem::createCamera(std::unique_ptr<VirtualCamera> camera)
{
    if (!camera)
        return 0;

    const CameraId id = nextCameraId_++;
    camera->cameraId_ = id;
    camera->system_ = this;
    camera->valid_ = true;

    VirtualCamera* raw = camera.get();
    cameras_.emplace(id, std::move(camera));
    if (raw->isEnabled())
        registry_.add(*raw);
    return id;
}

void CameraSystem::destroyCamera(CameraId id)
{
    const auto it = cameras_.find(id);
    if (it == cameras_.end())
        return;

    VirtualCamera* raw = it->second.get();
    for (auto& [cameraId, camera] : cameras_)
    {
        (void)cameraId;
        if (auto* manager = dynamic_cast<VirtualCameraManagerBase*>(camera.get()))
            manager->removeChild(*raw);
    }
    if (auto* manager = dynamic_cast<VirtualCameraManagerBase*>(raw))
        manager->invalidateCameraCache();

    for (auto& [cameraId, camera] : cameras_)
    {
        (void)cameraId;
        if (camera && camera->parentVirtualCamera() == raw)
            camera->parent_ = nullptr;
    }

    for (auto& [brainId, entry] : brains_)
    {
        (void)brainId;
        if (entry && entry->brain)
            entry->brain->onCameraDestroyed(raw);
    }

    registry_.remove(*raw);
    updateManager_.forget(raw);
    raw->valid_ = false;
    raw->system_ = nullptr;
    raw->parent_ = nullptr;
    cameras_.erase(it);
}

VirtualCamera* CameraSystem::camera(CameraId id)
{
    return findCamera(id);
}

const VirtualCamera* CameraSystem::camera(CameraId id) const
{
    const auto it = cameras_.find(id);
    return it == cameras_.end() ? nullptr : it->second.get();
}

VirtualCamera* CameraSystem::findCamera(CameraId id)
{
    const auto it = cameras_.find(id);
    return it == cameras_.end() ? nullptr : it->second.get();
}

bool CameraSystem::setCameraEnabled(CameraId id, bool enabled)
{
    VirtualCamera* cameraPtr = camera(id);
    if (cameraPtr == nullptr)
        return false;
    cameraPtr->setEnabled(enabled);
    return true;
}

bool CameraSystem::setCameraParent(CameraId childId, CameraId parentId)
{
    VirtualCamera* child = camera(childId);
    if (child == nullptr)
        return false;

    VirtualCamera* parent = parentId == 0 ? nullptr : camera(parentId);
    if (parentId != 0 && parent == nullptr)
        return false;
    if (parent == child)
        return false;

    for (VirtualCamera* current = parent; current != nullptr; current = current->parentVirtualCamera())
    {
        if (current == child)
            return false;
    }

    if (auto* oldManager =
            dynamic_cast<VirtualCameraManagerBase*>(child->parentVirtualCamera()))
    {
        oldManager->removeChild(*child);
    }
    else
    {
        child->setParent(nullptr);
    }

    if (auto* manager = dynamic_cast<VirtualCameraManagerBase*>(parent))
        return manager->addChild(*child);

    child->setParent(parent);
    return true;
}

BrainId CameraSystem::createBrain(ICameraOutput& output, const BrainConfig& config)
{
    const BrainId id = nextBrainId_++;
    auto entry = std::make_unique<BrainEntry>();
    entry->brain = std::make_unique<CameraBrain>(*this, output, config);
    brains_.emplace(id, std::move(entry));
    return id;
}

void CameraSystem::destroyBrain(BrainId id)
{
    brains_.erase(id);
}

CameraBrain* CameraSystem::brain(BrainId id)
{
    const auto it = brains_.find(id);
    return it == brains_.end() ? nullptr : it->second->brain.get();
}

const CameraBrain* CameraSystem::brain(BrainId id) const
{
    const auto it = brains_.find(id);
    return it == brains_.end() ? nullptr : it->second->brain.get();
}

GroupId CameraSystem::createTargetGroup(std::unique_ptr<TargetGroup> group)
{
    if (!group)
        return 0;
    const GroupId id = nextTargetGroupId_++;
    targetGroups_.emplace(id, std::move(group));
    return id;
}

void CameraSystem::destroyTargetGroup(GroupId id)
{
    targetGroups_.erase(id);
}

TargetGroup* CameraSystem::targetGroup(GroupId id)
{
    const auto it = targetGroups_.find(id);
    return it == targetGroups_.end() ? nullptr : it->second.get();
}

const TargetGroup* CameraSystem::targetGroup(GroupId id) const
{
    const auto it = targetGroups_.find(id);
    return it == targetGroups_.end() ? nullptr : it->second.get();
}

void CameraSystem::fixedUpdate(float fixedDeltaTime)
{
    update(UpdateClock::Fixed, fixedDeltaTime);
}

void CameraSystem::lateUpdate(float deltaTime)
{
    update(UpdateClock::Late, deltaTime);
}

void CameraSystem::manualUpdate(float deltaTime)
{
    const UpdateClock clock = config_.updateMethod == UpdateMethod::Fixed
        ? UpdateClock::Fixed
        : UpdateClock::Late;
    update(clock, deltaTime);
    for (auto& [id, entry] : brains_)
    {
        (void)id;
        if (entry && entry->brain
            && entry->brain->config().updateMethod == UpdateMethod::Manual)
        {
            entry->brain->processFrame(clock, deltaTime);
        }
    }
}

void CameraSystem::setWorldUp(math::Vec3 up)
{
    config_.worldUp = up;
}

math::Vec3 CameraSystem::worldUp() const
{
    return config_.worldUp;
}

void CameraSystem::setUpdateMethod(UpdateMethod method)
{
    config_.updateMethod = method;
}

UpdateMethod CameraSystem::updateMethod() const
{
    return config_.updateMethod;
}

void CameraSystem::setChannelMask(OutputChannels channels)
{
    config_.channelMask = channels;
}

OutputChannels CameraSystem::channelMask() const
{
    return config_.channelMask;
}

void CameraSystem::setUnitTestMode(bool enabled)
{
    updateManager_.setUnitTestMode(enabled);
}

bool CameraSystem::unitTestMode() const
{
    return updateManager_.unitTestMode();
}

void CameraSystem::resetCameraState()
{
    for (auto& [id, camera] : cameras_)
    {
        (void)id;
        if (camera)
        {
            if (auto* manager = dynamic_cast<VirtualCameraManagerBase*>(camera.get()))
                manager->invalidateCameraCache();
            camera->previousStateIsValid = false;
            camera->updateTargetEntity = 0;
        }
    }
    updateManager_.reset();
    updateTracker_.reset();
    for (auto& [id, entry] : brains_)
    {
        (void)id;
        if (entry && entry->brain)
            entry->brain->resetState();
    }
}

std::size_t CameraSystem::activeCameraCount() const
{
    return registry_.activeCameraCount();
}

VirtualCamera* CameraSystem::activeCamera(std::size_t index) const
{
    return registry_.activeCamera(index);
}

std::size_t CameraSystem::cameraCount() const
{
    return cameras_.size();
}

std::size_t CameraSystem::brainCount() const
{
    return brains_.size();
}

std::size_t CameraSystem::targetGroupCount() const
{
    return targetGroups_.size();
}

IEntityWorld& CameraSystem::entityWorld()
{
    return world_;
}

IPhysicsWorld& CameraSystem::physicsWorld()
{
    return physics_ != nullptr ? *physics_ : nullPhysics_;
}

ISplineProvider* CameraSystem::splineProvider()
{
    return splines_;
}

VirtualCameraRegistry& CameraSystem::registry()
{
    return registry_;
}

const VirtualCameraRegistry& CameraSystem::registry() const
{
    return registry_;
}

UpdateManager& CameraSystem::updateManager()
{
    return updateManager_;
}

const UpdateManager& CameraSystem::updateManager() const
{
    return updateManager_;
}

UpdateTracker& CameraSystem::updateTracker()
{
    return updateTracker_;
}

impulse::ImpulseManager& CameraSystem::impulseManager()
{
    return impulseManager_;
}

const impulse::ImpulseManager& CameraSystem::impulseManager() const
{
    return impulseManager_;
}

EntityId CameraSystem::updateTargetFor(const VirtualCamera& camera) const
{
    TargetRef target = camera.effectiveFollowTarget();
    if (!target.isValid())
        target = camera.effectiveLookAtTarget();

    if (target.kind == TargetRef::Kind::Entity && world_.exists(target.id))
        return target.id;
    if (camera.ownerEntity != 0 && world_.exists(camera.ownerEntity))
        return camera.ownerEntity;
    return 0;
}

void CameraSystem::prepareTracker(UpdateClock clock)
{
    for (auto& [id, camera] : cameras_)
    {
        (void)id;
        if (!camera || !camera->isEnabled())
            continue;
        camera->updateTargetEntity = updateTargetFor(*camera);
        updateTracker_.observe(camera->updateTargetEntity);
    }
    updateTracker_.record(clock);
}

void CameraSystem::update(UpdateClock clock, float deltaTime)
{
    const bool smart = config_.updateMethod == UpdateMethod::Smart;
    updateTargetGroups(clock);
    prepareBrains(clock);
    updateCameras(clock, deltaTime, config_.channelMask, config_.worldUp, smart);
    processBrains(clock, deltaTime);
}

void CameraSystem::updateTargetGroups(UpdateClock clock)
{
    for (auto& [id, group] : targetGroups_)
    {
        (void)id;
        if (group)
            group->update(clock);
    }
}

void CameraSystem::updateCameras(
    UpdateClock clock,
    float deltaTime,
    OutputChannels channelMask,
    math::Vec3 worldUp,
    bool smart)
{
    prepareTracker(clock);
    updateManager_.updateAll(channelMask, worldUp, deltaTime, clock, smart);
}

void CameraSystem::prepareBrains(UpdateClock clock)
{
    for (auto& [id, entry] : brains_)
    {
        (void)id;
        if (!entry || !entry->brain)
            continue;
        if (entry->brain->config().updateMethod == UpdateMethod::Manual)
            continue;

        const bool isFixed = entry->brain->config().blendUpdateMethod
            == BrainConfig::BlendUpdateMethod::Fixed;
        if ((clock == UpdateClock::Fixed) == isFixed)
            entry->brain->prepareLiveCamera();
    }
}

void CameraSystem::processBrains(UpdateClock clock, float deltaTime)
{
    for (auto& [id, entry] : brains_)
    {
        (void)id;
        if (!entry || !entry->brain)
            continue;
        if (entry->brain->config().updateMethod == UpdateMethod::Manual)
            continue;

        const bool isFixed = entry->brain->config().blendUpdateMethod
            == BrainConfig::BlendUpdateMethod::Fixed;
        if ((clock == UpdateClock::Fixed) == isFixed)
            entry->brain->processFrame(clock, deltaTime);
    }
}

void CameraSystem::handleCameraEnabled(VirtualCamera& camera, bool enabled)
{
    registry_.setEnabled(camera, enabled);
    if (!enabled)
        updateManager_.forget(&camera);
}

void CameraSystem::handleCameraParentChanged(VirtualCamera& camera)
{
    registry_.setParent(camera, camera.parentVirtualCamera());
}

}
