#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/core/blend_definition.hpp>
#include <cinerig/core/camera_target.hpp>
#include <cinerig/core/output_channels.hpp>
#include <cinerig/engine/camera_output.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/engine/null_adapters.hpp>
#include <cinerig/engine/physics_world.hpp>
#include <cinerig/engine/spline.hpp>
#include <cinerig/impulse/impulse_manager.hpp>
#include <cinerig/runtime/update_manager.hpp>
#include <cinerig/runtime/update_tracker.hpp>
#include <cinerig/runtime/virtual_camera_registry.hpp>

namespace cinerig
{

class CameraBrain;

using BrainId = std::uint64_t;

struct CameraSystemConfig
{
    UpdateMethod updateMethod = UpdateMethod::Smart;
    math::Vec3 worldUp{0.0f, 1.0f, 0.0f};
    OutputChannels channelMask = channelsFromMask(0xFFFFFFFFu);
};

struct LensModeOverrideSettings
{
    bool enabled = false;
    LensSettings::OverrideMode defaultMode = LensSettings::OverrideMode::Perspective;
};

struct BrainConfig
{
    OutputChannels channelMask = channelsFromMask(0xFFFFFFFFu);
    UpdateMethod updateMethod = UpdateMethod::Smart;

    enum class BlendUpdateMethod
    {
        Fixed,
        Late,
    };

    BlendUpdateMethod blendUpdateMethod = BlendUpdateMethod::Late;
    BlendDefinition defaultBlend{BlendStyle::EaseInOut, 2.0f};
    bool ignoreTimeScale = false;
    LensModeOverrideSettings lensModeOverride;
    math::Vec3 worldUpOverride{0.0f, 1.0f, 0.0f};
    bool useWorldUpOverride = false;
};

// Per-world runtime registry. The system owns cameras, brains and target
// groups; all references are non-owning for the duration of the call.
class CameraSystem
{
public:
    explicit CameraSystem(
        IEntityWorld& world,
        IPhysicsWorld* physics = nullptr,
        ISplineProvider* splines = nullptr);
    ~CameraSystem();

    CameraSystem(const CameraSystem&) = delete;
    CameraSystem& operator=(const CameraSystem&) = delete;

    CameraId createCamera(std::unique_ptr<VirtualCamera> camera);

    template <typename T, typename... Args>
    CameraId createCamera(Args&&... args)
    {
        static_assert(std::is_base_of_v<VirtualCamera, T>, "camera type must derive from VirtualCamera");
        return createCamera(std::make_unique<T>(std::forward<Args>(args)...));
    }

    void destroyCamera(CameraId id);
    VirtualCamera* camera(CameraId id);
    const VirtualCamera* camera(CameraId id) const;

    bool setCameraEnabled(CameraId id, bool enabled);
    bool setCameraParent(CameraId childId, CameraId parentId);

    BrainId createBrain(ICameraOutput& output, const BrainConfig& config);
    void destroyBrain(BrainId id);
    CameraBrain* brain(BrainId id);
    const CameraBrain* brain(BrainId id) const;

    GroupId createTargetGroup(std::unique_ptr<TargetGroup> group);
    void destroyTargetGroup(GroupId id);
    TargetGroup* targetGroup(GroupId id);
    const TargetGroup* targetGroup(GroupId id) const;

    void fixedUpdate(float fixedDeltaTime);
    void lateUpdate(float deltaTime);
    void manualUpdate(float deltaTime);
    void updateCameras(
        UpdateClock clock,
        float deltaTime,
        OutputChannels channelMask,
        math::Vec3 worldUp,
        bool smart);

    void setWorldUp(math::Vec3 up);
    math::Vec3 worldUp() const;
    void setUpdateMethod(UpdateMethod method);
    UpdateMethod updateMethod() const;
    void setChannelMask(OutputChannels channels);
    OutputChannels channelMask() const;
    void setUnitTestMode(bool enabled);
    bool unitTestMode() const;

    void resetCameraState();

    std::size_t activeCameraCount() const;
    VirtualCamera* activeCamera(std::size_t index) const;
    std::size_t cameraCount() const;
    std::size_t brainCount() const;
    std::size_t targetGroupCount() const;

    IEntityWorld& entityWorld();
    IPhysicsWorld& physicsWorld();
    ISplineProvider* splineProvider();
    VirtualCameraRegistry& registry();
    const VirtualCameraRegistry& registry() const;
    UpdateManager& updateManager();
    const UpdateManager& updateManager() const;
    UpdateTracker& updateTracker();
    impulse::ImpulseManager& impulseManager();
    const impulse::ImpulseManager& impulseManager() const;

private:
    struct BrainEntry
    {
        ~BrainEntry();

        std::unique_ptr<CameraBrain> brain;
    };

    friend class VirtualCamera;

    VirtualCamera* findCamera(CameraId id);
    EntityId updateTargetFor(const VirtualCamera& camera) const;
    void updateTargetGroups(UpdateClock clock);
    void prepareTracker(UpdateClock clock);
    void update(UpdateClock clock, float deltaTime);
    void prepareBrains(UpdateClock clock);
    void processBrains(UpdateClock clock, float deltaTime);
    void handleCameraEnabled(VirtualCamera& camera, bool enabled);
    void handleCameraParentChanged(VirtualCamera& camera);

    IEntityWorld& world_;
    IPhysicsWorld* physics_ = nullptr;
    NullPhysicsWorld nullPhysics_;
    ISplineProvider* splines_ = nullptr;

    CameraSystemConfig config_;
    VirtualCameraRegistry registry_;
    UpdateTracker updateTracker_;
    UpdateManager updateManager_;
    impulse::ImpulseManager impulseManager_;

    std::unordered_map<CameraId, std::unique_ptr<VirtualCamera>> cameras_;
    std::unordered_map<BrainId, std::unique_ptr<BrainEntry>> brains_;
    std::unordered_map<GroupId, std::unique_ptr<TargetGroup>> targetGroups_;
    CameraId nextCameraId_ = 1;
    BrainId nextBrainId_ = 1;
    GroupId nextTargetGroupId_ = 1;
};

}
