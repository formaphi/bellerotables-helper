# perf — profiling options (not implemented)

**Status: deferred (2026-10-22).** Nothing here is wired into the `Makefile` or the sandbox yet. This page
records the options so the work can start from a decision instead of from scratch.

## When it becomes worth doing

The helper is one-shot: it reads one frame, applies the rules and exits in milliseconds. Profiling a single
run mostly shows process start-up. A meaningful profile needs **load**: an end-to-end scenario that drives
many activations through `bellero-agent` → socket → helper → libiptc, so the hot paths (frame parsing,
rule building, `iptc_commit`) dominate the samples. Start this once that high-load E2E scenario exists.

## Why it is not a CMake flag

ASan, TSan and gcov are compiler instrumentation, so they are CMake flags (see the other pages in
`docs/build/`). `perf` is an external profiler: it samples the running process through the kernel's
`perf_event_open`. CMake can only make the binary *profilable*; running `perf` needs an environment that
allows it.

## Part 1 — a `perf` build type (CMake)

A `configure-perf` target, shaped like the other build types:

- `-DCMAKE_BUILD_TYPE=RelWithDebInfo` — optimized, so the profile reflects real code, with symbols for
  function and line attribution.
- `-fno-omit-frame-pointer -mno-omit-leaf-frame-pointer` — without them `perf record -g` gets broken call
  stacks.
- No sanitizers and no coverage: both distort timings.

## Part 2 — where `perf` runs

The sandbox cannot run `perf` today:

- `cap_drop: ALL` in `sandbox/docker-compose.yml`; `perf` needs `CAP_PERFMON`.
- Docker's default seccomp profile blocks `perf_event_open` without that capability.
- The dev host has `kernel.perf_event_paranoid = 4` (an Ubuntu-specific level that blocks unprivileged
  `perf` even on the host).

Either option needs `sudo sysctl kernel.perf_event_paranoid=1` on the host first (`-1` for kernel
counters), the same kind of host step as `vm.mmap_rnd_bits` for TSan.

| Option | How | Trade-off |
|--------|-----|-----------|
| **A. Separate `profiler` compose service** (preferred) | Same image, `cap_add: PERFMON` (plus `SYS_PTRACE` for `perf top`); `make perf-record` / `make perf-report` | Main sandbox stays locked down; reproducible. The image needs `linux-tools` matching the **host** kernel version, which breaks on every host kernel update |
| **B. `perf` on the host** | Build in the container (release output lands in `production-builds/` on the host), run `perf record` on the host | No sandbox or image change; outside the Makefile and tied to the host setup |
