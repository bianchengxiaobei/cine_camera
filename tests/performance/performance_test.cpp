#include "allocation_counter.hpp"
#include "fake_entity_world.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include <cinerig/brain/camera_brain.hpp>
#include <cinerig/components/follow.hpp>
#include <cinerig/components/rotation_composer.hpp>
#include <cinerig/core/bezier_spline.hpp>
#include <cinerig/core/camera_state_blend.hpp>
#include <cinerig/runtime/camera_system.hpp>

namespace
{

using namespace cinerig;
using namespace cinerig::testing;

constexpr float kDeltaTime = 1.0f / 60.0f;
constexpr int kCameraCount = 64;

class NoopCameraOutput final : public ICameraOutput
{
public:
    void apply(const AppliedCamera& camera) override
    {
        applied = camera;
        ++applyCount;
    }

    LensSettings outputLens() const override
    {
        return LensSettings::Default;
    }

    float aspect() const override
    {
        return 16.0f / 9.0f;
    }

    AppliedCamera applied;
    int applyCount = 0;
};

class CameraFrameRig
{
public:
    explicit CameraFrameRig(StandbyUpdateMode standby)
        : system(world)
    {
        world.setFrame(UpdateClock::Late, 0);
        world.setDeltaTime(UpdateClock::Late, kDeltaTime);
        system.setUpdateMethod(UpdateMethod::Late);
        system.setUnitTestMode(true);

        BrainConfig config;
        config.updateMethod = UpdateMethod::Late;
        config.blendUpdateMethod = BrainConfig::BlendUpdateMethod::Late;
        config.defaultBlend = BlendDefinition{BlendStyle::Cut, 0.0f};
        brainId = system.createBrain(output, config);

        cameras.reserve(kCameraCount);
        for (int index = 0; index < kCameraCount; ++index)
        {
            const EntityId entity = 1000 + static_cast<EntityId>(index);
            world.add(entity, Transform{
                {static_cast<float>(index) * 0.25f, 0.0f, 0.0f},
                math::Quat::identity(),
                math::Vec3::one,
            });

            const CameraId id = system.createCamera<VirtualCamera>("camera");
            VirtualCamera* camera = system.camera(id);
            camera->priority = PrioritySettings(index == 0 ? 100 : 10);
            camera->standbyUpdate = standby;
            camera->targets.tracking = TargetRef::entity(entity);
            camera->lens.fieldOfView = 60.0f;

            auto& follow = camera->setBody<Follow>();
            follow.followOffset = {0.0f, 1.5f, -6.0f};
            follow.trackerSettings.positionDamping = math::Vec3::zero;

            auto& composer = camera->setAim<RotationComposer>();
            composer.damping = math::Vec2::zero;

            cameras.push_back(id);
        }
    }

    void step()
    {
        ++frame;
        world.setFrame(UpdateClock::Late, frame);
        system.lateUpdate(kDeltaTime);
    }

    void warmUp(int steps)
    {
        for (int step = 0; step < steps; ++step)
            this->step();
    }

    FakeEntityWorld world;
    NoopCameraOutput output;
    CameraSystem system;
    BrainId brainId = 0;
    std::vector<CameraId> cameras;
    std::uint64_t frame = 0;
};

struct TimingResult
{
    double medianNs = 0.0;
};

template <typename Function>
TimingResult measure(
    Function&& function,
    int warmupIterations,
    int iterationsPerSample,
    int samples)
{
    for (int iteration = 0; iteration < warmupIterations; ++iteration)
        function();

    std::array<double, 9> sampleNs{};
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto start = std::chrono::steady_clock::now();
        for (int iteration = 0; iteration < iterationsPerSample; ++iteration)
            function();
        const auto end = std::chrono::steady_clock::now();
        sampleNs[static_cast<std::size_t>(sample)] =
            std::chrono::duration<double, std::nano>(end - start).count()
            / static_cast<double>(iterationsPerSample);
    }

    std::sort(
        sampleNs.begin(),
        sampleNs.begin() + static_cast<std::ptrdiff_t>(samples));
    return TimingResult{sampleNs[static_cast<std::size_t>(samples / 2)]};
}

#ifdef NDEBUG
constexpr bool kEnforceZeroAllocations = true;
constexpr double kAlwaysFrameBudgetNs = 500'000.0;
constexpr double kRoundRobinFrameBudgetNs = 50'000.0;
constexpr double kStateLerpBudgetNs = 3'000.0;
constexpr double kSplineEvaluateBudgetNs = 500.0;
#else
constexpr bool kEnforceZeroAllocations = false;
constexpr double kAlwaysFrameBudgetNs = 10'000'000.0;
constexpr double kRoundRobinFrameBudgetNs = 1'000'000.0;
constexpr double kStateLerpBudgetNs = 30'000.0;
constexpr double kSplineEvaluateBudgetNs = 10'000.0;
#endif

void report(
    const char* name,
    TimingResult result,
    double budgetNs)
{
    std::printf(
        "[perf] %-28s median=%8.3f us  budget=%8.3f us\n",
        name,
        result.medianNs / 1000.0,
        budgetNs / 1000.0);
}

std::size_t updatedCameraCount(CameraFrameRig& rig)
{
    std::size_t updated = 0;
    for (CameraId id : rig.cameras)
    {
        const auto* status = rig.system.updateManager().status(rig.system.camera(id));
        if (status != nullptr
            && status->lastUpdateFrame == static_cast<std::int64_t>(rig.frame))
        {
            ++updated;
        }
    }
    return updated;
}

} // namespace

TEST_CASE(
    "Steady-state camera frames perform no heap allocations",
    "[performance][allocation]")
{
    std::size_t probeAllocations = 0;
    {
        cinerig::bench::AllocationScope scope;
        int* probe = new int(1);
        delete probe;
        probeAllocations = scope.finish();
    }
    REQUIRE(probeAllocations >= 1);

    CameraFrameRig rig(StandbyUpdateMode::Always);
    rig.warmUp(4);

    std::size_t allocations = 0;
    {
        cinerig::bench::AllocationScope scope;
        for (int frame = 0; frame < 30; ++frame)
            rig.step();
        allocations = scope.finish();
    }

    std::printf(
        "[perf] steady-state allocations: %zu%s\n",
        allocations,
        kEnforceZeroAllocations
            ? ""
            : " (Debug CRT container proxies; zero enforced in Release)");
    REQUIRE(rig.output.applyCount == 34);
    if constexpr (kEnforceZeroAllocations)
        REQUIRE(allocations == 0);
}

TEST_CASE(
    "RoundRobin refreshes the live camera and one standby camera",
    "[performance][standby]")
{
    CameraFrameRig rig(StandbyUpdateMode::RoundRobin);
    rig.step();
    rig.step();

    REQUIRE(updatedCameraCount(rig) == 2);
}

TEST_CASE(
    "A 64-camera Always frame stays within its budget",
    "[performance][budget][camera]")
{
    CameraFrameRig rig(StandbyUpdateMode::Always);
    const TimingResult timing = measure([&] { rig.step(); }, 4, 20, 5);

    report("64-camera Always frame", timing, kAlwaysFrameBudgetNs);
    REQUIRE(rig.output.applyCount > 0);
    REQUIRE(updatedCameraCount(rig) == kCameraCount);
    REQUIRE(timing.medianNs < kAlwaysFrameBudgetNs);
}

TEST_CASE(
    "A 64-camera RoundRobin frame stays within its budget",
    "[performance][budget][camera]")
{
    CameraFrameRig rig(StandbyUpdateMode::RoundRobin);
    const TimingResult timing = measure([&] { rig.step(); }, 4, 200, 5);

    report("64-camera RoundRobin frame", timing, kRoundRobinFrameBudgetNs);
    REQUIRE(rig.output.applyCount > 0);
    REQUIRE(updatedCameraCount(rig) == 2);
    REQUIRE(timing.medianNs < kRoundRobinFrameBudgetNs);
}

TEST_CASE(
    "CameraState lerp stays within its budget",
    "[performance][budget][state]")
{
    CameraState a = CameraState::makeDefault();
    CameraState b = CameraState::makeDefault();
    a.referenceLookAt = {0.0f, 1.0f, 0.0f};
    b.referenceLookAt = {2.0f, 1.0f, -3.0f};
    a.rawPosition = {0.0f, 2.0f, -5.0f};
    b.rawPosition = {2.0f, 3.0f, -8.0f};
    a.rawOrientation = math::Quat::identity();
    b.rawOrientation = math::quatFromAxisAngle(math::Vec3::up, 0.5f);
    a.lens.fieldOfView = 45.0f;
    b.lens.fieldOfView = 65.0f;
    a.shotQuality = 0.75f;
    b.shotQuality = 1.0f;

    volatile float sink = 0.0f;
    const TimingResult timing = measure(
        [&]
        {
            sink = cinerig::lerp(a, b, 0.5f).rawPosition.x;
        },
        100,
        20000,
        5);

    report("CameraState::lerp", timing, kStateLerpBudgetNs);
    REQUIRE(sink == 1.0f);
    REQUIRE(timing.medianNs < kStateLerpBudgetNs);
}

TEST_CASE(
    "BezierSpline evaluation stays within its budget",
    "[performance][budget][spline]")
{
    std::vector<math::Vec3> points;
    points.reserve(33);
    for (int index = 0; index < 33; ++index)
    {
        const float x = static_cast<float>(index);
        points.push_back({
            x,
            std::sin(x * 0.31f) * 2.0f,
            std::cos(x * 0.17f) * 2.0f,
        });
    }
    const BezierSpline spline = BezierSpline::smooth(points);

    volatile float sink = 0.0f;
    const TimingResult timing = measure(
        [&]
        {
            sink = spline.evaluatePosition(0.37f).x;
        },
        100,
        20000,
        5);

    report("BezierSpline::evaluatePosition", timing, kSplineEvaluateBudgetNs);
    REQUIRE(sink > 0.0f);
    REQUIRE(timing.medianNs < kSplineEvaluateBudgetNs);
}
