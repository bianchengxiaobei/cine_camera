#include <cinerig/components/confiner_oven.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace cinerig
{
namespace
{

using Path = std::vector<math::Vec2>;

constexpr float kPointEpsilon = 1e-5f;
constexpr float kAreaEpsilon = 1e-6f;
constexpr float kMiterLimit = 2.0f;
constexpr int kMaxDecompositionDepth = 16;

bool isFinite(math::Vec2 point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

Path cleanPath(const Path& input)
{
    Path result;
    result.reserve(input.size());
    for (const math::Vec2 point : input)
    {
        if (!isFinite(point))
            continue;
        if (!result.empty()
            && (point - result.back()).sqrMagnitude() <= kPointEpsilon * kPointEpsilon)
        {
            continue;
        }
        result.push_back(point);
    }

    if (result.size() > 1
        && (result.front() - result.back()).sqrMagnitude()
            <= kPointEpsilon * kPointEpsilon)
    {
        result.pop_back();
    }

    return result;
}

float signedArea(const Path& path)
{
    float area = 0.0f;
    const std::size_t count = path.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        const math::Vec2 a = path[index];
        const math::Vec2 b = path[(index + 1) % count];
        area += math::cross(a, b);
    }
    return area * 0.5f;
}

bool pointOnSegment(math::Vec2 point, math::Vec2 a, math::Vec2 b)
{
    const math::Vec2 edge = b - a;
    const float lengthSquared = edge.sqrMagnitude();
    if (lengthSquared <= kPointEpsilon * kPointEpsilon)
        return (point - a).sqrMagnitude() <= kPointEpsilon * kPointEpsilon;

    const float t = math::clamp01(math::dot(point - a, edge) / lengthSquared);
    const math::Vec2 closest = a + edge * t;
    return (point - closest).sqrMagnitude() <= kPointEpsilon * kPointEpsilon;
}

enum class PointClass
{
    Outside,
    Inside,
    Boundary,
};

PointClass classifyPoint(math::Vec2 point, const Path& path)
{
    if (path.empty())
        return PointClass::Outside;
    if (path.size() == 1)
    {
        return (point - path.front()).sqrMagnitude()
                <= kPointEpsilon * kPointEpsilon
            ? PointClass::Boundary
            : PointClass::Outside;
    }
    if (path.size() == 2)
    {
        return pointOnSegment(point, path[0], path[1])
            ? PointClass::Boundary
            : PointClass::Outside;
    }

    const std::size_t count = path.size();
    bool inside = false;
    for (std::size_t index = 0; index < count; ++index)
    {
        const math::Vec2 a = path[index];
        const math::Vec2 b = path[(index + 1) % count];
        if (pointOnSegment(point, a, b))
            return PointClass::Boundary;

        if ((a.y > point.y) != (b.y > point.y))
        {
            const float crossingX = a.x
                + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (point.x < crossingX)
                inside = !inside;
        }
    }
    return inside ? PointClass::Inside : PointClass::Outside;
}

bool pointInRegion(const std::vector<Path>& paths, math::Vec2 point)
{
    int insideCount = 0;
    for (const Path& path : paths)
    {
        const PointClass classification = classifyPoint(point, path);
        if (classification == PointClass::Boundary)
            return true;
        if (classification == PointClass::Inside)
            ++insideCount;
    }
    return (insideCount % 2) != 0;
}

bool pointInPath(math::Vec2 point, const Path& path)
{
    return classifyPoint(point, path) != PointClass::Outside;
}

math::Vec2 leftNormal(math::Vec2 edge)
{
    const float length = edge.length();
    if (length <= kPointEpsilon)
        return {};
    return {-edge.y / length, edge.x / length};
}

math::Vec2 closestPointOnSegment(math::Vec2 point, math::Vec2 a, math::Vec2 b)
{
    const math::Vec2 edge = b - a;
    const float lengthSquared = edge.sqrMagnitude();
    if (lengthSquared <= kPointEpsilon * kPointEpsilon)
        return a;
    const float t = math::clamp01(math::dot(point - a, edge) / lengthSquared);
    return a + edge * t;
}

void appendUnique(Path& path, math::Vec2 point)
{
    if (path.empty()
        || (path.back() - point).sqrMagnitude() > kPointEpsilon * kPointEpsilon)
    {
        path.push_back(point);
    }
}

Path offsetPathLeft(const Path& path, float distance)
{
    if (path.size() < 3)
        return {};
    if (std::abs(distance) <= kPointEpsilon)
        return path;

    const std::size_t count = path.size();
    const float maxMiter = std::max(std::abs(distance) * kMiterLimit, kPointEpsilon);
    Path result;
    result.reserve(count * 2);

    for (std::size_t index = 0; index < count; ++index)
    {
        const math::Vec2 current = path[index];
        const math::Vec2 previous = path[(index + count - 1) % count];
        const math::Vec2 next = path[(index + 1) % count];
        const math::Vec2 previousEdge = current - previous;
        const math::Vec2 nextEdge = next - current;
        if (previousEdge.sqrMagnitude() <= kPointEpsilon * kPointEpsilon
            || nextEdge.sqrMagnitude() <= kPointEpsilon * kPointEpsilon)
        {
            continue;
        }

        const math::Vec2 previousNormal = leftNormal(previousEdge);
        const math::Vec2 nextNormal = leftNormal(nextEdge);
        const math::Vec2 previousStart = current + previousNormal * distance;
        const math::Vec2 nextStart = current + nextNormal * distance;

        const float denominator = math::cross(previousEdge, nextEdge);
        if (std::abs(denominator) <= kPointEpsilon * previousEdge.length() * nextEdge.length())
        {
            if (math::dot(previousEdge, nextEdge) > 0.0f)
            {
                appendUnique(result, nextStart);
            }
            else
            {
                appendUnique(result, previousStart);
                appendUnique(result, nextStart);
            }
            continue;
        }

        const float t = math::cross(nextStart - previousStart, nextEdge) / denominator;
        const math::Vec2 intersection = previousStart + previousEdge * t;
        if ((intersection - current).sqrMagnitude() > maxMiter * maxMiter)
        {
            appendUnique(result, previousStart);
            appendUnique(result, nextStart);
        }
        else
        {
            appendUnique(result, intersection);
        }
    }

    return cleanPath(result);
}

bool strictSegmentIntersection(
    math::Vec2 a,
    math::Vec2 b,
    math::Vec2 c,
    math::Vec2 d,
    math::Vec2& intersection)
{
    const math::Vec2 r = b - a;
    const math::Vec2 s = d - c;
    const float denominator = math::cross(r, s);
    if (std::abs(denominator) <= kPointEpsilon)
        return false;

    const math::Vec2 offset = c - a;
    const float t = math::cross(offset, s) / denominator;
    const float u = math::cross(offset, r) / denominator;
    constexpr float limit = 1e-5f;
    if (t <= limit || t >= 1.0f - limit || u <= limit || u >= 1.0f - limit)
        return false;

    intersection = a + r * t;
    return true;
}

void appendDecomposed(
    const Path& path,
    bool expectedPositive,
    const std::vector<Path>& originalPaths,
    int depth,
    std::vector<Path>& output)
{
    const Path cleaned = cleanPath(path);
    if (cleaned.size() < 3)
        return;

    const std::size_t count = cleaned.size();
    for (std::size_t first = 0; first < count; ++first)
    {
        const std::size_t firstNext = (first + 1) % count;
        for (std::size_t second = first + 1; second < count; ++second)
        {
            if (second == firstNext || (first == 0 && second + 1 == count))
                continue;

            const std::size_t secondNext = (second + 1) % count;
            math::Vec2 intersection{};
            if (!strictSegmentIntersection(
                    cleaned[first],
                    cleaned[firstNext],
                    cleaned[second],
                    cleaned[secondNext],
                    intersection))
            {
                continue;
            }

            if (depth >= kMaxDecompositionDepth)
                return;

            Path firstLoop;
            for (std::size_t index = first + 1; index <= second; ++index)
                firstLoop.push_back(cleaned[index]);
            firstLoop.push_back(intersection);

            Path secondLoop;
            for (std::size_t index = second + 1; index < count; ++index)
                secondLoop.push_back(cleaned[index]);
            for (std::size_t index = 0; index <= first; ++index)
                secondLoop.push_back(cleaned[index]);
            secondLoop.push_back(intersection);

            appendDecomposed(
                firstLoop,
                expectedPositive,
                originalPaths,
                depth + 1,
                output);
            appendDecomposed(
                secondLoop,
                expectedPositive,
                originalPaths,
                depth + 1,
                output);
            return;
        }
    }

    const float area = signedArea(cleaned);
    if (std::abs(area) <= kAreaEpsilon)
        return;
    if ((area > 0.0f) != expectedPositive)
        return;

    const math::Vec2 edgeMidpoint = (cleaned.front() + cleaned[1]) * 0.5f;
    if (!pointInRegion(originalPaths, edgeMidpoint))
        return;

    output.push_back(cleaned);
}

bool solutionRegionCollapsed(const std::vector<Path>& paths)
{
    for (std::size_t outer = 0; outer < paths.size(); ++outer)
    {
        if (signedArea(paths[outer]) <= 0.0f)
            continue;
        for (std::size_t hole = 0; hole < paths.size(); ++hole)
        {
            if (outer == hole || signedArea(paths[hole]) >= 0.0f)
                continue;
            if (pointInPath(paths[outer].front(), paths[hole]))
                return true;
        }
    }
    return false;
}

std::vector<Path> erodePaths(const std::vector<Path>& originalPaths, float distance)
{
    std::vector<Path> result;
    for (std::size_t index = 0; index < originalPaths.size(); ++index)
    {
        int depth = 0;
        const math::Vec2 representative = originalPaths[index].front();
        for (std::size_t other = 0; other < originalPaths.size(); ++other)
        {
            if (other != index && pointInPath(representative, originalPaths[other]))
                ++depth;
        }

        const bool expectedPositive = (depth % 2) == 0;
        Path normalized = originalPaths[index];
        if ((signedArea(normalized) > 0.0f) != expectedPositive)
            std::reverse(normalized.begin(), normalized.end());

        const Path offset = offsetPathLeft(normalized, distance);
        if (offset.size() < 3)
            continue;

        appendDecomposed(
            offset,
            expectedPositive,
            originalPaths,
            0,
            result);
    }
    return result;
}

} // namespace

ConfinerOven::ConfinerOven(
    const std::vector<std::vector<math::Vec2>>& inputPaths,
    float aspectRatio,
    float maxFrustumHeight,
    float skeletonPadding)
{
    initialize(inputPaths, aspectRatio, maxFrustumHeight, skeletonPadding);
}

void ConfinerOven::initialize(
    const std::vector<std::vector<math::Vec2>>& inputPaths,
    float aspectRatio,
    float maxFrustumHeight,
    float skeletonPadding)
{
    originalPath_.clear();
    aspectRatio_ = std::isfinite(aspectRatio) && std::abs(aspectRatio) > math::Epsilon
        ? std::abs(aspectRatio)
        : 1.0f;
    userSetMaxFrustumHeight_ = maxFrustumHeight;
    skeletonPadding_ = std::max(0.0f, skeletonPadding);
    theoreticalMaxFrustumHeight_ = 0.0f;
    bakeProgress_ = 1.0f;
    state_ = BakingState::Baked;

    std::vector<Path> cleaned;
    cleaned.reserve(inputPaths.size());
    for (const Path& input : inputPaths)
    {
        Path path = cleanPath(input);
        if (path.size() >= 3)
            cleaned.push_back(std::move(path));
    }
    if (cleaned.empty())
        return;

    float rawMinX = std::numeric_limits<float>::max();
    float rawMaxX = std::numeric_limits<float>::lowest();
    for (const Path& path : cleaned)
    {
        for (const math::Vec2 point : path)
        {
            rawMinX = std::min(rawMinX, point.x);
            rawMaxX = std::max(rawMaxX, point.x);
        }
    }
    centerX_ = (rawMinX + rawMaxX) * 0.5f;

    minX_ = std::numeric_limits<float>::max();
    maxX_ = std::numeric_limits<float>::lowest();
    minY_ = std::numeric_limits<float>::max();
    maxY_ = std::numeric_limits<float>::lowest();

    originalPath_.reserve(cleaned.size());
    for (const Path& path : cleaned)
    {
        Path stretched;
        stretched.reserve(path.size());
        for (const math::Vec2 point : path)
        {
            const math::Vec2 value = stretch(point);
            if (!isFinite(value))
                continue;
            stretched.push_back(value);
            minX_ = std::min(minX_, value.x);
            maxX_ = std::max(maxX_, value.x);
            minY_ = std::min(minY_, value.y);
            maxY_ = std::max(maxY_, value.y);
        }
        if (stretched.size() >= 3)
            originalPath_.push_back(std::move(stretched));
    }

    if (originalPath_.empty())
        return;

    const float width = maxX_ - minX_;
    const float height = maxY_ - minY_;
    theoreticalMaxFrustumHeight_ = std::max(width, height) * 0.5f;
}

void ConfinerOven::bakeConfiner(float maxComputationTimePerFrameInSeconds)
{
    (void)maxComputationTimePerFrameInSeconds;
    if (!isValid())
        return;
    state_ = BakingState::Baked;
    bakeProgress_ = 1.0f;
}

math::Vec2 ConfinerOven::stretch(math::Vec2 point) const
{
    return {(point.x - centerX_) / aspectRatio_ + centerX_, point.y};
}

math::Vec2 ConfinerOven::unstretch(math::Vec2 point) const
{
    return {(point.x - centerX_) * aspectRatio_ + centerX_, point.y};
}

ConfinerOven::BakedSolution ConfinerOven::getBakedSolution(float frustumHeight) const
{
    BakedSolution result;
    if (!isValid())
        return result;

    float height = std::max(0.0f, frustumHeight);
    if (userSetMaxFrustumHeight_ > 0.0f)
        height = std::min(height, userSetMaxFrustumHeight_);

    result.valid_ = true;
    result.frustumHeight_ = height;
    result.aspect_ = aspectRatio_;
    result.centerX_ = centerX_;
    const float width = maxX_ - minX_;
    const float shapeHeight = maxY_ - minY_;
    result.diagonalSquared_ = width * width + shapeHeight * shapeHeight;

    if (height >= theoreticalMaxFrustumHeight_)
    {
        result.paths_.push_back({
            {(minX_ + maxX_) * 0.5f, (minY_ + maxY_) * 0.5f},
        });
        return result;
    }

    if (height <= kPointEpsilon)
    {
        result.paths_ = originalPath_;
        return result;
    }

    result.paths_ = erodePaths(originalPath_, height);
    if (result.paths_.empty() || solutionRegionCollapsed(result.paths_))
    {
        result.paths_.clear();
        result.paths_.push_back({
            {(minX_ + maxX_) * 0.5f, (minY_ + maxY_) * 0.5f},
        });
    }
    return result;
}

math::Vec2 ConfinerOven::BakedSolution::confinePoint(math::Vec2 point) const
{
    if (!valid_ || paths_.empty())
        return point;

    const math::Vec2 stretchedPoint{
        (point.x - centerX_) / aspect_ + centerX_,
        point.y,
    };
    if (pointInRegion(paths_, stretchedPoint))
        return point;

    math::Vec2 closest = stretchedPoint;
    float minimumDistance = std::numeric_limits<float>::max();
    bool found = false;
    for (const Path& path : paths_)
    {
        if (path.empty())
            continue;
        if (path.size() == 1)
        {
            const float diffX = std::abs(stretchedPoint.x - path.front().x);
            const float diffY = std::abs(stretchedPoint.y - path.front().y);
            float distance = diffX * diffX + diffY * diffY;
            if (diffX > frustumHeight_ || diffY > frustumHeight_)
                distance += diagonalSquared_;
            if (distance < minimumDistance)
            {
                minimumDistance = distance;
                closest = path.front();
                found = true;
            }
            continue;
        }

        const std::size_t count = path.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            const math::Vec2 candidate = closestPointOnSegment(
                stretchedPoint,
                path[index],
                path[(index + 1) % count]);
            const float diffX = std::abs(stretchedPoint.x - candidate.x);
            const float diffY = std::abs(stretchedPoint.y - candidate.y);
            float distance = diffX * diffX + diffY * diffY;
            if (diffX > frustumHeight_ || diffY > frustumHeight_)
                distance += diagonalSquared_;
            if (distance < minimumDistance)
            {
                minimumDistance = distance;
                closest = candidate;
                found = true;
            }
        }
    }

    if (!found)
        return point;
    return {
        (closest.x - centerX_) * aspect_ + centerX_,
        closest.y,
    };
}

} // namespace cinerig

