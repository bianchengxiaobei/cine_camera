#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cinerig/brain/brain_events.hpp>
#include <cinerig/core/blend.hpp>
#include <cinerig/core/camera_interface.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

class SnapshotBlendSource final : public ICinemachineCamera
{
public:
    explicit SnapshotBlendSource(
        const ICinemachineCamera* source = nullptr,
        float remainingTimeInBlend = 0.0f);

    void takeSnapshot(const ICinemachineCamera* source);

    std::string_view name() const override;
    const CameraState& state() const override;
    bool isValid() const override;
    ICinemachineMixer* parentCamera() const override;

    float remainingTimeInBlend = 0.0f;

private:
    CameraState state_ = CameraState::makeDefault();
    std::string name_ = "(null)";
};

class FrozenBlendSource final : public NestedBlendSource
{
public:
    explicit FrozenBlendSource(Blend* blend = nullptr);
};

class CameraBlendStack
{
public:
    using LookupBlendFunction = std::function<BlendDefinition(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming)>;
    using CustomBlenderFunction = std::function<IBlender*(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming)>;

    CameraBlendStack();
    virtual ~CameraBlendStack() = default;

    CameraBlendStack(const CameraBlendStack&) = delete;
    CameraBlendStack& operator=(const CameraBlendStack&) = delete;

    void setEvents(BrainEvents* events) noexcept;

    void setLookupBlendFunction(LookupBlendFunction lookup);
    void setLookupBlend(LookupBlendFunction lookup) { setLookupBlendFunction(std::move(lookup)); }
    void setCustomBlenderFunction(CustomBlenderFunction customBlender);
    void setCustomBlender(CustomBlenderFunction customBlender)
    {
        setCustomBlenderFunction(std::move(customBlender));
    }

    void updateRootFrame(
        ICinemachineMixer* context,
        ICinemachineCamera* activeCamera,
        math::Vec3 worldUp,
        float deltaTime);

    void computeCurrentBlend();

    int setCameraOverride(
        int overrideId,
        int priority,
        ICinemachineCamera* camA,
        ICinemachineCamera* camB,
        float weightB,
        float deltaTime);

    void releaseCameraOverride(int overrideId);

    float deltaTimeOverride() const;
    float getDeltaTimeOverride() const { return deltaTimeOverride(); }

    void resetRootFrame();
    void reset();

    bool isInitialized() const noexcept;
    std::size_t overrideCount() const noexcept;
    float overridePriority(int overrideId) const;
    float overrideWeight(int overrideId) const;

    const Blend& currentBlend() const noexcept;
    Blend& currentBlend() noexcept;

protected:
    BrainEvents* events_ = nullptr;

private:
    struct Frame
    {
        int id = 0;
        int priority = 0;
        float deltaTimeOverride = -1.0f;
        Blend source;
        Blend blend;
        NestedBlendSource self{&blend};
        SnapshotBlendSource snapshot;
        ICinemachineCamera* snapshotSource = nullptr;
        float snapshotBlendWeight = 0.0f;
        float midBlendNormalizedStartPoint = 0.0f;
        float overrideWeight = 0.0f;

        bool active() const { return source.isValid(); }
        ICinemachineCamera* getSnapshotIfAppropriate(ICinemachineCamera* camera, float weight);
        void clearSnapshot();
    };

    void ensureRootFrame();
    void resetTransientSources();
    SnapshotBlendSource* makeSnapshot(const ICinemachineCamera* source, float remainingTime);
    NestedBlendSource* makeNestedBlend(const Blend& source, bool frozen);
    static bool advanceBlend(Blend& blend, float deltaTime, bool frozen);

    std::vector<std::unique_ptr<Frame>> frames_;
    int nextFrameId_ = 0;
    Blend currentBlend_;
    LookupBlendFunction lookupBlend_;
    CustomBlenderFunction customBlender_;

    std::vector<std::unique_ptr<Blend>> ownedBlends_;
    std::vector<std::unique_ptr<NestedBlendSource>> ownedNestedBlends_;
    std::vector<std::unique_ptr<FrozenBlendSource>> ownedFrozenBlends_;
    std::vector<std::unique_ptr<SnapshotBlendSource>> ownedSnapshots_;
};

} // namespace cinerig
