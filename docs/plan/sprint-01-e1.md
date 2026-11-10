# Sprint 01: E1 Request parser

**Planned:** 2026-11-04. **Done:** 2026-11-09. Stories and acceptance criteria are in [backlog.md](backlog.md) (E1, H-101 to H-105);
the full brief is `task-request-parser.md` at the repo root. This file adds the sprint goal, capacity and the
task breakdown.

## Status (2026-11-09)

All five stories are done on `feat/protobuf-contracts-cpp`, including the stretch story H-103. What changed
against the plan:

- H-104: the decision is in [../protovalidate-runtime.md](../protovalidate-runtime.md). The parser checks the
  contract rules with validators generated at build time (`tools/protoc-gen-bvalidate`), so `group = -1` is
  rejected, not accepted as the T-104.1 text assumed.
- H-103: the 64 KiB cases were added last; ASan and Valgrind run clean. Valgrind needed
  `ShutdownProtobufLibrary()` in the protobuf test binaries (`contracts_cpp/test/support/`).
- H-105: protobuf headers appear only under `request_parser/src`, its tests, `contracts_cpp` and
  `tools/protoc-gen-bvalidate`, never in a public header.
- Not recorded: the mutation checks listed in T-103.2 left no trace in the repo.

Next: merge into `develop` (squash), then E2.

## Sprint goal

By the end of this sprint, the helper's controller work (E2) can call one parser that turns an agent frame into a
`RedirectionRequest` or a typed `ParseProblem`, with protobuf generated from the pinned `contracts` and confined
to that library.

## Capacity

No tracker and no velocity history exist yet, so these are assumptions. Correct them and the selection below
moves.

| Input | Value |
|---|---|
| Team | 1 C++ developer |
| Sprint length | 2 weeks, 10 working days |
| Focus factor | 0.7 |
| Capacity | 10 × 1 × 0.7 = **7 person-days** |
| Point ratio (assumed) | 1 point ≈ 0.5 person-day, so 7 days ≈ 14 points |
| Commit line (85%) | ≈ 12 points |

## Selected stories

| Order | Story | Pts | Depends on |
|---|---|---|---|
| 1 | H-101 Pin `contracts` and generate C++ in the build | 5 | none |
| 2 | H-104 Record the protovalidate decision | 2 | none (its test is a case in T-102.4) |
| 3 | H-102 Parse a valid envelope | 3 | H-101 |
| 4 | H-105 Keep protobuf out of public headers | 1 | H-102 |
| | **Committed** | **11** | |
| Stretch | H-103 Reject bad input without crashing | 5 | H-102 |

E1 is 16 points, which is above one sprint at the assumed ratio, so H-103 is stretch. If it does not land, it
opens sprint 02 and E1 closes there. All five stories have acceptance criteria and estimates; none is flagged.

Next stories if capacity allows (after H-103): H-201 to H-203 (`adapter_iptc` onto `develop`, E2), which do not
depend on the parser.

## Tasks

Hours are ideal hours. Every task has one type, like a GitHub label: **Build** (compilation, dependencies, pipelines), **Feature**
(new behavior, with its structure), **Tests** (adds or changes tests, no new behavior), **Docs** (documentation),
**Spike** (prior investigation that enables another task). A task is cut where the type changes, even inside one story, and holds one type only. Checks that only confirm the
obvious are part of the task that produces the work and appear in its "done when". A task ends with something
reviewable in the branch.

### H-101 Pin `contracts` and generate C++ in the build (5 pts)

| ID | Type | Task | h |
|---|---|---|---|
| T-101.1 | Build | Done in `c9f9d48`: `contracts` as a submodule at `e86bc8f`; `make check-contracts` fails with a clear message when it is not initialised. | 1 |
| T-101.2 | Spike | Spike: find a Conan `protobuf` whose runtime matches `protoc` 25.1 (C++ runtime 4.25.x) and whether its own `protoc` can be the generator. Done when the version and the reason are written in the PR. | 2 |
| T-101.3 | Build | `conanfile.py` gets `protobuf` (and `protoc` as a tool requirement if T-101.2 allows), then `make sandbox-build`. CMake `add_custom_command` running `protoc --cpp_out` with `-I proto -I proto/third_party` on `bellerophon/ngfw/redirection/desired.proto`, `bellerophon/ngfw/shared/logging.proto` and `buf/validate/validate.proto`, output under the build directory, and a static target for the generated code. Done when `make rebuild` passes and `git status` shows no generated file. | 6 |

### H-104 Record the protovalidate decision (2 pts)

| ID | Type | Task | h |
|---|---|---|---|
| T-104.1 | Docs | Write `docs/protovalidate-runtime.md` (choice, recommended: do not link protovalidate-cc; reason; what would reverse it), update this repo's `CLAUDE.md` sentence, add a status line or pointer (not a rewrite) to hub `docs/decisions/protobuf-consumer-facade.md`. The test that `group = -1` gives the documented behavior is a case in T-102.4. | 3 |

### H-102 Parse a valid envelope (3 pts) and H-105 Keep protobuf out of public headers (1 pt)

H-105 has no tasks of its own: its acceptance criteria are met by T-102.1, T-102.3 and T-102.5.

| ID | Type | Task | h |
|---|---|---|---|
| T-102.1 | Build | Create the parser library and `domain/` (cpp-lib skill: folders, `include/<lib>/`, `src/`, `test/` under `BUILD_TESTING`, CMake wiring; `domain/` is Ring 2 and links nothing). Link `libprotobuf` and the generated-code target PRIVATE on the parser target only. Make `format` and `tidy` cover both new folders (rename or extend the globs in the `Makefile`). Done when `make rebuild` passes, `format-check` and `tidy-check` see the new folders, and `adapter_socket` still builds without protobuf. | 3 |
| T-102.2 | Feature | Domain DTOs in `domain/` (cpp-dto skill): `RedirectionRequest` (`correlationId` string, `id` `int32_t`), `DesiredRedirection`, `Logging` with optional fields, as in `docs/request-controller.md` ("Interfaces", "Contract sources"). In the parser library: `ParseProblem` with its kind enum and detail, and `IRequestParser` returning `std::expected<domain::RedirectionRequest, ParseProblem>` (cpp-interface skill). `IRedirectionUseCase` and `RedirectionProblem` join `domain/` with the controller work, not in this sprint. | 3 |
| T-102.3 | Feature | Concrete parser: envelope with nlohmann (no exceptions); `desired` through `JsonStringToMessage` with unknown fields rejected; mapping generated message → domain types. Done when the two greps from the H-105 acceptance criteria find no protobuf type in any public header. | 4 |
| T-102.4 | Tests | Happy path: `group` only, all four `Logging` fields, `desired` as `{}`; the `group = -1` case from T-104.1. Contract drift test: for `DesiredRedirection` and `Logging`, compare the field names in the generated message's descriptor with the list the mapping handles, and fail when a field is added to the `.proto` and not mapped (the compiler catches renames and retypes, not additions). See `docs/request-controller.md`, "Contract sources". | 3 |
| T-102.5 | Docs | `CLAUDE.md`: "Coming with the parser" dependency line, submodule init, codegen step, `protoc` version; check `make help`. | 1 |

### H-103 Reject bad input without crashing (5 pts, stretch)

| ID | Type | Task | h |
|---|---|---|---|
| T-103.1 | Feature | Classify failures (malformed JSON, missing envelope field, wrong field type, unknown field), mapping the protobuf parse status to a kind without depending on its message text where possible. The detail holds at most the field name and a short reason, never payload content. Add a depth limit if nlohmann or protobuf recurses without bound. | 5 |
| T-103.2 | Tests | Failures: empty, truncated, non-UTF-8; three missing fields; three wrong types; two unknown fields. Size and depth: 64 KiB of valid padding, a deeply nested payload. Mutation checks from the brief (drop `threshold`, accept a missing `correlation_id`, ignore unknown fields), each reverted. Done when all of it also runs clean under `make test-asan` and `make test-valgrind`. | 6 |

**Totals:** committed stories 26 h of tasks (H-101 9 h, H-104 3 h, H-102 and H-105 14 h; T-101.1 is already
done), stretch 11 h. At 7 person-days (≈ 49 focused hours) the committed work fits with a lot of room for review
and the gates, so H-103 should fit as well; the point estimates were kept until there is a measured velocity to
compare them with.

## Dependencies

```
T-101.1 (done)
T-101.2 → T-101.3 → T-102.1 → T-102.2 → T-102.3 → T-102.4 → T-102.5
                                          ↓
                                       T-103.1 → T-103.2
T-104.1   (parallel with all of the above; its test is a case in T-102.4)
```

T-102.1 needs the generated-code target from T-101.3. T-102.2 needs no protobuf, so it can start before
T-101.3 ends if T-102.1's folders exist.

## Risks

- **Protobuf version match.** The runtime must match `protoc` 25.1. If Conan has no exact 4.25.1, choosing a
  nearby 25.x patch is a decision to record (T-101.2), not a silent swap. Protobuf also pulls Abseil into the
  image build time.
- **Error classification.** The protobuf JSON parser reports failures as a status with text. Telling "unknown
  field" from "wrong type" may need checks on our side before calling it, rather than parsing the message.
- **Tooling globs.** `format` and `tidy` pick up `adapter_*/` only. `domain/`, and a parser named outside that
  pattern, need the Makefile extended (T-102.1).
- **Depth and size.** A 64 KiB payload can nest about 32 000 levels; recursion in either parser must be checked,
  not assumed.
- **Branching.** Cut `feat/request-parser` from `develop` and squash-merge back, per the repo rules.

## Out of this sprint

Controller, use case, `adapter_iptc` (unless stretch room remains after H-103), reply serialization, Yocto
packaging of protobuf and Abseil.
