# Valgrind (Memcheck) Build

## What problem does Valgrind solve?

Memory bugs in C++ programs come in two broad flavours: the kind you can catch at compile time and the kind that only show up when the program runs with real data. **Valgrind's Memcheck tool** catches the runtime kind.

Valgrind intercepts every single memory operation your program performs — every `malloc`, `free`, every read, every write — and checks each one against its internal model of "valid" memory. The moment your code reads an uninitialised byte, accesses a buffer one element past its end, or dereferences a pointer after the object it pointed to has been freed, Valgrind stops and prints a precise report.

Memcheck detects:

- **Heap memory leaks** — memory that was allocated but never freed (both definitely lost and indirectly lost)
- **Use of uninitialised memory** — reading a variable or buffer before it has been written to
- **Invalid reads / writes** — accessing memory outside the bounds of an allocated block (heap, stack, or global)
- **Use-after-free** — reading or writing memory after it has been returned to the allocator
- **Double-free / mismatched `free`** — freeing the same pointer twice or freeing a non-heap pointer

---

## How Valgrind differs from ASan and TSan

This is the most important concept to understand before looking at the implementation.

**ASan and TSan are compiler-based sanitizers.** They work by rewriting your binary at compile time. The compiler inserts extra instructions — shadow memory checks, lock acquisition hooks — directly into every function. The instrumented binary carries the detection logic inside itself. Consequence: you must compile with special flags, and the result is a different binary from the Release build.

**Valgrind is a dynamic binary analysis tool.** It does not touch the compiler at all. Instead, it runs your existing binary inside a virtual machine called VEX. The VM translates every machine instruction your program would execute into an instrumented equivalent that Valgrind can supervise. The original binary is never modified. You could, in principle, run Valgrind against a binary you downloaded from the internet.

The practical consequence is:

| | ASan / TSan | Valgrind |
|---|---|---|
| Requires special compiler flags | Yes (`-fsanitize=...`) | No |
| Requires special linker flags | Yes | No |
| Works on pre-compiled binaries | No | Yes |
| Compile flags that help | `-fsanitize=...` mandatory | `-g -fno-omit-frame-pointer` recommended |
| Performance overhead | 2–20x | 10–50x |

Valgrind is slower precisely because it interprets every instruction. ASan inserts checks inline and the CPU executes them natively — much faster. For this reason, ASan is the first choice for CI memory checks, and Valgrind is the deeper audit for when you suspect a subtle bug that ASan missed.

---

## Why a separate Conan profile?

Valgrind works on any binary. In theory you could run it against the Release build at `build/`. In practice, a Release build compiled with `-O2` or `-O3` has had most of its variables inlined, registers reused, and loop bodies unrolled — Valgrind can still find errors, but the stack trace in the report will point to an optimised instruction that bears little resemblance to the source code.

The dedicated Valgrind build is compiled with:

```
-g                    → embed debug symbols (file names, line numbers, variable names)
-fno-omit-frame-pointer → preserve stack frames for accurate unwinding
```

Combined, these flags make every Valgrind report immediately actionable: you see the exact source line where the error occurred without any guesswork.


These profiles are shared between the Valgrind and unit-gcov builds. Both pipelines require a plain Release-mode Conan install with no sanitizer flags, so a single profile file covers both. The Makefile keeps `VALGRIND_TOOLCHAIN` and `UNIT_GCOV_TOOLCHAIN` as separate variables (pointing into their respective build directories) even though they reference the same profile, because the two builds remain independently configurable.

The generators are isolated in `/test-builds/build-valgrind/` using `conan install --output-folder` so they do not conflict with the Release, TSan, or ASan builds.

---

## Why a separate CMake build directory?

Each instrumented build (Release, TSan, ASan, Valgrind) produces object files compiled with different flags. Mixing them in `build/` would corrupt incremental builds. Dedicated directories let all four builds coexist independently:

| Build | Directory | Purpose |
|---|---|---|
| Release | `production-builds/release/` | Production binary, optimised |
| TSan | `/test-builds/build-tsan/` | Race detection |
| ASan | `/test-builds/build-asan/` | Memory error detection (compiler-based) |
| Valgrind | `/test-builds/build-valgrind/` | Memory error detection (dynamic) |
| unit-gcov | `/test-builds/build-unit/` | Unit tests + gcov coverage |

---

## How CTest integrates with Valgrind

CTest has first-class support for Valgrind through its **MemCheck** dashboard mode. When you run:

```bash
ctest --test-dir /test-builds/build-valgrind -T memcheck
```

CTest looks up the `MEMORYCHECK_COMMAND` variable (set at CMake configure time) and prepends it to every test invocation. Instead of running:

```bash
./adapter_iptc_tests
```

it runs:

```bash
valgrind --leak-check=full --error-exitcode=1 ... ./adapter_iptc_tests
```

The `--error-exitcode=1` flag is critical: it tells Valgrind to exit with code 1 when it finds any error. CTest treats a non-zero exit code as a test failure, which causes `make test-valgrind` to fail and surfaces the issue in CI.

The flags passed via `MEMORYCHECK_COMMAND_OPTIONS` at configure time:

| Flag | Role |
|---|---|
| `--leak-check=full` | Report every individual leak, not just a summary count |
| `--error-exitcode=1` | Non-zero exit on any error, so CTest marks the test as failed |
| `--show-leak-kinds=all` | Report definitely lost, indirectly lost, possibly lost, and still reachable |
| `--track-origins=yes` | When an uninitialised-value error occurs, trace back to where the uninitialised memory was allocated |
| `--num-callers=20` | Show 20 stack frames in each report (default is 12, which is often not enough to see the application code through library wrappers) |

---

## Files added

| File | Description |
|---|---|
| `profiles/gcc13.conan` | Shared profile for Valgrind and unit-gcov builds (`build_type=Release`, GCC 13) |
| `sandbox/Dockerfile` | `valgrind` added to `apt-get install` |
| `Makefile` | `VALGRIND_BUILD_DIR`/`VALGRIND_TOOLCHAIN`/`UNIT_GCOV_BUILD_DIR`/`UNIT_GCOV_TOOLCHAIN` variables + Valgrind and unit-gcov targets |
| `.gitignore` | `production-builds/` excluded (covers all production build output) |

---

## Makefile targets

| Target | Description |
|---|---|
| `make conan-install-valgrind` | Install deps with Release build type into `/test-builds/build-valgrind/` |
| `make configure-valgrind` | CMake configure with `-g -fno-omit-frame-pointer` and `MEMORYCHECK_COMMAND=valgrind` |
| `make build-valgrind` | Compile |
| `make rebuild-valgrind` | `clean-valgrind` + `configure-valgrind` + `build-valgrind` |
| `make test-valgrind` | Run all tests under Valgrind memcheck |
| `make test-one-valgrind PATTERN=<p>` | Run a single test by pattern under Valgrind |
| `make clean-valgrind` | Remove `/test-builds/build-valgrind/` without touching other build directories |

---

## How to run a Valgrind build

```bash
make configure-valgrind   # conan install (Release) + cmake configure with debug symbols
make build-valgrind       # compile
make test-valgrind        # run tests under Valgrind; any error fails the target


# Clean only the Valgrind build (leaves Release, TSan, and ASan untouched)
make clean-valgrind
```

---

## Important lessons

These are real issues hit during the initial setup of this Valgrind build. They are common enough to be worth documenting.

### Bug 1 — `DartConfiguration.tcl` not found when using `ctest -T memcheck`

**Symptom:**
```
Cannot find file: /test-builds/build-valgrind/DartConfiguration.tcl
Memory checker (MemoryCheckCommand) not set, or cannot find the specified program.
Memory check project /test-builds/build-valgrind
```

**What happened:** `ctest -T memcheck` is a CTest *dashboard* mode. Dashboard modes require a `DartConfiguration.tcl` file in the build directory, which records project metadata and the memory checker command. This file is only generated when CMakeLists.txt calls `include(CTest)`. The project was relying on an implicit test setup — neither `include(CTest)` nor `enable_testing()` was present — so the file was never created and CTest could not locate Valgrind even though `-DMEMORYCHECK_COMMAND=valgrind` had been passed at configure time.

**Why it matters:** Passing `-DMEMORYCHECK_COMMAND=valgrind` on the `cmake` command line writes the value into the CMake cache, but `ctest -T memcheck` reads the memory checker path from `DartConfiguration.tcl`, not directly from the cache. Without `include(CTest)`, the file does not exist, and the cache entry is never transferred to it.

**Fix:** Add `include(CTest)` to the root `CMakeLists.txt`, before any `add_subdirectory` calls:

```cmake
include(CTest)
```

`include(CTest)` does two things that `enable_testing()` alone does not:

1. It calls `enable_testing()` internally, registering the project with CTest.
2. It generates `DartConfiguration.tcl` in the build directory, which CTest dashboard modes (including `-T memcheck`) read to find the memory checker, project name, and other metadata.

This change affects all builds (Release, TSan, ASan, Valgrind), not just Valgrind. It is a safe, additive change — the basic `ctest --output-on-failure` workflow used by the other test targets is unaffected.

### Bug 2 — `MEMORYCHECK_COMMAND` must not be set explicitly with a bare name

**Symptom:**
```
Memory checker (MemoryCheckCommand) not set, or cannot find the specified program.
Memory check project /workspace/build-valgrind
```

**What happened:** The original `configure-valgrind` target passed `-DMEMORYCHECK_COMMAND=valgrind` to CMake. When this cache entry is pre-populated with a bare name (`valgrind` instead of `/usr/bin/valgrind`), CMake's `include(CTest)` module calls `find_program(MEMORYCHECK_COMMAND ...)` — but `find_program` honours an existing cache entry and skips the filesystem search, leaving the value as the literal string `valgrind`. CTest then writes that bare name into `DartConfiguration.tcl` and later fails to execute it because it expects an absolute path.

**Fix:** Remove the explicit `-DMEMORYCHECK_COMMAND=valgrind` flag from the `cmake` command line. With no pre-existing cache entry, `include(CTest)` runs `find_program` freely and resolves valgrind to its absolute path (e.g. `/usr/bin/valgrind`), which CTest can then use correctly. `MEMORYCHECK_COMMAND_OPTIONS` is still passed explicitly via `-D` because `include(CTest)` does not auto-detect options.

---

## What a Valgrind error report looks like

### Heap memory leak

```
==12345== LEAK SUMMARY:
==12345==    definitely lost: 48 bytes in 1 blocks
==12345==      indirectly lost: 0 bytes in 0 blocks
==12345==        possibly lost: 0 bytes in 0 blocks
==12345==   still reachable: 0 bytes in 0 blocks
==12345==        suppressed: 0 bytes in 0 blocks
==12345==
==12345== 48 bytes in 1 blocks are definitely lost in loss record 1 of 1
==12345==    at 0x... operator new(unsigned long) (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x... MyClass::MyClass() (adapter_iptc/src/my_class.cpp:12)
==12345==    by 0x... main (main.cpp:8)
```

"Definitely lost" means there is no live pointer pointing to that memory anywhere in the program. It is a real leak. "Still reachable" means memory that was not freed before `main` returned but still had a live pointer — not always a bug, but worth reviewing.

### Invalid read (out-of-bounds)

```
==12345== Invalid read of size 4
==12345==    at 0x... MyClass::process() (adapter_iptc/src/my_class.cpp:58)
==12345==    by 0x... main (main.cpp:23)
==12345==  Address 0x... is 0 bytes after a block of size 16 alloc'd
==12345==    at 0x... operator new[](unsigned long)
==12345==    by 0x... MyClass::MyClass() (adapter_iptc/src/my_class.cpp:10)
```

"0 bytes after a block of size 16" means the access was exactly one past the end — a classic off-by-one error.

### Use of uninitialised value

```
==12345== Use of uninitialised value of size 8
==12345==    at 0x... MyClass::compute() (adapter_iptc/src/my_class.cpp:34)
==12345==    by 0x... main (main.cpp:15)
==12345==  Uninitialised value was created by a heap allocation
==12345==    at 0x... operator new (in /usr/lib/valgrind/...)
==12345==    by 0x... MyClass::MyClass() (adapter_iptc/src/my_class.cpp:8)
```

`--track-origins=yes` is what adds the "Uninitialised value was created by..." line. Without it, you only see where the value was used, not where it came from.

---

## Performance overhead

Valgrind is the heaviest analysis tool in the toolchain. Expect roughly:

- **10–50x** slowdown in CPU time (every instruction is interpreted, not executed natively)
- **1.5–2x** increase in memory usage (Valgrind's shadow memory model is more compact than ASan's)

This overhead means Valgrind is not a candidate for fast CI feedback loops. The recommended workflow is:

1. **ASan** runs on every pull request — fast, catches most memory bugs at compile time
2. **Valgrind** runs as a deeper audit — either on a schedule, manually when investigating a specific bug, or before significant releases

Unlike ASan and TSan, Valgrind and ASan *can* be used on the same codebase (just not on the same binary at the same time), because Valgrind is not a compiler flag and does not conflict with ASan's shadow memory at the compilation level.

---

## BUILD_TESTING and the production build split

### What `include(CTest)` introduces

Adding `include(CTest)` to the root `CMakeLists.txt` (required for `ctest -T memcheck`) brings in a CMake option called `BUILD_TESTING`, default `ON`. Its intended contract is: set it to `OFF` and test targets are skipped. The critical detail is that **this contract only holds if every `add_subdirectory` containing tests is guarded**:

```cmake
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
```

Without that guard, `-DBUILD_TESTING=OFF` suppresses `add_test()` registration (so `ctest` won't list the tests) but does **nothing** to stop CMake from compiling and linking the test executables. The flag silently lies — you get test binaries in your build directory even though you asked for none.

---

### The build split: Release vs test builds

This project enforces a deliberate separation:

| Build | Directory | `BUILD_TESTING` | Purpose |
|---|---|---|---|
| Release | `production-builds/release/` | `OFF` — explicit `-DBUILD_TESTING=OFF` | Production binary, no test artefacts |
| TSan | `/test-builds/build-tsan/` | `ON` (default) | Test build — race detection |
| ASan | `/test-builds/build-asan/` | `ON` (default) | Test build — memory error detection |
| Valgrind | `/test-builds/build-valgrind/` | `ON` (default) | Test build — dynamic memory analysis |

The Release build is the artifact that would be deployed or shipped as a library. It must be clean — no test binaries, no gtest, no mock objects, no test object files. Everything related to testing lives in the dedicated test builds.

---

### Impact analysis: what happens without the guard

#### Build time — real and measurable

When gtest unit tests are added without the `if(BUILD_TESTING)` guard, a user building with `-DBUILD_TESTING=OFF` still compiles:

- Every test `.cpp` file in `adapter_iptc/test/` and `adapter_socket/test/`
- gtest itself — internally around 400–600 source files, taking 1–3 minutes on a clean build even with Ninja
- Every mock object and test fixture

For a developer or CI job building a production artifact that does not need tests, this is pure wasted time on every clean build.

#### Disk space — bounded but real

Each unguarded test suite adds roughly:

| Artefact | Approximate size |
|---|---|
| `libgtest.a` | 3–8 MB depending on optimisation |
| Test object files | Hundreds of KB per test file |
| Test executables | 5–20 MB each (statically linked with gtest) |

For a project with, say, 10 test binaries across the libraries, this is 50–200 MB of extra artefacts in `build/` on every Release build. In the Docker sandbox where `production-builds/` is a bind mount, that consumption lands on the developer's host filesystem.

#### Library consumer correctness — the most important impact

`adapter_iptc` is meant to move out as a standalone library ([adapter-iptc-placement.md](../adapter-iptc-placement.md)). When a consumer does:

```cmake
add_subdirectory(third_party/adapter_iptc)
```

CMake processes the library's `CMakeLists.txt` in the consumer's build context. Without the `if(BUILD_TESTING)` guard, the consumer's build compiles all of the library's tests and pulls in gtest as a transitive dependency — even though the consumer has no use for either. They have no clean way to opt out.

This directly violates the library contract. The guard is what makes embedding work correctly.

#### Security — low but project-specific

`bellerotables-helper` requires `NET_ADMIN` and `NET_RAW` capabilities to manipulate kernel firewall tables. Test binaries that exercise `adapter_iptc` will call into `libiptc` — they are, effectively, privileged iptables clients. If test binaries end up in a production deployment and are executed with those capabilities outside the sandbox, they can manipulate the host's netfilter rules.

Test binaries do not have install rules and normally never leave the build directory, so this risk is bounded. But it reinforces that the Release build should be genuinely test-free.

---

### The fix is one line per subdirectory

Each library's `CMakeLists.txt` includes its own `test/` subdirectory, always guarded:

```cmake
if(BUILD_TESTING)
    add_subdirectory(test)
endif()
```

This makes `-DBUILD_TESTING=OFF` honest and ensures the Release build is clean. Every new library follows the same shape as `adapter_iptc` and `adapter_socket`.

---

## Suppression files

Some libraries produce Valgrind false positives — most notably the protobuf runtime (arriving with the parser), which intentionally leaves some memory reachable at process exit as a performance optimisation. If Valgrind reports are polluted with noise from third-party code, a suppression file can filter them out.

To generate suppressions from an existing run:

```bash
valgrind --gen-suppressions=all --leak-check=full ./your_test 2>&1 | grep -A 20 "^{" > my.supp
```

Then pass `--suppressions=my.supp` via `MEMORYCHECK_COMMAND_OPTIONS`. Commit the suppression file to the repository alongside the Valgrind profile.
