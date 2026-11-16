#pragma once

#include <cstddef>
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

    struct Member
    {
        EntityId entity = 0;
        float weight = 1.0f;
        float radius = 0.5f;
    };

    static constexpr std::size_t npos = static_cast<std::size_t>(-1);

    explicit TargetGroup(IEntityWorld& world);

    PositionMode positionMode = PositionMode::GroupCenter;
    RotationMode rotationMode = RotationMode::Manual;
    math::Vec3 manualPosition{};
    math::Quat manualRotation{};

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

    const std::vector<Member>& members() const noexcept { return members_; }

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

    std::vector<ValidMember> validMembers() const;
    void refresh() const;

    IEntityWorld& world_;
    std::vector<Member> members_;
    mutable math::Vec3 groupPosition_{};
    mutable math::Quat groupRotation_{};
    mutable math::Bounds boundingBox_{};
    mutable math::BoundingSphere boundingSphere_{};
    mutable bool hasValidMembers_ = false;
};

} // namespace cinerig::targets
