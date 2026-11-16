#include <cinerig/targets/target_group.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace cinerig::targets
{

TargetGroup::TargetGroup(IEntityWorld& world)
    : world_(world)
{
}

UpdateClock TargetGroup::selectedClock() const noexcept
{
    return updateMethod == GroupUpdateMethod::Fixed
        ? UpdateClock::Fixed
        : UpdateClock::Late;
}

std::uint64_t TargetGroup::currentUpdateFrame() const
{
    return world_.frame(selectedClock());
}

math::Vec3 TargetGroup::fallbackPosition() const
{
    if (groupEntity != 0 && world_.exists(groupEntity))
        return world_.transform(groupEntity).position;
    return manualPosition;
}

math::Quat TargetGroup::fallbackRotation() const
{
    if (groupEntity != 0 && world_.exists(groupEntity))
        return world_.transform(groupEntity).rotation;
    return manualRotation;
}

void TargetGroup::addMember(EntityId entity, float weight, float radius)
{
    members_.push_back({entity, std::max(0.0f, weight), std::max(0.0f, radius)});
    invalidateCache();
}

bool TargetGroup::removeMember(EntityId entity)
{
    const std::size_t index = findMember(entity);
    if (index == npos)
        return false;
    members_.erase(members_.begin() + static_cast<std::ptrdiff_t>(index));
    invalidateCache();
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
    invalidateCache();
}

void TargetGroup::validate()
{
    for (Member& member : members_)
    {
        member.weight = std::max(0.0f, member.weight);
        member.radius = std::max(0.0f, member.radius);
    }
    manualRotation = math::normalize(manualRotation);
    invalidateCache();
}

void TargetGroup::update(UpdateClock clock)
{
    if (clock != selectedClock())
        return;
    if (cacheValid_ && !dirty_ && lastUpdateClock_ == clock
        && lastUpdateFrame_ == currentUpdateFrame())
    {
        return;
    }
    doUpdate();
}

void TargetGroup::doUpdate()
{
    validMembers_.clear();
    validMembers_.reserve(members_.size());
    weightSum_ = 0.0f;
    maxWeight_ = 0.0f;

    for (std::size_t index = 0; index < members_.size(); ++index)
    {
        const Member& member = members_[index];
        if (member.entity == 0 || member.weight <= math::Epsilon)
            continue;
        if (!world_.exists(member.entity))
            continue;

        const Transform transform = world_.transform(member.entity);
        validMembers_.push_back({
            index,
            member.entity,
            transform.position,
            transform.rotation,
            member.weight,
            std::max(0.0f, member.radius),
        });
        weightSum_ += member.weight;
        maxWeight_ = std::max(maxWeight_, member.weight);
    }

    const math::Vec3 fallbackPosition = this->fallbackPosition();
    const math::Quat fallbackRotation = this->fallbackRotation();

    if (validMembers_.empty())
    {
        averagePosition_ = fallbackPosition;
        groupPosition_ = fallbackPosition;
        groupRotation_ = fallbackRotation;
        boundingBox_ = {fallbackPosition, math::Vec3::zero};
        boundingSphere_ = {fallbackPosition, 0.0f};
    }
    else
    {
        averagePosition_ = math::Vec3::zero;
        for (const ValidMember& member : validMembers_)
            averagePosition_ += member.position * member.weight;
        averagePosition_ = averagePosition_ / weightSum_;

        math::Vec3 boxMin = averagePosition_;
        math::Vec3 boxMax = averagePosition_;
        for (const ValidMember& member : validMembers_)
        {
            const auto [position, radius] = weightedMemberBounds(member);
            const math::Vec3 extent{radius, radius, radius};
            boxMin = math::min(boxMin, position - extent);
            boxMax = math::max(boxMax, position + extent);
        }
        boundingBox_ = {
            (boxMin + boxMax) * 0.5f,
            (boxMax - boxMin) * 0.5f,
        };

        const auto first = weightedMemberBounds(validMembers_.front());
        math::BoundingSphere sphere{first.first, first.second};
        for (std::size_t index = 1; index < validMembers_.size(); ++index)
        {
            const auto [position, radius] = weightedMemberBounds(validMembers_[index]);
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
            : averagePosition_;

        if (rotationMode == RotationMode::Manual)
        {
            groupRotation_ = fallbackRotation;
        }
        else if (weightSum_ <= 0.001f || maxWeight_ <= math::Epsilon)
        {
            groupRotation_ = fallbackRotation;
        }
        else
        {
            math::Vec3 averageForward = math::Vec3::zero;
            math::Vec3 averageUp = math::Vec3::zero;
            for (const ValidMember& member : validMembers_)
            {
                const float scale = member.weight / weightSum_;
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
                groupRotation_ = fallbackRotation;
            }
        }
    }

    const UpdateClock clock = selectedClock();
    lastUpdateClock_ = clock;
    lastUpdateFrame_ = world_.frame(clock);
    cacheValid_ = true;
    dirty_ = false;

    writeBackSucceeded_ = false;
    if (groupEntity != 0)
    {
        Transform transform{};
        if (world_.exists(groupEntity))
            transform = world_.transform(groupEntity);
        transform.position = groupPosition_;
        transform.rotation = groupRotation_;
        writeBackSucceeded_ = world_.trySetTransform(groupEntity, transform);
    }
}

std::pair<math::Vec3, float> TargetGroup::weightedMemberBounds(
    const ValidMember& member) const
{
    const float ratio = maxWeight_ > math::Epsilon
        ? member.weight / maxWeight_
        : 0.0f;
    return {
        math::lerp(averagePosition_, member.position, ratio),
        member.radius * ratio,
    };
}

void TargetGroup::refresh() const
{
    if (!cacheValid_ || dirty_ || lastUpdateClock_ != selectedClock()
        || lastUpdateFrame_ != currentUpdateFrame())
    {
        const_cast<TargetGroup*>(this)->doUpdate();
    }
}

bool TargetGroup::isValid() const
{
    return true;
}

bool TargetGroup::isEmpty() const
{
    refresh();
    return validMembers_.empty();
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
    refresh();
    const math::Quat inverseRotation = math::normalize(math::inverse(observerRotation));
    auto toView = [&](math::Vec3 worldPosition)
    {
        return math::rotate(inverseRotation, worldPosition - observerPosition);
    };

    if (validMembers_.empty())
        return {toView(averagePosition_), math::Vec3::zero};

    math::Bounds bounds{toView(averagePosition_), math::Vec3::zero};
    bool gotOne = false;
    for (const ValidMember& member : validMembers_)
    {
        const auto [position, radius] = weightedMemberBounds(member);
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

    refresh();
    if (validMembers_.empty())
        return;

    const math::Quat inverseRotation = math::normalize(math::inverse(observerRotation));
    auto toView = [&](math::Vec3 worldPosition)
    {
        return math::rotate(inverseRotation, worldPosition - observerPosition);
    };

    const float averageDepth = -toView(averagePosition_).z;
    zRange = {
        averageDepth - boundingSphere_.radius,
        averageDepth + boundingSphere_.radius,
    };

    float minTangentX = 0.0f;
    float maxTangentX = 0.0f;
    float minTangentY = 0.0f;
    float maxTangentY = 0.0f;
    bool haveOne = false;

    for (const ValidMember& member : validMembers_)
    {
        const auto [position, radius] = weightedMemberBounds(member);
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
