#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <cinerig/core/camera_target.hpp>
#include <cinerig/engine/entity_world.hpp>
#include <cinerig/math/geometry.hpp>

namespace cinerig::targets
{

class TargetGroup : public ::cinerig::TargetGroup
{
public:
    enum class PositionMode
    {
        GroupCenter,
        GroupAverage,
    };

    enum class RotationMode
    {
        Manual,
        GroupAverage,
    };

    enum class GroupUpdateMethod
    {
        Fixed,
        Late,
    };

    struct Member
    {
        EntityId entity = 0;
        float weight = 1.0f;
        float radius = 0.5f;
    };

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    explicit TargetGroup(IEntityWorld& world);

    GroupUpdateMethod updateMethod = GroupUpdateMethod::Late;
    EntityId groupEntity = 0;

    PositionMode positionMode = PositionMode::GroupCenter;
    RotationMode rotationMode = RotationMode::Manual;
    math::Vec3 manualPosition{};
    math::Quat manualRotation{};

    void update(UpdateClock clock) override;
    void doUpdate();
    void addMember(EntityId entity, float weight = 1.0f, float radius = 0.5f);
    bool removeMember(EntityId entity);
    std::size_t findMember(EntityId entity) const;
    void clear();
    void validate();

    bool isValid() const override;
    bool isEmpty() const override;
    std::size_t targetCount() const override;
    math::Vec3 targetPosition(std::size_t index) const override;
    math::Quat targetRotation(std::size_t index) const override;
    float targetWeight(std::size_t index) const override;
    float targetRadius(std::size_t index) const override;

    math::Vec3 groupPosition() const override;
    math::Quat groupRotation() const override;
    math::Bounds boundingBox() const;
    math::BoundingSphere boundingSphere() const;
    math::Bounds viewSpaceBoundingBox(
        math::Vec3 observerPosition,
        math::Quat observerRotation,
        bool includeBehind) const;
    void viewSpaceAngularBounds(
        math::Vec3 observerPosition,
        math::Quat observerRotation,
        math::Vec2& minAngles,
        math::Vec2& maxAngles,
        math::Vec2& zRange) const;

    bool writeBackSucceeded() const noexcept { return writeBackSucceeded_; }

    const std::vector<Member>& members() const noexcept { return members_; }
    std::vector<Member>& members() noexcept
    {
        dirty_ = true;
        return members_;
    }

private:
    struct ValidMember
    {
        std::size_t index = 0;
        EntityId entity = 0;
        math::Vec3 position{};
        math::Quat rotation{};
        float weight = 0.0f;
        float radius = 0.0f;
    };

    UpdateClock selectedClock() const noexcept;
    std::uint64_t currentUpdateFrame() const;
    math::Vec3 fallbackPosition() const;
    math::Quat fallbackRotation() const;
    std::pair<math::Vec3, float> weightedMemberBounds(const ValidMember& member) const;
    void refresh() const;
    void invalidateCache() noexcept { dirty_ = true; }

    IEntityWorld& world_;
    std::vector<Member> members_;
    mutable std::vector<ValidMember> validMembers_;
    mutable math::Vec3 averagePosition_{};
    mutable math::Vec3 groupPosition_{};
    mutable math::Quat groupRotation_{};
    mutable math::Bounds boundingBox_{};
    mutable math::BoundingSphere boundingSphere_{};
    mutable float weightSum_ = 0.0f;
    mutable float maxWeight_ = 0.0f;
    mutable std::uint64_t lastUpdateFrame_ = 0;
    mutable UpdateClock lastUpdateClock_ = UpdateClock::Late;
    mutable bool cacheValid_ = false;
    mutable bool writeBackSucceeded_ = false;
    bool dirty_ = true;
};

} // namespace cinerig::targets
