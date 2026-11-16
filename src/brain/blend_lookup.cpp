#include <cinerig/brain/blend_lookup.hpp>

#include <utility>

namespace cinerig
{

BlendLookup::BlendLookup(const BlendDefinition& defaultBlend)
    : defaultBlend_(defaultBlend)
{
}

void BlendLookup::setDefaultBlend(const BlendDefinition& blend)
{
    defaultBlend_ = blend;
}

const BlendDefinition& BlendLookup::defaultBlend() const noexcept
{
    return defaultBlend_;
}

void BlendLookup::addRule(BlendRule rule)
{
    rules_.push_back(std::move(rule));
}

void BlendLookup::setRules(std::vector<BlendRule> rules)
{
    rules_ = std::move(rules);
}

void BlendLookup::clearRules()
{
    rules_.clear();
}

const std::vector<BlendRule>& BlendLookup::rules() const noexcept
{
    return rules_;
}

void BlendLookup::setOverride(BlendOverride override)
{
    override_ = std::move(override);
}

void BlendLookup::clearOverride()
{
    override_ = {};
}

bool BlendLookup::hasOverride() const noexcept
{
    return static_cast<bool>(override_);
}

BlendDefinition BlendLookup::lookup(
    const ICinemachineCamera* outgoing,
    const ICinemachineCamera* incoming) const
{
    const std::string from = outgoing != nullptr ? std::string(outgoing->name()) : std::string{};
    const std::string to = incoming != nullptr ? std::string(incoming->name()) : std::string{};
    const std::string anyLabel(kAnyCameraLabel);

    BlendDefinition defaultBlend = defaultBlend_;
    bool gotAnyToMe = false;
    bool gotMeToAny = false;
    BlendDefinition anyToMe = defaultBlend;
    BlendDefinition meToAny = defaultBlend;

    for (const BlendRule& rule : rules_)
    {
        if (rule.from == from && rule.to == to)
            return rule.blend;

        if (rule.from == anyLabel)
        {
            if (!to.empty() && rule.to == to)
            {
                if (!gotAnyToMe)
                    anyToMe = rule.blend;
                gotAnyToMe = true;
            }
            else if (rule.to == anyLabel)
            {
                defaultBlend = rule.blend;
            }
        }
        else if (rule.to == anyLabel && !from.empty() && rule.from == from)
        {
            if (!gotMeToAny)
                meToAny = rule.blend;
            gotMeToAny = true;
        }
    }

    if (gotAnyToMe)
        defaultBlend = anyToMe;
    else if (gotMeToAny)
        defaultBlend = meToAny;

    if (override_)
        return override_(outgoing, incoming, defaultBlend);
    return defaultBlend;
}

} // namespace cinerig
