# Task description

Build the incoming parser of `bellerotables-helper`: a new library that turns the payload of one request frame (a
JSON text) into a clean domain request, or into a typed failure. It is the protobuf facade of this repo: the
generated `contracts` classes live in one translation unit behind it, and nothing else in the helper includes a
`.pb.h` or links `libprotobuf`.

The payload is the canonical protobuf JSON of the request envelope `{correlation_id, id, desired}`, as fixed in
the hub decision `../docs/decisions/helper-socket-posix.md` (item 5) and shaped by
`../docs/decisions/protobuf-consumer-facade.md` (the C++ half). Today the helper can accept a connection and
read/write frames (`adapter_socket/`, merged on `develop`), but nothing interprets the bytes. This task is the
step between "a frame arrived" and "a use case can run".

What exists upstream and what does not:

- `contracts` (sibling repo, `../contracts`, commit `e86bc8f` at the time of writing) has
  `proto/bellerophon/ngfw/redirection/desired.proto` (`DesiredRedirection`, one optional `Logging`) and
  `proto/bellerophon/ngfw/shared/logging.proto` (`Logging`: `group` required and `>= 0`, optional `prefix`
  max 64 chars, optional `range >= 0`, optional `threshold >= 1`). It has **no** envelope message; the envelope
  is parsed by this repo.
- This repo has no `contracts` submodule, no protobuf in the Conan file or the sandbox image, and `main.cpp` is an
  empty `main`.

The work:

1. **Bring `contracts` in as a pinned submodule** of this repo, at the commit above (or a newer one if the
   implementer records why). Its generated headers are produced at build time, not committed here.
2. **Codegen in the build.** A CMake custom command regenerates the C++ classes from the submodule's `.proto`
   files with `protoc --cpp_out`, so a breaking schema change breaks the compile (the generated classes are the
   pact, the compiler is the verifier). The include paths and the vendored `buf/validate` import must work the way
   `../contracts/codegen/gen.sh` sets them (`-I proto -I proto/third_party`). The protoc version must be the one
   `contracts` pins (25.1, `../contracts/codegen/Dockerfile.protoc`) and the `libprotobuf` linked must be the
   matching runtime. Dependencies reach the build through the existing channels: Conan for libraries
   (`make sandbox-build` rebuilds the cached image), apt/pip in `sandbox/Dockerfile` for tools.
3. **A parser library** (own folder under the repo root, same layout and house rules as `adapter_socket/`) with a
   public interface that takes the payload text and returns `std::expected<RedirectionRequest, ParseProblem>`:
   - `RedirectionRequest` is a plain domain struct: `correlationId` (string), `id` (integer), `desired` (a domain
     type holding an optional `Logging` with `group`, optional `prefix`, optional `range`, optional `threshold`).
     No protobuf type appears in any public header.
   - Parsing goes: envelope JSON, then `desired` through the generated message (`JsonStringToMessage` or the
     equivalent), then mapping to the domain struct. The envelope and the generated message must agree on field
     names (`correlation_id`, `id`, `desired`, and the `Logging` fields as in the `.proto`).
   - Failures are values, not exceptions: `ParseProblem` carries a machine-readable kind (at least: malformed
     JSON, missing envelope field, wrong field type, unknown field in `desired`, rule violation) and a human
     detail. The detail never echoes more than the offending field name and a short reason (the payload is
     untrusted input to a privileged process).
4. **Decide, and record, whether this library runs protovalidate at runtime.** This is genuinely open and part of
   the task, because two documents disagree: `CLAUDE.md` says "the helper validates nothing at runtime — it trusts
   input already validated by the API upstream", while `protobuf-consumer-facade.md` lists protovalidate-cc inside
   the C++ parse. Recommended: **do not link protovalidate-cc in this task.** Reasons: it is a heavy dependency
   (Abseil + a CEL runtime) whose Yocto footprint the facade decision itself says must be confirmed first; the
   API already validates before the agent forwards; and the helper still rejects structurally invalid input
   (malformed JSON, unknown fields, wrong types) through the protobuf parser. Whichever way is chosen, write the
   decision, its reason and what would reverse it in a short document under `docs/` and make the CLAUDE.md
   sentence and the facade decision agree with it (the hub file is edited only to add a pointer or a status line,
   not to rewrite it). If protovalidate is chosen, the rule-violation kind in item 3 is exercised by tests; if not,
   a payload that violates a rule (for example `group = -1`) is documented as accepted by this layer.
5. **Tests.** Unit tests for the parser with literal JSON strings: the happy paths, each failure kind, and the
   bounds of the payload size the socket layer allows (a payload of exactly 64 KiB is parsed or rejected as
   structurally invalid, never crashes). Tests run without the network, without systemd and without a socket.

**Decisions already taken (do not reopen):**

- Generated protobuf classes stay behind this facade; domain code is protobuf-agnostic; `libprotobuf` is linked
  only into the parser library. (`protobuf-consumer-facade.md`: one blast radius for the heavy dependency.)
- `LITE_RUNTIME` is not an option: canonical JSON mapping needs reflection. Do not propose it.
- Wire format is canonical protobuf JSON (Mode B) with proto field names (`correlation_id`, not
  `correlationId`).
- `nlohmann_json` is this repo's own JSON layer (envelope, outgoing text later); it is not what is generated from
  the contract.
- The result type is `std::expected` (C++23, GCC 13); no exceptions for expected bad input.
- House rules as in `adapter_socket/`: no free functions (helpers are `static` private members; file-scope
  `constexpr` constants are fine), headers only under `include/<lib>/`, interfaces with only a defaulted virtual
  destructor and pure virtuals, concrete classes `final` and non-copyable/non-movable, DTOs as plain structs with
  static helpers, no calls or compound expressions inside `if`/`while` conditions, no magic numbers, comments only
  with a modifier (`LEARN:`, `TODO:`, ...), no `std::shared_ptr`, no anemic getters. Names not fixed above are the
  implementer's choice.

**Out of scope:**

- The controller, the use case and `adapter_iptc`: nothing consumes `RedirectionRequest` yet, and `main.cpp` stays
  an empty `main`.
- Outgoing serialization (the phase-2 reply) and `ProblemDetails` on the wire.
- Any change to `adapter_socket/`, to the frame size limit, or to the `contracts` repo. If the envelope should
  become a proto message, that is a change in `contracts`, not here.
- The correlation-id UUID check and the semantic meaning of `id`: the agent validates and correlates upstream; the
  parser carries the values.
- CDC beyond the compile-time check in item 2 (runtime consumer tests belong to the cross-cutting strategy in
  `../docs/decisions/consumer-driven-contract-testing.md`).
- Yocto packaging of protobuf/Abseil in `meta-bellerophon`.

# How to judge completion

Run from the repo root. Builds run in the sandbox through the `Makefile`; after the dependency changes,
`make sandbox-build` must succeed first.

1. `git submodule status` shows `contracts` at a recorded commit and `.gitmodules` exists. A fresh clone with
   `git submodule update --init` plus `make rebuild` produces the binary with no manual step.
2. `make rebuild` (Release, `BUILD_TESTING=OFF`) succeeds, and the `.pb.h` / `.pb.cc` files are generated into the
   build directory, not into the source tree: `git status` shows no generated file.
3. Break-the-contract check: in a scratch copy of the submodule, rename `Logging.group` in
   `shared/logging.proto`; `make rebuild-unit` fails to compile in the mapping code. Revert. (Evidence that the
   compiler is the verifier.)
4. `make rebuild-unit && make test-unit` passes, the test count is higher than before, and the existing
   `adapter_socket` tests are unchanged in what they assert.
5. Happy paths, each a named test with the literal payload:
   - `{"correlation_id":"<uuid>","id":7,"desired":{"logging":{"group":5}}}` gives `id = 7`, `group = 5` and the
     three other `Logging` fields empty.
   - the same with `prefix`, `range` and `threshold` set gives all four values.
   - `desired` as `{}` (logging omitted, meaning "disabled") gives a request whose `Logging` is empty.
6. Failure paths, each a named test asserting the kind and that nothing throws:
   - empty string, truncated JSON, and non-UTF-8 bytes: malformed JSON.
   - envelope without `correlation_id`, without `id` and without `desired`: missing envelope field (three tests).
   - `"id":"7"` (string where integer expected), `"desired":[]`, `"group":"5"`: wrong field type.
   - `"desired":{"logging":{"group":5,"bogus":1}}` and an unknown top-level key in `desired`: unknown field.
   - a payload of 64 KiB of valid JSON padding and a deeply nested one: no crash, no unbounded recursion (a
     failure kind or a success, documented in the test).
7. No protobuf outside the facade:
   `grep -rln "\.pb\.h\|google/protobuf" --include=*.h --include=*.cpp . | grep -v '^./contracts'` lists only files
   under the parser library's `src/` and its tests, none of its public `include/` headers.
   `grep -rn "protobuf" CMakeLists.txt */CMakeLists.txt` shows the `libprotobuf` link only on the parser library
   target, and `adapter_socket` still builds without it.
8. The protovalidate decision (item 4 of the work) is written in a document under `docs/`, `CLAUDE.md` and
   `../docs/decisions/protobuf-consumer-facade.md` no longer contradict it, and the test list matches it (a
   `group = -1` test exists and asserts the documented behavior).
9. Mutation checks (revert each after): make the mapping drop `threshold`, the four-field test fails; make the
   parser return success on a missing `correlation_id`, the three missing-field tests fail; make unknown fields
   ignored (`ignore_unknown_fields`), the unknown-field tests fail.
10. `make rebuild-asan && make test-asan` and `make rebuild-valgrind && make test-valgrind` pass with no sanitizer
    errors and no leaks, including on the malformed and oversized payloads.
11. `make format-check` and `make tidy-check` report nothing, including the new library (the `format` and `tidy`
    targets pick up the new folder, or are extended to).
12. `CLAUDE.md` is updated where it says "Coming with the parser" (dependencies) and the build notes (submodule
    init, codegen step, protoc version), and `make help` still lists every target.

## Evidence the task is complete, and pointers for the reviewer

- `git submodule status` and `git status --short` after a clean build (items 1-2).
- The compile error excerpt from the break-the-contract check (item 3).
- The `make test-unit` summary line, before and after (items 4-6).
- The output of both greps in item 7.
- The path of the decision document and the one-line diff of the `CLAUDE.md` sentence (item 8).
- Names of the failing tests for each mutation (item 9).
- Final lines of `make test-asan`, `make test-valgrind`, `make format-check`, `make tidy-check` (items 10-11).
- `git diff --stat`: a `.gitmodules` and a submodule entry, `conanfile.py`, `sandbox/Dockerfile` (protoc), top-level
  `CMakeLists.txt`, the new library and its tests, docs. `adapter_socket/` should not appear.

## How you verified it, which files to look at

To be filled by the implementer:

- Branch: ______  Base commit: ______  `contracts` commit pinned: ______  protoc / libprotobuf versions: ______

| Item | Command run | Result |
|------|-------------|--------|
| 1 | | |
| 2 | | |
| 3 | | |
| 4 | | |
| 5 | | |
| 6 | | |
| 7 | | |
| 8 | | |
| 9 | | |
| 10 | | |
| 11 | | |
| 12 | | |

Files to read, in this order:

1. `../docs/decisions/protobuf-consumer-facade.md` and `../docs/decisions/helper-socket-posix.md`
2. The new decision document on protovalidate
3. The parser's public headers (the surface other code sees), then its `src/` (the only place protobuf appears)
4. The codegen section of the top-level `CMakeLists.txt`, `conanfile.py`, `sandbox/Dockerfile`
5. The parser tests, in the order of items 5, 6, 9

# Other notes for the reviewer

## The project in short

`bellerotables-helper` is a one-shot C++23 binary, socket-activated by systemd, that receives one length-prefixed
frame from the agent over a Unix stream socket, applies iptables rules and reports the real kernel state (never an
echo of `desired`). It runs with `CAP_NET_ADMIN` only, has no database and no network, and talks only to the
agent. The agent validates and correlates; the API writes. The socket layer (`adapter_socket/`) is implemented and
merged on `develop`; it hands out framed byte payloads. The planned chain is: socket lib, then parser (this task),
then controller, then use case driving `adapter_iptc`, then the phase-2 reply.

Reference example used throughout: a request enabling NFLOG logging for the redirection rule, group 5 with prefix
`"redir: "`, correlation id a UUID, id 7:

```json
{"correlation_id":"6f1c2a0e-7a52-4a0e-9b5e-0d6a3f6f1a11","id":7,
 "desired":{"logging":{"group":5,"prefix":"redir: "}}}
```

## Known gaps left on purpose

- The envelope is not a proto message in `contracts`; this repo owns its parsing. A follow-up in `contracts` could
  move it there.
- Nothing calls the parser yet; it is proven by tests only, until the controller exists.
- Runtime validation depends on the item-4 decision; if protovalidate is left out, rule violations such as
  `group = -1` reach the use case unless the use case or the API rejects them.
- Footprint of `libprotobuf` + Abseil on the Yocto image is unmeasured; the facade decision asks to confirm it
  before the image (`meta-bellerophon`) packages it.
- `docker` is not available inside the sandbox, so `../contracts/codegen/gen.sh` (which runs `docker run`) cannot
  be the build step; the build calls `protoc` directly with the same flags.
