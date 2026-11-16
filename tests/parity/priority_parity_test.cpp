#include "brain_test_support.hpp"

#include <climits>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using namespace cinerig;
using namespace cinerig::testing;

namespace
{

BrainConfig cutConfig()
{
    BrainConfig config;
    config.updateMethod = UpdateMethod::Late;
    config.defaultBlend = BlendDefinition{BlendStyle::Cut, 0.0f};
    return config;
}

void checkPriorityOrder(const int* priorities, std::size_t count)
{
    BrainTestRig rig(cutConfig());
    std::vector<CameraId> ids;
    ids.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
        ids.push_back(rig.addCamera("priority", priorities[i], static_cast<float>(i) * 10.0f));

    rig.step(0.0f);

    for (std::size_t i = 0; i < count; ++i)
    {
        REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(ids[i]));
        REQUIRE(rig.system.setCameraEnabled(ids[i], false));
        rig.step(0.0f);
    }

    for (CameraId id : ids)
        REQUIRE(rig.system.setCameraEnabled(id, true));

    rig.step(0.0f);
    REQUIRE(rig.brain->activeVirtualCamera() == &rig.camera(ids.front()));
}

} // namespace

TEST_CASE("Active camera priority matches the reference standard ordering", "[parity][priority]")
{
    const int priorities[] = {40, 30, 20, 10, 0, -10, -20, -30, -40};
    checkPriorityOrder(priorities, sizeof(priorities) / sizeof(priorities[0]));
}

TEST_CASE("Active camera priority matches the reference integer limits", "[parity][priority]")
{
    const int priorities[] = {INT_MAX, INT_MAX / 2, 0, INT_MIN / 2, INT_MIN};
    checkPriorityOrder(priorities, sizeof(priorities) / sizeof(priorities[0]));
}
