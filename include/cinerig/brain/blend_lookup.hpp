#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <cinerig/core/blend_definition.hpp>
#include <cinerig/core/camera_interface.hpp>

namespace cinerig
{

inline constexpr std::string_view kAnyCameraLabel = "**ANY CAMERA**";
inline constexpr std::string_view kBlendFromAnyCameraLabel = kAnyCameraLabel;

struct BlendRule
{
    std::string from;
    std::string to;
    BlendDefinition blend;
};

using CustomBlend = BlendRule;

class BlendLookup
{
public:
    using BlendOverride = std::function<BlendDefinition(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming,
        const BlendDefinition& blend)>;

    BlendLookup() = default;
    explicit BlendLookup(const BlendDefinition& defaultBlend);

    void setDefaultBlend(const BlendDefinition& blend);
    const BlendDefinition& defaultBlend() const noexcept;

    void addRule(BlendRule rule);
    void setRules(std::vector<BlendRule> rules);
    void clearRules();
    const std::vector<BlendRule>& rules() const noexcept;

    void setOverride(BlendOverride override);
    void clearOverride();
    bool hasOverride() const noexcept;

    BlendDefinition lookup(
        const ICinemachineCamera* outgoing,
        const ICinemachineCamera* incoming) const;

private:
    BlendDefinition defaultBlend_{BlendStyle::EaseInOut, 2.0f};
    std::vector<BlendRule> rules_;
    BlendOverride override_;
};

using BlendSettings = BlendLookup;
using BlenderSettings = BlendLookup;
using BlendLookupTable = BlendLookup;

} // namespace cinerig
