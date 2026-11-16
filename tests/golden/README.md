# Golden Tests

Golden tests compare a deterministic JSON snapshot against a checked-in
baseline. The helper lives in `compare.hpp` and is header-only.

## Format

The snapshot is ordinary JSON. The comparison helper understands objects,
arrays, strings, booleans, numbers, and null. Object key order does not matter.
Generated files use:

- LF line endings;
- two-space indentation;
- fixed six-decimal numbers (`1.000000`);
- a trailing newline.

A typical frame snapshot:

```json
{
  "scenario": "priority_switch",
  "frames": [
    {
      "frame": 0,
      "activeCamera": "wide",
      "state": {
        "position": [0.0, 2.0, -6.0],
        "orientation": [0.0, 0.0, 0.0, 1.0],
        "fieldOfView": 40.0,
        "dutch": 0.0
      }
    }
  ]
}
```

## Comparison

```cpp
#include "compare.hpp"

using namespace cinerig::testing;

GoldenResult result = compareGoldenJson(expectedJson, actualJson);
REQUIRE(result.ok);

GoldenResult fileResult = compareGoldenFile(actualPath, goldenPath);
```

Default tolerance is `1e-4` for ordinary numbers. `GoldenTolerance` also has
separate `position` and `rotation` tolerances; paths containing `position`,
`rotation`, or `orientation` use them. `relative` is an additional tolerance
proportional to the larger absolute value.

Failures report the JSON path, the expected and actual values, and the applied
tolerance, for example:

```text
number mismatch at $.state.position[1]: expected 2.000000, actual 2.100000 (tolerance 0.000100)
```

## Updating Snapshots

Generation mode is explicit. Either pass `update = true` to
`compareGoldenFile`, or set the environment variable:

```text
CINERIG_UPDATE_GOLDEN=1
```

With the environment variable set, `ctest` rewrites the golden files that are
actually compared. Without it, golden tests only compare and never modify
tracked files. The update helper normalizes line endings and number formatting.

## Rules

- Scenario tests must use `FixedStep` or an explicit `dt`; never use system
  time.
- Golden files should be stable across platforms. Avoid locale-dependent text
  and unordered containers.
- Keep camera scenarios deterministic: same input, same frame count, same
  result.
- Run `ctest -L golden` for just the golden target.
