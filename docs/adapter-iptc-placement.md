# adapter_iptc placement — piiptables or this repo

**Decided 2026-10-22.** `adapter_iptc` stays inside `bellerotables-helper` for the first vertical slice.
The build infrastructure built in the personal `piiptables` repo
(`gitlab.com/PietroAndrade/piiptables`) is ported here instead. `adapter_iptc` moves out to
`piiptables` as a standalone library when a second helper needs it.

## What piiptables holds

The repo was the earlier home of the libiptc experiments' *build setup*, not of the code.

| Area | Content | Fate |
|------|---------|------|
| Code | `helper/`, `receiver/` (renamed `application/`, `command_builder/` on `docs/8-architecture-redesign`) — only `.gitkeep` | nothing to keep |
| Design v0.01 | one-shot CLI: JSON rule file → JSON Schema (valijson) → shell out to the `iptables` binary | dropped — the helper writes through libiptc, and its input is a protobuf frame from `bellero-agent` |
| `proto/rule.proto` | rule format | superseded by `contracts` |
| `features/*.feature` | monitoring / DoS Gherkin | superseded by `EndToEnd` |
| `sandbox/` | Debian + AlmaLinux images: `libiptc-dev`, legacy iptables via `update-alternatives`, `NET_ADMIN` with `cap_drop: ALL`, internal network, Conan cache baked into the image | **ported** (one image) |
| `profiles/`, `Makefile` | release, ASan, TSan, Valgrind memcheck, gcov + lcov HTML, per build type | **ported** |
| `.clang-format`, `.clang-tidy`, `.githooks/pre-commit` | style and lint gates | **ported** |
| `wiki/` | how each build type works | **ported** to `docs/build/` |
| `.gitlab-ci.yml` | DinD pipeline mirroring the pre-commit suite | not ported — this repo lives on GitHub; rewrite as Actions later |

The libiptc code itself (`3848f02` onward, see [adapter-iptc-decision-trail.md](adapter-iptc-decision-trail.md))
was always in this repo. The two repos share no history (root commits `b9e9836` and `a50c8af`).

## Options weighed

**Unify both repos with two remotes.** Rejected. Merging needs `--allow-unrelated-histories` and imports
scaffolding-only history; every push goes to GitHub (`formaphi`) and GitLab, and CI would run only on
GitLab.

**piiptables becomes the `adapter_iptc` repo now.** Technically sound — `adapter_iptc` has no Bellerophon
concept. By decision it speaks iptables vocabulary per table (`MangleAdapter`, future `FilterAdapter`,
`NatAdapter`), so the whole directory, `MangleAdapter` included, is library material. Deferred because:

- Every milestone of the slice grows the library: DNAT (M1), source (M2), SNAT + MARK (M3), hashlimit (M4).
  Each would become a library PR, a pin bump and a helper PR.
- It adds a fetch to the `meta-bellerophon` recipe, which is paused.
- A production dependency would sit in a personal GitLab repo while the helper is in the `formaphi`
  GitHub organization.
- The code is still PoC quality and has no tests; hardening it is the same work in either repo.

**Keep `adapter_iptc` here, port the infra.** Chosen. The build types that matter most for
`calloc` / offset / binary-buffer code (ASan, Valgrind) become available now, without the cross-repo cost.

## Extraction trigger and path

Extract when the second per-domain helper (`zoneProtection`, `portForward`, …) starts, or when the slice
stabilizes, whichever comes first. Until then keep the boundary extractable:

- `adapter_iptc/` depends only on libiptc and the standard library — never on `adapter_socket`, the parser,
  or any domain type.
- Its mocks stay in `adapter_iptc/include/adapter_iptc/mocks/` so consumers keep using them after the move.
- Its tests live in `adapter_iptc/test/` and build on their own.

Path when the trigger fires: `git subtree split --prefix=adapter_iptc` (keeps the files' history), replace
the v0.01 scaffolding in `piiptables`, add it back here as a submodule consumed with `add_subdirectory`
(Yocto fetches it via `gitsm://` with a pinned `SRCREV`), and decide beforehand whether the repo moves to
`formaphi`.

## Build environment choice

The sandbox image is **Ubuntu 24.04**, not piiptables' Debian bookworm:

- Its default GCC is 13, the same major version as Yocto Scarthgap's toolchain, so the sandbox cannot
  accept language or library features the target compiler rejects. Bookworm ships GCC 12; trixie ships 14.
- It ships `libiptc-dev`, `libsystemd-dev` (for `sd_listen_fds()`), legacy iptables through
  `update-alternatives`, clang-format/clang-tidy, Valgrind and lcov.
- AlmaLinux variants are dropped: GCC 8 cannot build C++23.

The image replaces the transitional AlmaLinux 8 `Dockerfile`, until the `meta-bellerophon` image can be
dockerized (hub `docs/design/firewall-vertical-slice.md`, decision 8).
