# gcov Coverage Build

## What problem does gcov solve?

Unit tests tell you whether your code behaves correctly for the inputs you tested. **gcov** tells you which lines, branches, and functions your tests actually reached. Without coverage data, a green test suite can hide entire code paths that were never exercised.

gcov reports three metrics:

- **Line coverage** — which source lines were executed at least once
- **Branch coverage** — which conditional branches (if/else, switch, ternary) were taken
- **Function coverage** — which functions were called at all

A line that was never executed cannot have been tested. Low coverage is a signal to write more tests or to delete dead code.

---

## Why merge with unit tests?

Coverage data is produced by running the instrumented binary — the same binary that runs the unit tests. There is no separate "coverage run" step. The test executable, compiled with coverage flags, writes `.gcda` data files as it runs.

Merging coverage into the unit test build (`-unit`) means a single `make test-unit` run produces both:
- CTest pass/fail results (which tests passed or failed)
- `.gcda` files in the build directory (which code paths were hit)

This is preferable to a separate build because:
1. One pass in pre-commit instead of two
2. The combined output (test results + coverage) gives a complete picture for code review
3. Running tests twice with different binaries introduces inconsistency

---

## How it works

### At compile time

The `-fprofile-arcs -ftest-coverage` flags (passed via `ENABLE_COVERAGE=ON`) instruct GCC to:
1. Insert counters at every arc (edge) in the control-flow graph of each function
2. Write a `.gcno` file alongside each object file — a static map from counter IDs to source lines

`.gcno` files are written once at compile time and do not change between runs.

### At runtime

When the instrumented test binary executes, GCC's coverage runtime increments the arc counters. On program exit, the runtime writes `.gcda` files into the build directory — one per source file — containing the counter values for that run.

`.gcda` files accumulate across multiple runs (counters are added, not reset). To get clean single-run coverage, delete all `.gcda` files and do a fresh `make test-unit`.

### Reading raw output

To inspect coverage for a single file, run inside the sandbox:

```bash
gcov -o /test-builds/build-unit/CMakeFiles/bellerotables-helper.dir/app <source-file>.cpp
```

gcov prints a summary to stdout and writes an annotated `.cpp.gcov` file showing hit counts per line. Lines with `#####` were never executed. Lines with `-` are not executable (comments, blank lines, declarations).

Note: lcov HTML report generation is covered in issue #4. The `.gcda` files produced here are the input for that step.

---

## Why `ENABLE_COVERAGE` is guarded by `BUILD_TESTING`

The `CMakeLists.txt` guard:

```cmake
if(ENABLE_COVERAGE AND BUILD_TESTING)
    string(APPEND CMAKE_CXX_FLAGS " -fprofile-arcs -ftest-coverage")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " --coverage")
endif()
```

The production build passes `-DBUILD_TESTING=OFF`. Even if `ENABLE_COVERAGE=ON` were accidentally passed alongside it, no coverage flags would be applied to the production binary. Coverage instrumentation adds runtime overhead and writes data files to disk — behaviour that must never appear in a shipped binary.

If you pass `-DENABLE_COVERAGE=ON -DBUILD_TESTING=OFF`, CMake emits a warning:

```
CMake Warning at CMakeLists.txt:N (message):
  ENABLE_COVERAGE=ON has no effect when BUILD_TESTING=OFF.
  Coverage instrumentation will not be applied.
```

---

## Why `Release` build_type is used

The Conan profile uses `build_type=Release`. This is not a performance choice — it is a dependency constraint. The Docker sandbox image pre-compiles the Conan dependencies during image build using `build_type=Release`; a Debug profile forces a rebuild from source, and in `piiptables` it broke protobuf, whose `protobuf/3.21.12` recipe does not provide the `protocd` Debug library.

The coverage instrumentation flags are applied at the CMake level (not inside the Conan profile), so the pre-cached dependencies are reused unchanged.

---

## Performance overhead

Expect approximately:

- **~2x** slowdown in test execution (counter increments on every arc)
- Minor increase in binary size (counter arrays + `.gcno` data embedded)

This is lighter than TSan (5–15x) and Valgrind (10–50x). Coverage is suitable as the standard unit test mode in pre-commit and CI.

---

## Files added

| File | Description |
|---|---|
| `profiles/gcc13.conan` | Shared profile for Valgrind and unit-gcov builds (GCC 13) |

## Files modified

| File | Change |
|---|---|
| `CMakeLists.txt` | Added `ENABLE_COVERAGE` option with `BUILD_TESTING` guard |
| `Makefile` | Renamed all `-unit` targets to `-unit`; added `-DENABLE_COVERAGE=ON` to configure targets |
| `.githooks/pre-commit` | Updated to call `-unit` targets |

---

## Makefile targets

| Target | Description |
|---|---|
| `make conan-install-unit` | Install deps into `/test-builds/build-unit/` |
| `make configure-unit` | CMake configure with `BUILD_TESTING=ON` and `ENABLE_COVERAGE=ON` |
| `make build-unit` | Compile tests + main binary with coverage instrumentation |
| `make rebuild-unit` | `clean-unit` + `configure-unit` + `build-unit` |
| `make test-unit` | Run all unit tests; writes `.gcda` coverage files |
| `make test-one-unit PATTERN=<p>` | Run a single test by pattern |
| `make clean-unit` | Remove `/test-builds/build-unit/` |
