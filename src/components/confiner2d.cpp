#include <cinerig/components/confiner2d.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

#include <cinerig/camera/virtual_camera.hpp>
#include <cinerig/math/damp.hpp>

namespace cinerig
{
namespace
{

constexpr float kCornerAngleThreshold = 10.0f;

float vectorAngleDegrees(math::Vec3 a, math::Vec3 b)
{
    const float lengthA = a.length();
    const float lengthB = b.length();
    if (lengthA <= math::Epsilon || lengthB <= math::Epsilon)
        return 0.0f;
    const float cosine = math::clamp(
        math::dot(a, b) / (lengthA * lengthB),
        -1.0f,
        1.0f);
    return std::acos(cosine) * math::RadToDeg;
}

float columnLength(const math::Mat4& matrix, int column)
{
    const math::Vec4 value = matrix.column(column);
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

void Confiner2D::ShapeCache::invalidate()
{
    valid = false;
    oven = ConfinerOven{};
    bakedToWorld = math::Mat4::identity();
    deltaWorldToBaked = math::Mat4::identity();
    deltaBakedToWorld = math::Mat4::identity();
    aspectRatio = 1.0f;
    deltaScale = 1.0f;
    oversize = {};
}

void Confiner2D::reset()
{
    boundingShape.clear();
    boundingShapeOffset = {};
    boundingShapeToWorld = math::Mat4::identity();
    damping = 0.5f;
    slowingDistance = 5.0f;
    oversizeWindow = {};
    shapeCache_.invalidate();
    extraStates_.clear();
}

void Confiner2D::validate()
{
    damping = std::max(0.0f, damping);
    slowingDistance = std::max(0.0f, slowingDistance);
    oversizeWindow.maxWindowSize = std::max(0.0f, oversizeWindow.maxWindowSize);
    oversizeWindow.padding = math::clamp(oversizeWindow.padding, 0.0f, 100.0f);
}

bool Confiner2D::boundingShapeIsBaked() const
{
    return shapeCache_.valid
        && shapeCache_.oven.state() == ConfinerOven::BakingState::Baked;
}

bool Confiner2D::bakeBoundingShape(VirtualCamera& vcam, float maxTimeInSeconds)
{
    (void)maxTimeInSeconds;
    bool changed = false;
    if (!validateShapeCache(vcam.state().lens.aspect, changed))
        return false;
    return boundingShapeIsBaked();
}

void Confiner2D::invalidateBoundingShapeCache()
{
    shapeCache_.invalidate();
    invalidateLensCache();
}

void Confiner2D::invalidateLensCache()
{
    for (auto& [cameraId, extra] : extraStates_)
    {
        (void)cameraId;
        extra.bakedSolution = {};
        extra.frustumHeight = 0.0f;
    }
}

bool Confiner2D::cameraWasDisplaced(CameraId cameraId) const
{
    return cameraDisplacementDistance(cameraId) > 0.0f;
}

float Confiner2D::cameraDisplacementDistance(CameraId cameraId) const
{
    const ExtraState* extra = findExtraState(cameraId);
    return extra == nullptr ? 0.0f : extra->previousDisplacement.length();
}

float Confiner2D::calculateHalfFrustumHeight(
    const LensSettings& lens,
    float cameraPositionLocalZ)
{
    const float halfFrustumHeight = lens.orthographic
        ? lens.orthographicSize
        : cameraPositionLocalZ
            * std::tan(lens.fieldOfView * 0.5f * math::DegToRad);
    return std::abs(halfFrustumHeight);
}

void Confiner2D::onTargetObjectWarped(
    EntityId target,
    math::Vec3 positionDelta)
{
    const VirtualCamera* camera = virtualCamera();
    if (camera == nullptr
        || !camera->isEntityTarget(camera->effectiveFollowTarget(), target))
    {
        return;
    }

    for (auto& [cameraId, extra] : extraStates_)
    {
        (void)cameraId;
        extra.previousCameraPosition += positionDelta;
    }
}

float Confiner2D::maxDampTime() const
{
    return std::max(damping, slowingDistance * 0.2f);
}

Confiner2D::ExtraState& Confiner2D::extraState(CameraId cameraId)
{
    return extraStates_[cameraId];
}

const Confiner2D::ExtraState* Confiner2D::findExtraState(CameraId cameraId) const
{
    const auto iterator = extraStates_.find(cameraId);
    return iterator == extraStates_.end() ? nullptr : &iterator->second;
}

bool Confiner2D::oversizeMatches(const ShapeCache& cache) const
{
    const float scale = std::max(1.0f, std::max(
        std::abs(oversizeWindow.maxWindowSize),
        std::abs(cache.oversize.maxWindowSize)));
    const float epsilon = 1e-4f * scale;
    return cache.oversize.enabled == oversizeWindow.enabled
        && std::abs(cache.oversize.maxWindowSize - oversizeWindow.maxWindowSize)
            <= epsilon
        && std::abs(cache.oversize.padding - oversizeWindow.padding) <= 1e-4f;
}

bool Confiner2D::deltaIsUniform() const
{
    const float scaleX = columnLength(shapeCache_.deltaWorldToBaked, 0);
    const float scaleY = columnLength(shapeCache_.deltaWorldToBaked, 1);
    if (!std::isfinite(scaleX) || !std::isfinite(scaleY)
        || scaleX <= math::Epsilon || scaleY <= math::Epsilon)
    {
        return false;
    }
    const float tolerance = 1e-4f * std::max(scaleX, scaleY);
    return std::abs(scaleX - scaleY) <= tolerance;
}

void Confiner2D::calculateDelta()
{
    const math::Mat4 currentWorldToShape =
        math::inverseAffine(boundingShapeToWorld);
    const math::Mat4 offset =
        math::Mat4::translation({-boundingShapeOffset.x, -boundingShapeOffset.y, 0.0f});
    shapeCache_.deltaWorldToBaked =
        shapeCache_.bakedToWorld * offset * currentWorldToShape;
    shapeCache_.deltaBakedToWorld =
        math::inverseAffine(shapeCache_.deltaWorldToBaked);
    shapeCache_.deltaScale =
        std::max(columnLength(shapeCache_.deltaWorldToBaked, 0), math::Epsilon);
}

bool Confiner2D::validateShapeCache(float aspectRatio, bool& changed)
{
    changed = false;
    if (!isValid())
    {
        if (shapeCache_.valid)
        {
            shapeCache_.invalidate();
            invalidateLensCache();
            changed = true;
        }
        return false;
    }

    const float validatedAspect = std::isfinite(aspectRatio)
            && std::abs(aspectRatio) > math::Epsilon
        ? std::abs(aspectRatio)
        : 1.0f;

    if (shapeCache_.valid
        && std::abs(shapeCache_.aspectRatio - validatedAspect) <= math::Epsilon
        && oversizeMatches(shapeCache_))
    {
        calculateDelta();
        if (deltaIsUniform())
            return true;
    }

    shapeCache_.invalidate();
    std::vector<std::vector<math::Vec2>> worldPaths;
    worldPaths.reserve(boundingShape.size());
    for (const std::vector<math::Vec2>& path : boundingShape)
    {
        std::vector<math::Vec2> worldPath;
        worldPath.reserve(path.size());
        for (const math::Vec2 point : path)
        {
            if (!std::isfinite(point.x) || !std::isfinite(point.y))
                continue;
            const math::Vec3 world = boundingShapeToWorld.transformPoint(
                {point.x, point.y, 0.0f});
            if (!std::isfinite(world.x) || !std::isfinite(world.y))
                continue;
            worldPath.push_back({world.x, world.y});
        }
        if (!worldPath.empty())
            worldPaths.push_back(std::move(worldPath));
    }

    if (worldPaths.empty())
    {
        invalidateLensCache();
        changed = true;
        return false;
    }

    shapeCache_.oven.initialize(
        worldPaths,
        validatedAspect,
        oversizeWindow.enabled ? oversizeWindow.maxWindowSize : -1.0f,
        oversizeWindow.padding);
    if (!shapeCache_.oven.isValid())
    {
        invalidateLensCache();
        changed = true;
        return false;
    }

    shapeCache_.valid = true;
    shapeCache_.bakedToWorld = boundingShapeToWorld;
    shapeCache_.aspectRatio = validatedAspect;
    shapeCache_.oversize = oversizeWindow;
    calculateDelta();
    invalidateLensCache();
    changed = true;
    return true;
}

math::Vec3 Confiner2D::confinePoint(
    math::Vec3 point,
    math::Vec3 forward,
    const ExtraState& extra) const
{
    const math::Vec3 local = shapeCache_.deltaWorldToBaked.transformPoint(point);
    const math::Vec2 confined = extra.bakedSolution.confinePoint(
        {local.x, local.y});
    const math::Vec3 bakedPosition{confined.x, confined.y, 0.0f};
    const math::Vec3 newPosition =
        shapeCache_.deltaBakedToWorld.transformPoint(bakedPosition);
    return newPosition - forward * math::dot(forward, newPosition - point);
}

float Confiner2D::distanceFromEdge(
    math::Vec3 point,
    math::Vec3 directionUnit,
    float maxDistance,
    math::Vec3 forward,
    const ExtraState& extra) const
{
    const math::Vec3 edgePoint = point + directionUnit * maxDistance;
    return std::max(
        0.0f,
        maxDistance - (confinePoint(edgePoint, forward, extra) - edgePoint).length());
}

void Confiner2D::postPipelineStageCallback(
    VirtualCamera& vcam,
    Stage stage,
    CameraState& state,
    float deltaTime)
{
    if (stage != Stage::Body)
        return;

    bool cacheChanged = false;
    if (!validateShapeCache(state.lens.aspect, cacheChanged))
        return;

    ExtraState& extra = extraState(vcam.cameraId());
    const math::Vec3 cameraPosition = state.correctedPosition();
    if (cacheChanged || !extra.bakedSolution.isValid())
    {
        const math::Vec3 local =
            shapeCache_.deltaWorldToBaked.transformPoint(cameraPosition);
        extra.frustumHeight =
            calculateHalfFrustumHeight(state.lens, local.z) * shapeCache_.deltaScale;
        extra.bakedSolution =
            shapeCache_.oven.getBakedSolution(extra.frustumHeight);
    }
    if (!extra.bakedSolution.isValid())
        return;

    const math::Vec3 forward = math::rotate(
        state.correctedOrientation(),
        math::Vec3::forward);
    math::Vec3 newPosition = confinePoint(cameraPosition, forward, extra);

    if (slowingDistance > math::Epsilon
        && deltaTime >= 0.0f
        && vcam.previousStateIsValid)
    {
        const math::Vec3 previousPosition = extra.previousCameraPosition;
        const math::Vec3 direction = newPosition - previousPosition;
        const float speed = direction.length();
        if (speed > math::Epsilon)
        {
            const float distance = distanceFromEdge(
                previousPosition,
                direction / speed,
                slowingDistance,
                forward,
                extra);
            const float t = distance / slowingDistance;
            newPosition = math::lerp(
                previousPosition,
                newPosition,
                t * t * t + 0.05f);
        }
    }

    math::Vec3 displacement = newPosition - cameraPosition;
    const math::Vec3 previousDisplacement = extra.previousDisplacement;
    extra.previousDisplacement = displacement;

    if (!vcam.previousStateIsValid || deltaTime < 0.0f || damping <= 0.0f)
    {
        extra.dampedDisplacement = {};
    }
    else
    {
        if (previousDisplacement.sqrMagnitude() > 0.01f
            && vectorAngleDegrees(previousDisplacement, displacement)
                > kCornerAngleThreshold)
        {
            extra.dampedDisplacement += displacement - previousDisplacement;
        }

        extra.dampedDisplacement -= math::standardDamp(
            extra.dampedDisplacement,
            damping,
            deltaTime);
        displacement -= extra.dampedDisplacement;
    }

    state.positionCorrection += displacement;
    extra.previousCameraPosition = state.correctedPosition();
}

} // namespace cinerig
