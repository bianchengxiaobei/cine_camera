#include <cinerig/targets/target_group.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

#include <cinerig/math/damp.hpp>

namespace cinerig::targets
{

TargetGroup::TargetGroup(IEntityWorld& world)
    : world_(world)
{
}

void TargetGroup::addMember(EntityId entity, float weight, float radius)
{
    members_.push_back({entity, std::max(0.0f, weight), std::max(0.0f, radius)});
}

bool TargetGroup::removeMember(EntityId entity)
{
    const std::size_t index = findMember(entity);
    if (index == npos)
        return false;
    members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

std::size_t TargetGroup::findMember(EntityId entity) const
{
    for (std::size_t index = 0; index < members_.size(); ++index)
    {
        if (members_[index].entity == entity)
            return index;
    }
    return npos;
}

void TargetGroup::clear()
{
    members_.clear();
}

void TargetGroup::validate()
{
    for (Member& member : members_)
    {
        member.weight = std::max(0.0f, member.weight);
        member.radius = std::max(0.0f, member.radius);
    }
    manualRotation = math::normalize(manualRotation);
}

std::vector<TargetGroup::ValidMember> TargetGroup::validMembers() const
{
    std::vector<ValidMember> result;
    result.reserve(members_.size());
    for (std::size_t index = 0; index < members_.size(); ++index)
    {
        const Member& member = members_[index];
        if (member.entity == 0 || member.weight <= math::Epsilon)
            continue;
        if (!world_.exists(member.entity))
            continue;

        const Transform transform = world_.transform(member.entity);
        result.push_back({
            index,
            member.entity,
            transform.position,
            transform.rotation,
            member.weight,
            std::max(0.0f, member.radius),
        });
    }
    return result;
}

void TargetGroup::refresh() const
{
    const std::vector<ValidMember> valid = validMembers();
    hasValidMembers_ = !valid.empty();
    if (!hasValidMembers_)
    {
        groupPosition_ = manualPosition;
        groupRotation_ = manualRotation;
        boundingBox_ = {manualPosition, math::Vec3::zero};
        boundingSphere_ = {manualPosition, 0.0f};
        return;
    }

    float weightSum = 0.0f;
    float maxWeight = 0.0f;
    math::Vec3 averagePosition = math::Vec3::zero;
    for (const ValidMember& member : valid)
    {
        weightSum += member.weight;
        maxWeight = std::max(maxWeight, member.weight);
        averagePosition += member.position * member.weight;
    }
    averagePosition = averagePosition / weightSum;

    math::Vec3 boxMin = averagePosition;
    math::Vec3 boxMax = averagePosition;
    auto weightedMember = [&](const ValidMember& member)
    {
        const float ratio = maxWeight > math::Epsilon
            ? member.weight / maxWeight
            : 0.0f;
        return std::pair{
            math::lerp(averagePosition, member.position, ratio),
            member.radius * ratio,
        };
    };

    for (const ValidMember& member : valid)
    {
        const auto [position, radius] = weightedMember(member);
        const math::Vec3 extent{radius, radius, radius};
        boxMin = math::min(boxMin, position - extent);
        boxMax = math::max(boxMax, position + extent);
    }
    boundingBox_ = {
        (boxMin + boxMax) * 0.5f,
        (boxMax - boxMin) * 0.5f,
    };

    const auto first = weightedMember(valid.front());
    math::BoundingSphere sphere{first.first, first.second};
    for (std::size_t index = 1; index < valid.size(); ++index)
    {
        const auto [position, radius] = weightedMember(valid[index]);
        const float distance = (position - sphere.center).length() + radius;
        if (distance > sphere.radius)
        {
            const float newRadius = (sphere.radius + distance) * 0.5f;
            sphere.center = (sphere.center * newRadius
                + position * (distance - newRadius)) / distance;
            sphere.radius = newRadius;
        }
    }

    boundingSphere_ = sphere;
    groupPosition_ = positionMode == PositionMode::GroupCenter
        ? boundingSphere_.center
        : averagePosition;

    if (rotationMode == RotationMode::Manual)
    {
        groupRotation_ = manualRotation;
        return;
    }

    if (weightSum <= 0.001f || maxWeight <= math::Epsilon)
    {
        groupRotation_ = manualRotation;
        return;
    }

    math::Vec3 averageForward = math::Vec3::zero;
    math::Vec3 averageUp = math::Vec3::zero;
    for (const ValidMember& member : valid)
    {
        const float scale = member.weight / weightSum;
        averageForward += math::rotate(member.rotation, math::Vec3::forward) * scale;
        averageUp += math::rotate(member.rotation, math::Vec3::up) * scale;
    }

    if (averageForward.sqrMagnitude() > 0.0001f
        && averageUp.sqrMagnitude() > 0.0001f)
    {
        groupRotation_ = math::lookRotation(averageForward, averageUp);
    }
    else
    {
        groupRotation_ = manualRotation;
    }
}

bool TargetGroup::isValid() const
{
    return true;
}

bool TargetGroup::isEmpty() const
{
    return validMembers().empty();
}

std::size_t TargetGroup::targetCount() const
{
    return members_.size();
}

math::Vec3 TargetGroup::targetPosition(std::size_t index) const
{
    if (index >= members_.size())
        return {};
    const EntityId entity = members_[index].entity;
    if (entity == 0 || !world_.exists(entity))
        return {};
    return world_.transform(entity).position;
}

math::Quat TargetGroup::targetRotation(std::size_t index) const
{
    if (index >= members_.size())
        return math::Quat::identity();
    const EntityId entity = members_[index].entity;
    if (entity == 0 || !world_.exists(entity))
        return math::Quat::identity();
    return world_.transform(entity).rotation;
}

float TargetGroup::targetWeight(std::size_t index) const
{
    if (index >= members_.size())
        return 0.0f;
    const Member& member = members_[index];
    if (member.entity == 0 || member.weight <= math::Epsilon || !world_.exists(member.entity))
        return 0.0f;
    return member.weight;
}

float TargetGroup::targetRadius(std::size_t index) const
{
    if (index >= members_.size())
        return 0.0f;
    const Member& member = members_[index];
    if (member.entity == 0 || !world_.exists(member.entity))
        return 0.0f;
    return std::max(0.0f, member.radius);
}

math::Vec3 TargetGroup::groupPosition() const
{
    refresh();
    return groupPosition_;
}

math::Quat TargetGroup::groupRotation() const
{
    refresh();
    return groupRotation_;
}

math::Bounds TargetGroup::boundingBox() const
{
    refresh();
    return boundingBox_;
}

math::BoundingSphere TargetGroup::boundingSphere() const
{
    refresh();
    return boundingSphere_;
}

math::Bounds TargetGroup::viewSpaceBoundingBox(
    math::Vec3 observerPosition,
    math::Quat observerRotation,
    bool includeBehind) const
{
    const std::vector<ValidMember> valid = validMembers();
    const math::Quat inverseRotation = math::normalize(math::inverse(observerRotation));
    auto toView = [&](math::Vec3 worldPosition)
    {
        return math::rotate(inverseRotation, worldPosition - observerPosition);
    };

    if (valid.empty())
        return {toView(manualPosition), math::Vec3::zero};

    float weightSum = 0.0f;
    float maxWeight = 0.0f;
    math::Vec3 averagePosition = math::Vec3::zero;
    for (const ValidMember& member : valid)
    {
        weightSum += member.weight;
        maxWeight = std::max(maxWeight, member.weight);
        averagePosition += member.position * member.weight;
    }
    averagePosition = averagePosition / weightSum;

    auto weightedMember = [&](const ValidMember& member)
    {
        const float ratio = maxWeight > math::Epsilon
            ? member.weight / maxWeight
            : 0.0f;
        return std::pair{
            math::lerp(averagePosition, member.position, ratio),
            member.radius * ratio,
        };
    };

    math::Bounds bounds{toView(averagePosition), math::Vec3::zero};
    bool gotOne = false;
    for (const ValidMember& member : valid)
    {
        const auto [position, radius] = weightedMember(member);
        const math::Vec3 viewPosition = toView(position);
        if (viewPosition.z >= 0.0f && !includeBehind)
            continue;

        const math::Vec3 extent{radius, radius, radius};
        if (!gotOne)
        {
            bounds = {viewPosition, extent};
            gotOne = true;
        }
        else
        {
            bounds.encapsulate(viewPosition - extent);
            bounds.encapsulate(viewPosition + extent);
        }
    }
    return bounds;
}

void TargetGroup::viewSpaceAngularBounds(
    math::Vec3 observerPosition,
    math::Quat observerRotation,
    math::Vec2& minAngles,
    math::Vec2& maxAngles,
    math::Vec2& zRange) const
{
    minAngles = {};
    maxAngles = {};
    zRange = {};

    const std::vector<ValidMember> valid = validMembers();
    if (valid.empty())
        return;

    const math::Quat inverseRotation = math::normalize(math::inverse(observerRotation));
    auto toView = [&](math::Vec3 worldPosition)
    {
        return math::rotate(inverseRotation, worldPosition - observerPosition);
    };

    float weightSum = 0.0f;
    float maxWeight = 0.0f;
    math::Vec3 averagePosition = math::Vec3::zero;
    for (const ValidMember& member : valid)
    {
        weightSum += member.weight;
        maxWeight = std::max(maxWeight, member.weight);
        averagePosition += member.position * member.weight;
    }
    averagePosition = averagePosition / weightSum;

    auto weightedMember = [&](const ValidMember& member)
    {
        const float ratio = maxWeight > math::Epsilon
            ? member.weight / maxWeight
            : 0.0f;
        return std::pair{
            math::lerp(averagePosition, member.position, ratio),
            member.radius * ratio,
        };
    };

    const math::BoundingSphere sphere = boundingSphere();
    const float averageDepth = -toView(averagePosition).z;
    zRange = {
        averageDepth - sphere.radius,
        averageDepth + sphere.radius,
    };

    float minTangentX = 0.0f;
    float maxTangentX = 0.0f;
    float minTangentY = 0.0f;
    float maxTangentY = 0.0f;
    bool haveOne = false;

    for (const ValidMember& member : valid)
    {
        const auto [position, radius] = weightedMember(member);
        const math::Vec3 viewPosition = toView(position);
        const float depth = -viewPosition.z;
        if (depth <= math::Epsilon)
            continue;

        const float tangentX = viewPosition.x / depth;
        const float tangentY = viewPosition.y / depth;
        const float angularRadius = std::max(0.0f, radius) / depth;
        if (!haveOne)
        {
            minTangentX = tangentX - angularRadius;
            maxTangentX = tangentX + angularRadius;
            minTangentY = tangentY - angularRadius;
            maxTangentY = tangentY + angularRadius;
            haveOne = true;
        }
        else
        {
            minTangentX = std::min(minTangentX, tangentX - angularRadius);
            maxTangentX = std::max(maxTangentX, tangentX + angularRadius);
            minTangentY = std::min(minTangentY, tangentY - angularRadius);
            maxTangentY = std::max(maxTangentY, tangentY + angularRadius);
        }

        zRange.x = std::min(zRange.x, depth);
        zRange.y = std::max(zRange.y, depth);
    }

    if (!haveOne)
        return;

    minAngles = {
        std::atan(minTangentY) * math::RadToDeg,
        -std::atan(maxTangentX) * math::RadToDeg,
    };
    maxAngles = {
        std::atan(maxTangentY) * math::RadToDeg,
        -std::atan(minTangentX) * math::RadToDeg,
    };
}

} // namespace cinerig::targets


