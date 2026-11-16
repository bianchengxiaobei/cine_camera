#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include <cinerig/math/quat.hpp>
#include <cinerig/math/vec.hpp>

namespace cinerig
{

enum class UpdateClock : std::uint8_t;

class TargetGroup
{
public:
    virtual ~TargetGroup() = default;

    virtual void update(UpdateClock clock)
    {
        (void)clock;
    }

    virtual bool isValid() const = 0;
    virtual bool isEmpty() const = 0;
    virtual std::size_t targetCount() const = 0;
    virtual math::Vec3 targetPosition(std::size_t index) const = 0;
    virtual math::Quat targetRotation(std::size_t index) const = 0;
    virtual float targetWeight(std::size_t index) const = 0;
    virtual float targetRadius(std::size_t index) const = 0;

    virtual math::Vec3 groupPosition() const
    {
        float totalWeight = 0.0f;
        math::Vec3 weightedSum{};
        const std::size_t count = targetCount();
        for (std::size_t index = 0; index < count; ++index)
        {
            const float weight = targetWeight(index);
            if (weight <= 0.0f || !std::isfinite(weight))
                continue;
            weightedSum += targetPosition(index) * weight;
            totalWeight += weight;
        }
        return totalWeight > math::Epsilon ? weightedSum / totalWeight : math::Vec3{};
    }

    virtual math::Quat groupRotation() const
    {
        std::size_t bestIndex = 0;
        float bestWeight = -1.0f;
        bool found = false;
        const std::size_t count = targetCount();
        for (std::size_t index = 0; index < count; ++index)
        {
            const float weight = targetWeight(index);
            if (weight > bestWeight)
            {
                bestWeight = weight;
                bestIndex = index;
                found = true;
            }
        }
        return found ? targetRotation(bestIndex) : math::Quat::identity();
    }
};

struct TargetRef
{
    enum class Kind
    {
        None = 0,
        Entity,
        Camera,
        Group,
    };

    Kind kind = Kind::None;
    std::uint64_t id = 0;

    static constexpr TargetRef entity(std::uint64_t entityId) noexcept
    {
        return TargetRef{Kind::Entity, entityId};
    }

    static constexpr TargetRef camera(std::uint64_t cameraId) noexcept
    {
        return TargetRef{Kind::Camera, cameraId};
    }

    static constexpr TargetRef group(std::uint64_t groupId) noexcept
    {
        return TargetRef{Kind::Group, groupId};
    }

    constexpr bool isValid() const noexcept
    {
        return kind != Kind::None;
    }

    friend constexpr bool operator==(const TargetRef& a, const TargetRef& b) noexcept
    {
        return a.kind == b.kind && a.id == b.id;
    }

    friend constexpr bool operator!=(const TargetRef& a, const TargetRef& b) noexcept
    {
        return !(a == b);
    }
};

struct CameraTarget
{
    TargetRef tracking;
    TargetRef lookAt;
    bool customLookAt = false;

    constexpr TargetRef effectiveLookAt() const noexcept
    {
        return customLookAt ? lookAt : tracking;
    }
};

}


