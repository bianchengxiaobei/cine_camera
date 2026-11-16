#include "compare.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using namespace cinerig::testing;

namespace
{

class TempGoldenDirectory
{
public:
    explicit TempGoldenDirectory(std::string_view name)
        : path(std::filesystem::temp_directory_path() / ("cinerig_p0_06_golden_" + std::string(name)))
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path, error);
    }

    ~TempGoldenDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

std::string readFile(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>());
}

}

TEST_CASE("Golden JSON comparison accepts exact and tolerance values", "[golden]")
{
    const std::string expected = R"({
        "scenario": "priority_switch",
        "frames": [{
            "frame": 0,
            "state": {
                "position": [0.0, 2.0, -6.0],
                "orientation": [0.0, 0.0, 0.0, 1.0],
                "fieldOfView": 40.0
            }
        }]
    })";
    const std::string actual = R"({
        "scenario": "priority_switch",
        "frames": [{
            "state": {
                "fieldOfView": 40.00005,
                "orientation": [0.0, 0.0, 0.0, 1.0],
                "position": [0.00005, 2.0, -6.0]
            },
            "frame": 0
        }]
    })";

    const GoldenResult result = compareGoldenJson(expected, actual);
    REQUIRE(result.ok);
    REQUIRE_FALSE(result.updated);

    const GoldenResult mismatch = compareGoldenJson(
        R"({"state":{"position":[0.0,2.0,-6.0]}})",
        R"({"state":{"position":[0.0,2.1,-6.0]}})");
    REQUIRE_FALSE(mismatch.ok);
    REQUIRE(mismatch.message.find("$.state.position[1]") != std::string::npos);
    REQUIRE(mismatch.message.find("expected 2.000000") != std::string::npos);
    REQUIRE(mismatch.message.find("actual 2.100000") != std::string::npos);
}

TEST_CASE("Golden comparison supports separate position and rotation tolerances", "[golden]")
{
    GoldenTolerance tolerance;
    tolerance.absolute = 0.0;
    tolerance.position = 0.01;
    tolerance.rotation = 0.001;

    const GoldenResult positionOnly = compareGoldenJson(
        R"({"state":{"position":[0.0,1.0,0.0]}})",
        R"({"state":{"position":[0.005,1.0,0.0]}})",
        tolerance);
    REQUIRE(positionOnly.ok);

    const GoldenResult rotationFailure = compareGoldenJson(
        R"({"state":{"orientation":[0.0,0.0,0.0,1.0]}})",
        R"({"state":{"orientation":[0.0,0.0,0.0,1.005]}})",
        tolerance);
    REQUIRE_FALSE(rotationFailure.ok);
    REQUIRE(rotationFailure.message.find("state.orientation[3]") != std::string::npos);
}

TEST_CASE("Golden files handle missing snapshots and explicit update mode", "[golden]")
{
    const TempGoldenDirectory directory("files");
    const std::filesystem::path actualPath = directory.path / "actual.json";
    const std::filesystem::path missingPath = directory.path / "missing.json";
    const std::filesystem::path goldenPath = directory.path / "golden.json";

    const GoldenResult writeActual = writeGoldenFile(
        actualPath,
        R"({"scenario":"priority_switch","value":1.0})");
    REQUIRE(writeActual.ok);

    const GoldenResult missing = compareGoldenFile(actualPath, missingPath);
    REQUIRE_FALSE(missing.ok);
    REQUIRE(missing.message.find("golden file missing") != std::string::npos);

    const GoldenResult updated = compareGoldenFile(actualPath, goldenPath, {}, true);
    REQUIRE(updated.ok);
    REQUIRE(updated.updated);

    const std::string generated = readFile(goldenPath);
    REQUIRE(generated.find("\"value\": 1.000000") != std::string::npos);
    REQUIRE(generated.find('\r') == std::string::npos);
    REQUIRE_FALSE(generated.empty());
    REQUIRE(generated.back() == '\n');

    const GoldenResult compared = compareGoldenFile(actualPath, goldenPath);
    REQUIRE(compared.ok);
    REQUIRE_FALSE(compared.updated);
}

TEST_CASE("Golden writer normalizes newlines and rejects invalid JSON", "[golden]")
{
    const TempGoldenDirectory directory("writer");
    const std::filesystem::path path = directory.path / "snapshot.json";

    const GoldenResult invalid = writeGoldenFile(path, "{not json");
    REQUIRE_FALSE(invalid.ok);
    REQUIRE(invalid.message.find("parse failed") != std::string::npos);

    const GoldenResult valid = writeGoldenFile(path, "{\r\n\"value\": 2\r\n}");
    REQUIRE(valid.ok);
    REQUIRE(valid.updated);

    const std::string generated = readFile(path);
    REQUIRE(generated.find("\"value\": 2.000000") != std::string::npos);
    REQUIRE(generated.find('\r') == std::string::npos);
    REQUIRE(generated.back() == '\n');
}

TEST_CASE("Golden formatter keeps six decimal places", "[golden]")
{
    REQUIRE(formatGoldenNumber(1.0) == "1.000000");
    REQUIRE(formatGoldenNumber(-0.0) == "0.000000");
    REQUIRE(formatGoldenNumber(1.2345678) == "1.234568");
}

TEST_CASE("Golden update mode requires the explicit environment switch", "[golden]")
{
    const char* previous = std::getenv("CINERIG_UPDATE_GOLDEN");
    const std::string saved = previous == nullptr ? std::string{} : std::string(previous);

    const auto setEnvironment = [](const char* value)
    {
#if defined(_WIN32)
        _putenv_s("CINERIG_UPDATE_GOLDEN", value);
#else
        setenv("CINERIG_UPDATE_GOLDEN", value, 1);
#endif
    };
    const auto clearEnvironment = []()
    {
#if defined(_WIN32)
        _putenv_s("CINERIG_UPDATE_GOLDEN", "");
#else
        unsetenv("CINERIG_UPDATE_GOLDEN");
#endif
    };

    clearEnvironment();
    REQUIRE_FALSE(updateGoldenRequested());

    setEnvironment("1");
    REQUIRE(updateGoldenRequested());

    setEnvironment("0");
    REQUIRE_FALSE(updateGoldenRequested());

    if (previous != nullptr)
        setEnvironment(saved.c_str());
    else
        clearEnvironment();
}
