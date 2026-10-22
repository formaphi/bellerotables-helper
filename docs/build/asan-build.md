# AddressSanitizer (ASan) Build

## What problem does ASan solve?

Memory bugs are among the most dangerous and hardest-to-diagnose defects in C++ programs. Reading past the end of a heap buffer, writing into freed memory, returning a pointer to a stack variable that no longer exists — these errors often produce no crash at all, or crash much later and far from the original bug.

**AddressSanitizer** is a compiler-based tool that catches these bugs at the exact moment they happen. It instruments every memory access at compile time so that accesses to invalid memory trigger an immediate, informative error report instead of silent corruption or an unrelated crash later.

ASan detects:

- Heap buffer overflow / underflow
- Stack buffer overflow / underflow
- Global buffer overflow
- Use-after-free (dangling pointer dereference)
- Use-after-return (pointer to stack variable escapes its function)
- Use-after-scope (pointer to local variable used after its block ends)
- Double-free / invalid-free

---

## Why a separate Conan profile?

The project already has:
- `profiles/gcc13.conan` → `build_type=Release`, GCC 13


1. **Reuse the pre-cached dependencies.** The Docker image pre-compiles the Conan dependencies (gtest today, protobuf once the parser lands) in Release mode. A Debug profile forces a rebuild from source; in `piiptables` it also broke protobuf, whose `protobuf/3.21.12` recipe does not provide the `d`-suffixed `protocd` library.

2. **ASan only needs to instrument your code, not dependencies.** Memory bugs in dependencies are not your concern. Applying ASan flags selectively via `CMAKE_CXX_FLAGS` achieves the same safety goal without rebuilding third-party libraries.

The generators are isolated in a separate output folder (`/test-builds/build-asan/`) using `conan install --output-folder`, so they do not conflict with the Release or TSan build generators.

---

## Why a separate CMake build directory?

The Release binary at `production-builds/release/` is compiled without instrumentation — fast and optimized. The ASan binary needs different object files, different linking, and a different layout. Sharing the same directory would force a full rebuild on every switch.

By pointing CMake at `/test-builds/build-asan/` (`-B $(ASAN_BUILD_DIR)`), all three builds (Release, TSan, ASan) coexist independently.

---

## What the compiler and linker flags do

```
-DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
```

| Flag | Role |
|---|---|
| `-fsanitize=address` | Tells GCC to insert a shadow memory check around every memory read and write. If the access falls outside a valid object, ASan aborts immediately with a detailed report. |
| `-fno-omit-frame-pointer` | Preserves the frame pointer register so that ASan's stack unwinder can produce full, accurate call stacks in error reports. Without this, stacks are often truncated or garbled. |
| `-g` | Embeds debug symbols — file names, line numbers, variable names — so that ASan reports show the exact source location of the bug. |
| Linker `-fsanitize=address` | The linker must also know about ASan so it links in `libasan` — the runtime library that maintains the shadow memory and prints reports. Omitting this flag causes undefined symbol errors at link time. |

---

## Files added

| File | Description |
|---|---|
| `profiles/gcc13.conan` | ASan profile (`build_type=Release`, GCC 13) |
| `Makefile` | `ASAN_BUILD_DIR`/`ASAN_TOOLCHAIN` variables + 14 new targets |

---

## Makefile targets

| Target | Description |
|---|---|
| `make conan-install-asan` | Install deps with Release build type into `/test-builds/build-asan/` |
| `make configure-asan` | CMake configure with ASan flags into `/test-builds/build-asan/` |
| `make build-asan` | Compile |
| `make rebuild-asan` | `clean-asan` + `configure-asan` + `build-asan` |
| `make test-asan` | Run all tests under ASan |
| `make test-one-asan PATTERN=<p>` | Run a single test by pattern |
| `make clean-asan` | Remove `/test-builds/build-asan/` without touching `production-builds/release/` or `/test-builds/build-tsan/` |

---

## How to run an ASan build

```bash
make configure-asan   # conan install (Release) + cmake configure with ASan flags
make build-asan       # compile
make test-asan        # run tests; ASan will abort on any memory error it detects


# Clean only the ASan build (leaves Release and TSan builds untouched)
make clean-asan
```

---

## What an ASan error report looks like

If ASan catches a bug, you will see output like:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x... at pc 0x...
WRITE of size 4 at 0x... thread T0
    #0 0x... in MyClass::fillBuffer() adapter_iptc/src/my_class.cpp:58
    #1 0x... in main main.cpp:23

0x... is located 0 bytes to the right of 16-byte region [0x...,0x...)
allocated by thread T0 here:
    #0 0x... in operator new[](unsigned long)
    #1 0x... in MyClass::MyClass() adapter_iptc/src/my_class.cpp:12
```

The report shows the exact access type (read/write), size, source location, and the allocation site of the object. Fix the bug, rebuild with `make rebuild-asan`, and re-run.

---

## Important note: ASan and TSan cannot be combined

ASan and TSan use incompatible shadow memory layouts. Never apply both `-fsanitize=address` and `-fsanitize=thread` to the same build. The dedicated `/test-builds/build-asan/` and `/test-builds/build-tsan/` directories exist precisely to keep these builds separate.

---

## Performance overhead

ASan is not free. Expect roughly:

- **2x** increase in memory usage (one shadow byte per eight application bytes)
- **2x** slowdown in CPU time

This is significantly lighter than TSan's overhead. Run ASan in CI on every pull request to catch memory bugs before they reach `main`.
