#pragma once

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
        float frustumHeight_ = 0.0f;
        float aspect_ = 1.0f;
        float centerX_ = 0.0f;
        float diagonalSquared_ = 0.0f;
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
    math::Vec2 stretch(math::Vec2 point) const;
    math::Vec2 unstretch(math::Vec2 point) const;

    std::vector<std::vector<math::Vec2>> originalPath_;
    float aspectRatio_ = 1.0f;
    float centerX_ = 0.0f;
    float theoreticalMaxFrustumHeight_ = 0.0f;
    float userSetMaxFrustumHeight_ = -1.0f;
    float skeletonPadding_ = 0.0f;
    float minX_ = 0.0f;
    float maxX_ = 0.0f;
    float minY_ = 0.0f;
    float maxY_ = 0.0f;
    BakingState state_ = BakingState::Baked;
    float bakeProgress_ = 1.0f;
};

} // namespace cinerig
