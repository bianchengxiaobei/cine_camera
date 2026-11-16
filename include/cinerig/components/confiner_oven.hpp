#pragma once

#include <limits>
#include <vector>

#include <cinerig/math/vec.hpp>

namespace cinerig
{

class ConfinerOven
{
public:
    enum class BakingState
    {
        Baking = 0,
        Baked,
        Timeout,
    };

    class BakedSolution
    {
    public:
        BakedSolution() = default;

        bool isValid() const noexcept { return valid_; }
        bool isEmpty() const noexcept { return paths_.empty(); }

        math::Vec2 confinePoint(math::Vec2 point) const;

        const std::vector<std::vector<math::Vec2>>& paths() const noexcept
        {
            return paths_;
        }

        float frustumHeight() const noexcept { return frustumHeight_; }

    private:
        friend class ConfinerOven;

        bool valid_ = false;
        bool hasBones_ = false;
        float frustumHeight_ = 0.0f;
        float aspect_ = 1.0f;
        float centerX_ = 0.0f;
        float diagonalSquared_ = 0.0f;
        std::vector<std::vector<math::Vec2>> originalPaths_;
        std::vector<std::vector<math::Vec2>> paths_;
    };

    ConfinerOven() = default;
    ConfinerOven(
        const std::vector<std::vector<math::Vec2>>& inputPaths,
        float aspectRatio,
        float maxFrustumHeight = -1.0f,
        float skeletonPadding = 0.0f);

    void initialize(
        const std::vector<std::vector<math::Vec2>>& inputPaths,
        float aspectRatio,
        float maxFrustumHeight = -1.0f,
        float skeletonPadding = 0.0f);

    bool isValid() const noexcept { return !originalPath_.empty(); }
    BakingState state() const noexcept { return state_; }
    float bakeProgress() const noexcept { return bakeProgress_; }

    void bakeConfiner(float maxComputationTimePerFrameInSeconds);

    BakedSolution getBakedSolution(float frustumHeight) const;

private:
    struct PolygonSolution
    {
        std::vector<std::vector<math::Vec2>> polygons;
        float frustumHeight = 0.0f;
        bool hasValue = false;

        bool stateChanged(
            const std::vector<std::vector<math::Vec2>>& candidate) const;
    };

    math::Vec2 stretch(math::Vec2 point) const;
    math::Vec2 unstretch(math::Vec2 point) const;
    float minimumBakeStep() const;

    std::vector<std::vector<math::Vec2>> offsetRegion(
        const std::vector<std::vector<math::Vec2>>& region,
        float distance,
        bool clipToRegion) const;
    std::vector<std::vector<math::Vec2>> unionPaths(
        const std::vector<std::vector<math::Vec2>>& a,
        const std::vector<std::vector<math::Vec2>>& b) const;
    std::vector<std::vector<math::Vec2>> subtractPaths(
        const std::vector<std::vector<math::Vec2>>& a,
        const std::vector<std::vector<math::Vec2>>& b) const;
    void computeSkeleton();

    std::vector<std::vector<math::Vec2>> originalPath_;
    std::vector<std::vector<math::Vec2>> theoreticalMaxCandidate_;
    std::vector<std::vector<math::Vec2>> skeleton_;
    std::vector<PolygonSolution> solutions_;
    PolygonSolution leftCandidate_{};
    PolygonSolution rightCandidate_{};
    std::vector<std::vector<math::Vec2>> userSetMaxCandidate_;
    float aspectRatio_ = 1.0f;
    float centerX_ = 0.0f;
    float theoreticalMaxFrustumHeight_ = 0.0f;
    float userSetMaxFrustumHeight_ = -1.0f;
    float maxFrustumHeight_ = 0.0f;
    float stepSize_ = 0.0f;
    float currentFrustumHeight_ = 0.0f;
    float skeletonPadding_ = 1.0f;
    float minFrustumHeightWithBones_ = std::numeric_limits<float>::max();
    float bakeTime_ = 0.0f;
    float minX_ = 0.0f;
    float maxX_ = 0.0f;
    float minY_ = 0.0f;
    float maxY_ = 0.0f;
    BakingState state_ = BakingState::Baked;
    float bakeProgress_ = 1.0f;
};

} // namespace cinerig
