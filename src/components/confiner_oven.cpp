#include <cinerig/components/confiner_oven.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <unordered_set>

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

bool segmentsIntersectOrTouch(
    math::Vec2 a,
    math::Vec2 b,
    math::Vec2 c,
    math::Vec2 d)
{
    if (pointOnSegment(a, c, d) || pointOnSegment(b, c, d)
        || pointOnSegment(c, a, b) || pointOnSegment(d, a, b))
    {
        return true;
    }

    const math::Vec2 r = b - a;
    const math::Vec2 s = d - c;
    const float denominator = math::cross(r, s);
    if (std::abs(denominator) <= kPointEpsilon)
        return false;

    const math::Vec2 offset = c - a;
    const float t = math::cross(offset, s) / denominator;
    const float u = math::cross(offset, r) / denominator;
    return t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f;
}

bool segmentIntersectsRegion(
    math::Vec2 a,
    math::Vec2 b,
    const std::vector<Path>& region)
{
    for (const Path& path : region)
    {
        if (path.size() < 2)
            continue;
        for (std::size_t index = 0; index < path.size(); ++index)
        {
            if (segmentsIntersectOrTouch(
                    a,
                    b,
                    path[index],
                    path[(index + 1) % path.size()]))
            {
                return true;
            }
        }
    }
    return false;
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
    const std::vector<Path>* clipPaths,
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
                clipPaths,
                depth + 1,
                output);
            appendDecomposed(
                secondLoop,
                expectedPositive,
                clipPaths,
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
    if (clipPaths != nullptr && !pointInRegion(*clipPaths, edgeMidpoint))
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

std::vector<Path> offsetPaths(
    const std::vector<Path>& region,
    float distance,
    bool clipToRegion)
{
    std::vector<Path> result;
    for (std::size_t index = 0; index < region.size(); ++index)
    {
        if (region[index].size() < 3)
            continue;

        int depth = 0;
        const math::Vec2 representative = region[index].front();
        for (std::size_t other = 0; other < region.size(); ++other)
        {
            if (other != index && pointInPath(representative, region[other]))
                ++depth;
        }

        const bool expectedPositive = (depth % 2) == 0;
        Path normalized = region[index];
        if ((signedArea(normalized) > 0.0f) != expectedPositive)
            std::reverse(normalized.begin(), normalized.end());

        const Path offset = offsetPathLeft(normalized, distance);
        if (offset.size() < 3)
            continue;

        appendDecomposed(
            offset,
            expectedPositive,
            clipToRegion ? &region : nullptr,
            0,
            result);
    }
    return result;
}

struct Segment
{
    math::Vec2 a{};
    math::Vec2 b{};
};

struct PointKey
{
    std::int64_t x = 0;
    std::int64_t y = 0;

    bool operator==(const PointKey& other) const noexcept
    {
        return x == other.x && y == other.y;
    }
};

struct PointKeyHash
{
    std::size_t operator()(const PointKey& key) const noexcept
    {
        const std::uint64_t x = static_cast<std::uint64_t>(key.x);
        const std::uint64_t y = static_cast<std::uint64_t>(key.y);
        std::uint64_t value = x + 0x9e3779b97f4a7c15ull;
        value = (value ^ y) * 0xbf58476d1ce4e5b9ull;
        return static_cast<std::size_t>(value ^ (value >> 29));
    }
};

struct DirectedKey
{
    PointKey from{};
    PointKey to{};

    bool operator==(const DirectedKey& other) const noexcept
    {
        return from == other.from && to == other.to;
    }
};

struct DirectedKeyHash
{
    std::size_t operator()(const DirectedKey& key) const noexcept
    {
        const PointKeyHash hash;
        const std::size_t fromHash = hash(key.from);
        const std::size_t toHash = hash(key.to);
        return fromHash ^ (toHash + 0x9e3779b9u + (fromHash << 6) + (fromHash >> 2));
    }
};

float regionScale(const std::vector<Path>& region)
{
    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    for (const Path& path : region)
    {
        for (const math::Vec2 point : path)
        {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
    }
    if (maxX < minX || maxY < minY)
        return 1.0f;
    return std::max({1.0f, maxX - minX, maxY - minY});
}

PointKey makePointKey(math::Vec2 point, float snap)
{
    return {
        static_cast<std::int64_t>(std::llround(point.x / snap)),
        static_cast<std::int64_t>(std::llround(point.y / snap)),
    };
}

void appendEdgeSegments(const std::vector<Path>& region, std::vector<Segment>& segments)
{
    for (const Path& path : region)
    {
        if (path.size() < 2)
            continue;
        for (std::size_t index = 0; index < path.size(); ++index)
        {
            const math::Vec2 a = path[index];
            const math::Vec2 b = path[(index + 1) % path.size()];
            if ((b - a).sqrMagnitude() > kPointEpsilon * kPointEpsilon)
                segments.push_back({a, b});
        }
    }
}

void appendSplitParameters(
    const Segment& first,
    const Segment& second,
    std::vector<float>& parameters)
{
    const math::Vec2 r = first.b - first.a;
    const math::Vec2 s = second.b - second.a;
    const float lengthSquaredR = r.sqrMagnitude();
    const float lengthSquaredS = s.sqrMagnitude();
    if (lengthSquaredR <= kPointEpsilon * kPointEpsilon
        || lengthSquaredS <= kPointEpsilon * kPointEpsilon)
    {
        return;
    }

    constexpr float pad = 1e-5f;
    const float denominator = math::cross(r, s);
    const float parallelLimit =
        kPointEpsilon * std::sqrt(lengthSquaredR * lengthSquaredS);
    if (std::abs(denominator) > parallelLimit)
    {
        const math::Vec2 offset = second.a - first.a;
        const float t = math::cross(offset, s) / denominator;
        const float u = math::cross(offset, r) / denominator;
        if (t >= -pad && t <= 1.0f + pad && u >= -pad && u <= 1.0f + pad)
            parameters.push_back(math::clamp01(t));
        return;
    }

    const float lineDistance = std::abs(math::cross(second.a - first.a, r))
        / std::sqrt(lengthSquaredR);
    if (lineDistance > kPointEpsilon)
        return;

    const math::Vec2 endpoints[2] = {second.a, second.b};
    for (const math::Vec2 point : endpoints)
    {
        if (!pointOnSegment(point, first.a, first.b))
            continue;
        const float t = math::clamp01(
            math::dot(point - first.a, r) / lengthSquaredR);
        parameters.push_back(t);
    }
}

std::vector<Segment> splitSegments(const std::vector<Segment>& segments)
{
    std::vector<Segment> result;
    for (std::size_t index = 0; index < segments.size(); ++index)
    {
        const Segment& current = segments[index];
        std::vector<float> parameters{0.0f, 1.0f};
        for (std::size_t other = 0; other < segments.size(); ++other)
        {
            if (other != index)
                appendSplitParameters(current, segments[other], parameters);
        }

        std::sort(parameters.begin(), parameters.end());
        std::vector<float> unique;
        unique.reserve(parameters.size());
        for (const float value : parameters)
        {
            if (unique.empty() || std::abs(value - unique.back()) > 1e-6f)
                unique.push_back(value);
        }

        const math::Vec2 direction = current.b - current.a;
        for (std::size_t part = 0; part + 1 < unique.size(); ++part)
        {
            const math::Vec2 a = current.a + direction * unique[part];
            const math::Vec2 b = current.a + direction * unique[part + 1];
            if ((b - a).sqrMagnitude() > kPointEpsilon * kPointEpsilon)
                result.push_back({a, b});
        }
    }
    return result;
}

std::vector<Path> booleanRegion(
    const std::vector<Path>& a,
    const std::vector<Path>& b,
    bool subtract)
{
    if (a.empty() && b.empty())
        return {};
    if (subtract && a.empty())
        return {};
    if (a.empty())
        return b;
    if (b.empty())
        return a;

    std::vector<Segment> sourceSegments;
    appendEdgeSegments(a, sourceSegments);
    appendEdgeSegments(b, sourceSegments);
    const std::vector<Segment> atomic = splitSegments(sourceSegments);

    const float scale = std::max(regionScale(a), regionScale(b));
    const float sideEpsilon = std::max(1e-4f, scale * 1e-6f);
    const float snap = std::max(1e-4f, scale * 1e-6f);

    const auto insideResult = [&](math::Vec2 point)
    {
        const bool insideA = pointInRegion(a, point);
        const bool insideB = pointInRegion(b, point);
        return subtract ? (insideA && !insideB) : (insideA || insideB);
    };

    std::unordered_set<DirectedKey, DirectedKeyHash> chosen;
    for (const Segment& segment : atomic)
    {
        const math::Vec2 edge = segment.b - segment.a;
        const float length = edge.length();
        if (length <= kPointEpsilon)
            continue;

        const math::Vec2 midpoint = (segment.a + segment.b) * 0.5f;
        const math::Vec2 normal = leftNormal(edge);
        const float localEpsilon = std::max(
            1e-7f,
            std::min(sideEpsilon, length * 0.25f));
        const bool leftInside = insideResult(midpoint + normal * localEpsilon);
        const bool rightInside = insideResult(midpoint - normal * localEpsilon);
        if (leftInside == rightInside)
            continue;

        const math::Vec2 from = leftInside ? segment.a : segment.b;
        const math::Vec2 to = leftInside ? segment.b : segment.a;
        const DirectedKey key{makePointKey(from, snap), makePointKey(to, snap)};
        if (key.from == key.to)
            continue;

        const DirectedKey reverse{key.to, key.from};
        if (chosen.erase(reverse) == 0)
            chosen.insert(key);
    }

    std::unordered_map<PointKey, int, PointKeyHash> nodeIndices;
    std::vector<math::Vec2> nodes;
    const auto nodeIndexFor = [&](math::Vec2 point)
    {
        const PointKey key = makePointKey(point, snap);
        const auto found = nodeIndices.find(key);
        if (found != nodeIndices.end())
            return found->second;
        const int index = static_cast<int>(nodes.size());
        nodes.push_back({
            static_cast<float>(key.x) * snap,
            static_cast<float>(key.y) * snap,
        });
        nodeIndices.emplace(key, index);
        return index;
    };

    struct DirectedEdge
    {
        int from = -1;
        int to = -1;
        bool visited = false;
    };
    std::vector<DirectedEdge> edges;
    edges.reserve(chosen.size());
    for (const DirectedKey& key : chosen)
    {
        const int from = nodeIndexFor({
            static_cast<float>(key.from.x) * snap,
            static_cast<float>(key.from.y) * snap,
        });
        const int to = nodeIndexFor({
            static_cast<float>(key.to.x) * snap,
            static_cast<float>(key.to.y) * snap,
        });
        if (from != to)
            edges.push_back({from, to, false});
    }

    std::vector<std::vector<int>> outgoing(nodes.size());
    for (std::size_t index = 0; index < edges.size(); ++index)
        outgoing[static_cast<std::size_t>(edges[index].from)].push_back(
            static_cast<int>(index));

    std::vector<Path> result;
    for (std::size_t seed = 0; seed < edges.size(); ++seed)
    {
        if (edges[seed].visited)
            continue;

        const int startNode = edges[seed].from;
        std::vector<int> loopNodes{startNode};
        int current = static_cast<int>(seed);
        bool closed = false;

        for (std::size_t guard = 0; guard <= edges.size(); ++guard)
        {
            if (edges[static_cast<std::size_t>(current)].visited)
                break;
            edges[static_cast<std::size_t>(current)].visited = true;

            const int node = edges[static_cast<std::size_t>(current)].to;
            if (node == startNode)
            {
                closed = true;
                break;
            }
            loopNodes.push_back(node);

            const math::Vec2 incoming =
                nodes[static_cast<std::size_t>(node)]
                - nodes[static_cast<std::size_t>(
                    edges[static_cast<std::size_t>(current)].from)];
            if (incoming.sqrMagnitude() <= kPointEpsilon * kPointEpsilon)
                break;
            const float backAngle = std::atan2(-incoming.y, -incoming.x);

            int next = -1;
            float bestTurn = std::numeric_limits<float>::max();
            for (const int candidate : outgoing[static_cast<std::size_t>(node)])
            {
                const DirectedEdge& edge = edges[static_cast<std::size_t>(candidate)];
                if (edge.visited)
                    continue;
                const math::Vec2 outgoingVector =
                    nodes[static_cast<std::size_t>(edge.to)]
                    - nodes[static_cast<std::size_t>(node)];
                if (outgoingVector.sqrMagnitude()
                    <= kPointEpsilon * kPointEpsilon)
                {
                    continue;
                }

                const float angle = std::atan2(outgoingVector.y, outgoingVector.x);
                float turn = std::fmod(backAngle - angle, math::TwoPi);
                if (turn < 0.0f)
                    turn += math::TwoPi;
                if (turn < bestTurn)
                {
                    bestTurn = turn;
                    next = candidate;
                }
            }

            if (next < 0)
                break;
            current = next;
        }

        if (!closed || loopNodes.size() < 3)
            continue;

        Path path;
        path.reserve(loopNodes.size());
        for (const int node : loopNodes)
            path.push_back(nodes[static_cast<std::size_t>(node)]);
        path = cleanPath(path);
        if (path.size() >= 3 && std::abs(signedArea(path)) > kAreaEpsilon)
            result.push_back(std::move(path));
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
    theoreticalMaxCandidate_.clear();
    skeleton_.clear();
    solutions_.clear();
    userSetMaxCandidate_.clear();
    leftCandidate_ = {};
    rightCandidate_ = {};
    aspectRatio_ = std::isfinite(aspectRatio) && std::abs(aspectRatio) > math::Epsilon
        ? std::abs(aspectRatio)
        : 1.0f;
    userSetMaxFrustumHeight_ = maxFrustumHeight;
    skeletonPadding_ = std::max(0.0f, skeletonPadding) + 1.0f;
    theoreticalMaxFrustumHeight_ = 0.0f;
    maxFrustumHeight_ = 0.0f;
    stepSize_ = 0.0f;
    currentFrustumHeight_ = 0.0f;
    minFrustumHeightWithBones_ = std::numeric_limits<float>::max();
    bakeTime_ = 0.0f;
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
    theoreticalMaxCandidate_.push_back({
        {(minX_ + maxX_) * 0.5f, (minY_ + maxY_) * 0.5f},
    });

    if (maxFrustumHeight < 0.0f)
        return;

    maxFrustumHeight_ = maxFrustumHeight;
    if (maxFrustumHeight_ == 0.0f
        || maxFrustumHeight_ > theoreticalMaxFrustumHeight_)
    {
        maxFrustumHeight_ = theoreticalMaxFrustumHeight_;
        userSetMaxCandidate_ = theoreticalMaxCandidate_;
    }
    else
    {
        userSetMaxCandidate_ = offsetPaths(
            originalPath_, userSetMaxFrustumHeight_, true);
        if (userSetMaxCandidate_.empty())
            userSetMaxCandidate_ = theoreticalMaxCandidate_;
    }

    std::vector<Path> initial = offsetPaths(originalPath_, 0.0f, true);
    if (initial.empty())
        initial = originalPath_;
    solutions_.push_back({initial, 0.0f, true});
    leftCandidate_ = solutions_.front();
    currentFrustumHeight_ = 0.0f;
    stepSize_ = maxFrustumHeight_;

    if (maxFrustumHeight_ <= math::Epsilon)
        return;

    state_ = BakingState::Baking;
    bakeProgress_ = 0.0f;
}

bool ConfinerOven::PolygonSolution::stateChanged(
    const std::vector<std::vector<math::Vec2>>& candidate) const
{
    if (!hasValue || candidate.size() != polygons.size())
        return true;
    for (std::size_t index = 0; index < polygons.size(); ++index)
    {
        if (candidate[index].size() != polygons[index].size())
            return true;
    }
    return false;
}

float ConfinerOven::minimumBakeStep() const
{
    const float size = std::max(maxX_ - minX_, maxY_ - minY_);
    const float t = math::clamp01((size - 100.0f) / (10000.0f - 100.0f));
    const float floatToInt = math::lerp(100000.0f, 100.0f, t);
    return std::max(1e-6f, 50.0f / floatToInt);
}

void ConfinerOven::bakeConfiner(float maxComputationTimePerFrameInSeconds)
{
    if (state_ != BakingState::Baking || !isValid())
        return;

    const auto startTime = std::chrono::steady_clock::now();
    const float minStep = minimumBakeStep();
    const float heightEpsilon = std::max(1e-6f, maxFrustumHeight_ * 1e-5f);

    while (solutions_.size() < 1000)
    {
        stepSize_ = std::min(
            stepSize_,
            maxFrustumHeight_ - leftCandidate_.frustumHeight);
        if (stepSize_ <= 0.0f)
        {
            if (!rightCandidate_.hasValue)
            {
                solutions_.push_back(leftCandidate_);
                break;
            }
            stepSize_ = 0.0f;
        }

        currentFrustumHeight_ = leftCandidate_.frustumHeight + stepSize_;
        std::vector<Path> candidate =
            std::abs(currentFrustumHeight_ - maxFrustumHeight_) < heightEpsilon
            ? userSetMaxCandidate_
            : offsetPaths(originalPath_, currentFrustumHeight_, true);
        if (candidate.empty())
            candidate = userSetMaxCandidate_;

        if (leftCandidate_.stateChanged(candidate))
        {
            rightCandidate_ = {candidate, currentFrustumHeight_, true};
            stepSize_ = std::max(stepSize_ * 0.5f, minStep);
        }
        else
        {
            leftCandidate_ = {candidate, currentFrustumHeight_, true};
            if (rightCandidate_.hasValue)
                stepSize_ = std::max(stepSize_ * 0.5f, minStep);
        }

        if (rightCandidate_.hasValue && stepSize_ <= minStep)
        {
            solutions_.push_back(leftCandidate_);
            solutions_.push_back(rightCandidate_);
            leftCandidate_ = rightCandidate_;
            rightCandidate_ = {};
            stepSize_ = maxFrustumHeight_;
        }
        else if (!rightCandidate_.hasValue
                 || leftCandidate_.frustumHeight >= maxFrustumHeight_)
        {
            solutions_.push_back(leftCandidate_);
            break;
        }

        const float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - startTime)
                                  .count();
        if (elapsed > maxComputationTimePerFrameInSeconds)
        {
            bakeTime_ += elapsed;
            if (bakeTime_ > 5.0f)
                state_ = BakingState::Timeout;
            bakeProgress_ = maxFrustumHeight_ > math::Epsilon
                ? math::clamp01(
                      leftCandidate_.frustumHeight / maxFrustumHeight_)
                : 1.0f;
            return;
        }
    }

    computeSkeleton();
    for (std::size_t index = solutions_.size(); index > 0; --index)
    {
        if (solutions_[index - 1].polygons.empty())
            solutions_.erase(solutions_.begin() + static_cast<std::ptrdiff_t>(index - 1));
    }
    bakeProgress_ = 1.0f;
    state_ = BakingState::Baked;
}

std::vector<std::vector<math::Vec2>> ConfinerOven::offsetRegion(
    const std::vector<std::vector<math::Vec2>>& region,
    float distance,
    bool clipToRegion) const
{
    return offsetPaths(region, distance, clipToRegion);
}

std::vector<std::vector<math::Vec2>> ConfinerOven::unionPaths(
    const std::vector<std::vector<math::Vec2>>& a,
    const std::vector<std::vector<math::Vec2>>& b) const
{
    return booleanRegion(a, b, false);
}

std::vector<std::vector<math::Vec2>> ConfinerOven::subtractPaths(
    const std::vector<std::vector<math::Vec2>>& a,
    const std::vector<std::vector<math::Vec2>>& b) const
{
    return booleanRegion(a, b, true);
}

void ConfinerOven::computeSkeleton()
{
    skeleton_.clear();
    minFrustumHeightWithBones_ = std::numeric_limits<float>::max();
    for (std::size_t index = 1; index + 1 < solutions_.size(); index += 2)
    {
        const PolygonSolution& previous = solutions_[index];
        const PolygonSolution& next = solutions_[index + 1];
        const float step = skeletonPadding_
            * (next.frustumHeight - previous.frustumHeight);

        const std::vector<Path> expandedPrevious =
            offsetRegion(previous.polygons, -step, false);
        const std::vector<Path> expandedNext =
            offsetRegion(next.polygons, -2.0f * step, false);
        std::vector<Path> lost = subtractPaths(expandedPrevious, expandedNext);
        if (lost.empty())
            continue;

        skeleton_.insert(
            skeleton_.end(),
            std::make_move_iterator(lost.begin()),
            std::make_move_iterator(lost.end()));
        if (minFrustumHeightWithBones_ == std::numeric_limits<float>::max())
            minFrustumHeightWithBones_ = next.frustumHeight;
    }
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
    result.originalPaths_ = originalPath_;
    result.hasBones_ = false;
    const float width = maxX_ - minX_;
    const float shapeHeight = maxY_ - minY_;
    result.diagonalSquared_ = width * width + shapeHeight * shapeHeight;

    if (state_ == BakingState::Baked
        && height >= theoreticalMaxFrustumHeight_)
    {
        result.paths_ = theoreticalMaxCandidate_;
        return result;
    }

    if (height <= kPointEpsilon)
    {
        result.paths_ = originalPath_;
        return result;
    }

    std::vector<Path> solution = offsetRegion(originalPath_, height, true);
    if (solution.empty() || solutionRegionCollapsed(solution))
        solution = theoreticalMaxCandidate_;

    if (state_ == BakingState::Baking || skeleton_.empty())
        result.paths_ = std::move(solution);
    else
        result.paths_ = unionPaths(solution, skeleton_);
    result.hasBones_ = minFrustumHeightWithBones_ < height;
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

    bool insideOriginal = false;
    if (hasBones_)
    {
        for (const Path& path : originalPaths_)
        {
            if (pointInPath(stretchedPoint, path))
            {
                insideOriginal = true;
                break;
            }
        }
    }
    const bool checkIntersectOriginal = hasBones_ && insideOriginal;

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
            if (distance < minimumDistance
                && (!checkIntersectOriginal
                    || !segmentIntersectsRegion(
                        stretchedPoint,
                        path.front(),
                        originalPaths_)))
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
            if (distance < minimumDistance
                && (!checkIntersectOriginal
                    || !segmentIntersectsRegion(
                        stretchedPoint,
                        candidate,
                        originalPaths_)))
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

