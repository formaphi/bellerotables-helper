# ThreadSanitizer (TSan) Build

## What problem does TSan solve?

Imagine two threads in your program both touch the same variable. Thread A reads it while Thread B is writing it — no lock, no `std::atomic`. This is a **data race**. The result is undefined behaviour: sometimes you get stale data, sometimes a crash, sometimes nothing visible at all. These bugs are nearly impossible to reproduce because they depend on exact CPU scheduling.

**ThreadSanitizer** is a tool that makes races reliably visible. It instruments your binary at compile time so that every memory read/write and every synchronization operation (mutex lock/unlock, `std::atomic` operations) is recorded in a shadow memory. When two conflicting accesses happen in the wrong order, TSan prints a detailed report with both stack traces.

---

## Why a separate Conan profile?

The project already has:
- `profiles/gcc13.conan` → `build_type=Release`, GCC 13


1. **Reuse the pre-cached dependencies.** The Docker image pre-compiles the Conan dependencies (gtest today, protobuf once the parser lands) in Release mode. A Debug profile forces a rebuild from source; in `piiptables` it also broke protobuf, whose `protobuf/3.21.12` recipe does not provide the `d`-suffixed `protocd` library.

2. **TSan only needs to instrument your code, not dependencies.** Races in dependencies are not your problem. Applying TSan flags selectively via `CMAKE_CXX_FLAGS` achieves the same safety goal without the overhead of rebuilding third-party libraries.

The generators are isolated in a separate output folder (`/test-builds/build-tsan/`) using `conan install --output-folder`, so they do not conflict with the Release build's generators in `production-builds/release/`.

---

## Why a separate CMake build directory?

The Release binary at `production-builds/release/` is compiled without any instrumentation — it's fast, stripped, optimized. The TSan binary needs different object files, different linking, different layout. If they shared the same directory, every switch would trigger a complete rebuild.

By pointing CMake at `/test-builds/build-tsan/` (`-B $(TSAN_BUILD_DIR)`), both builds live independently side by side.

---

## What the compiler and linker flags do

```
-DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
```

| Flag | Role |
|---|---|
| `-fsanitize=thread` | Tells GCC to insert instrumentation code around every memory access. The compiler rewrites your reads/writes into calls that check the shadow memory. |
| `-fno-omit-frame-pointer` | By default the compiler recycles the frame pointer register for speed. This flag preserves it so that stack unwinders (and TSan's report printer) can walk the full call chain. Without this, race reports often show truncated or garbled stacks. |
| `-g` | Embeds debug symbols — file names, line numbers, variable names. Debug build type already implies this, but being explicit is self-documenting. |
| Linker `-fsanitize=thread` | The linker must also know about TSan so it links in `libtsan` — the runtime library that actually maintains the shadow memory and prints reports at runtime. Forgetting this flag causes undefined symbol errors at link time. |

---

## Files added

| File | Description |
|---|---|
| `profiles/gcc13.conan` | TSan profile (`build_type=Release`, GCC 13) |
| `Makefile` | `TSAN_BUILD_DIR`/`TSAN_TOOLCHAIN` variables + 14 new targets |

---

## Makefile targets

| Target | Description |
|---|---|
| `make conan-install-tsan` | Install deps for Debug build type |
| `make configure-tsan` | CMake configure with TSan flags into `/test-builds/build-tsan/` |
| `make build-tsan` | Compile |
| `make rebuild-tsan` | `clean-tsan` + `configure-tsan` + `build-tsan` |
| `make test-tsan` | Run all tests under TSan |
| `make test-one-tsan PATTERN=<p>` | Run a single test by pattern |
| `make clean-tsan` | Remove `/test-builds/build-tsan/` without touching `production-builds/release/` |

---

## How to run a TSan build

```bash
make configure-tsan   # conan install (Debug) + cmake configure with TSan flags
make build-tsan       # compile
make test-tsan        # run tests; TSan will report any data races it finds


# Clean only the TSan build (leaves Release untouched)
make clean-tsan
```

---

## What a TSan race report looks like

If TSan catches a race, you will see output like:

```
WARNING: ThreadSanitizer: data race (pid=...)
  Write of size 4 at 0x... by thread T2:
    #0 MyClass::setCounter() adapter_iptc/src/my_class.cpp:42
  Previous read of size 4 at 0x... by thread T1:
    #0 MyClass::getCounter() adapter_iptc/src/my_class.cpp:37
```

Both the writer and reader are identified with exact file and line numbers — no guessing needed. Fix the race by adding a mutex or using `std::atomic` for the shared variable.

---

## Important lessons

These are real bugs hit during the initial setup of this TSan build. They are common enough to be worth documenting.

### Bug 1 — `protocd` not found when using `build_type=Debug` (found in piiptables)

**Symptom:**
```
CMake Error: Library 'protocd' not found in package.
```

**What happened:** Setting `build_type=Debug` in the Conan profile caused Conan to generate CMake files expecting a library named `protocd` — the `d` suffix is a Debug-build naming convention. The Conan recipe for `protobuf/3.21.12` does not produce a library with that suffix, so the configure step failed.

**Why it matters:** This is TSan's most common gotcha with Conan. It looks like a missing library error but the root cause is a profile/recipe mismatch.

**Fix:** Use `build_type=Release` in the TSan Conan profiles. TSan only needs to instrument *your* code — dependencies compiled in Release are fine. The TSan flags are applied selectively at CMake level via `CMAKE_CXX_FLAGS`, not via the Conan profile.

---

### Bug 2 — `CMAKE_BUILD_TYPE` must be passed explicitly on the command line

**Symptom:**
```
CMake Error: Please, set the CMAKE_BUILD_TYPE variable when calling to CMake
adding the '-DCMAKE_BUILD_TYPE=<build_type>' argument.
```

**What happened:** Conan's `CMakeDeps` generator emits a `check_build_type_defined()` guard inside every `*-config.cmake` file. This guard runs during `find_package()`, before the toolchain has had a chance to set `CMAKE_BUILD_TYPE`. Even though the Conan toolchain (`conan_toolchain.cmake`) would eventually set the variable, the check fires too early and aborts.

**Fix:** Always pass `-DCMAKE_BUILD_TYPE=Release` explicitly on the `cmake` command line in `configure-tsan`. The command-line value is available immediately and satisfies the guard. This applies to any Conan 2 + CMakeDeps project, not just this one.

---

## Performance overhead

TSan is not a free check. Expect roughly:

- **5–15x** increase in memory usage (shadow memory for every byte in your program)
- **2–20x** slowdown in CPU time

This is why TSan runs in a dedicated build and is never used for production or benchmarking. Run it in CI on every pull request to catch races before they reach `main`.

### Bug 2 — `FATAL: ThreadSanitizer: unexpected memory mapping` (this repo, 2026-10-22)

**What happened:** every TSan test binary aborted at start-up inside the Ubuntu 24.04 sandbox.

**Why:** the host kernel randomises mmap addresses with `vm.mmap_rnd_bits=32`; GCC 13's TSan runtime
expects at most 28 bits and finds memory where its shadow layout should be. The usual workaround,
`setarch -R` (disable ASLR for the process), fails inside the container because Docker's default seccomp
profile rejects that `personality()` flag.

**Fix:** lower the entropy on the host before running the TSan targets:

```bash
sudo sysctl vm.mmap_rnd_bits=28
make test-tsan
```

