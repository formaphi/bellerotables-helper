# Task description
Turn the payload of one request frame (a JSON text) into a clean domain request, or into a typed failure, in a new `request_parser/` library. The generated protobuf classes and nlohmann stay inside the `.cpp`; the callers only see `IRequestParser`, `domain::RedirectionRequest` and `ParseProblem`.

# How to judge completion

Done when `JsonRequestParser::parse` returns `std::expected<RedirectionRequest, ParseProblem>` for every payload (never throws), no public header under `request_parser/include/` mentions protobuf or nlohmann, and no regressions occur in `adapter_socket/`.

# Other notes for the reviewer

## The project in short

bellerotables-helper is a one-shot C++23 binary, socket-activated by systemd, that reads one length-prefixed frame from the agent, applies iptables rules and reports the real kernel state. The socket layer hands out the frame payload; the parser is the step between "a frame arrived" and "a use case can run".

Reference payload used throughout: id 7, NFLOG group 5, prefix `"redir: "`.

```json
{"correlation_id":"6f1c2a0e-7a52-4a0e-9b5e-0d6a3f6f1a11","id":7,
 "desired":{"logging":{"group":5,"prefix":"redir: "}}}
```

## Known gaps left on purpose

- Nothing calls the parser yet; it is proven by tests only, until the controller exists.
- The envelope is not a proto message in `contracts`; this repo parses it with nlohmann.
- The footprint of `libprotobuf` on the Yocto image is unmeasured.

## Out of scope

- The controller, the use case and `adapter_iptc`. `main.cpp` stays an empty `main`.
- Outgoing serialization (the phase-2 reply).
- Any change to `adapter_socket/`, to the frame size limit or to the `contracts` repo.

## General Idea how they interact

A caller depends on `IRequestParser`. `JsonRequestParser` runs four steps in a chain; the first step that fails stops the chain and its `ParseProblem` is returned. Only the last step produces a domain object. Tests put `MockRequestParser` in the caller's place.

```
caller ----> IRequestParser <---- JsonRequestParser          MockRequestParser (tests)
                                        |
   payload (string_view)                |
        |                               v
        +--> readDocument -----> nlohmann: parse, depth <= 16, must be an object
        |        | fail: MalformedJson (not an object) / ContractMismatch (too deep)
        +--> readEnvelope -----> keys = correlation_id, id, desired; id fits int32;
        |        |               desired -> JsonStringToMessage -> DesiredRedirection
        |        | fail: ContractMismatch (unknown/missing field, wrong type)
        +--> checkConstraints -> bvalidate: Accepted{correlation_id, id} + DesiredRedirection
        |        | fail: ConstraintViolation (rule id + field path, never the value)
        +--> toRequest --------> domain::RedirectionRequest { correlationId, id, desired.logging? }
```

`ParseProblem` carries a kind (`MalformedJson`, `ContractMismatch`, `ConstraintViolation`) and a detail that names fields and rules only.

## Evidence the task is complete, and pointers for the reviewer

- The files are really there (listing of `request_parser/`).
- `make test-unit` passes and lists the `request_parser_tests` binary.
- `make test-asan`, `make test-valgrind`, `make format-check` and `make tidy-check` pass.

## How you verified it, which files to look at

1. `make rebuild-unit && make test-unit`
   Expected: all PASS, including `request_parser_tests`.
2. Files that exist:
   `find request_parser -type f | sort`
3. No protobuf or nlohmann in the public headers:
   `grep -rn '#include' request_parser/include | grep -iE 'pb\.h|protobuf|nlohmann'` prints nothing.
   `grep -rln 'pb\.h\|google/protobuf' request_parser` lists only `src/` and `test/`.
4. Tests exist for the happy paths and each failure kind:
   `grep -nE '^(TEST|TEST_F|TEST_P)' request_parser/test/*.cpp`
5. `make rebuild-asan && make test-asan` and `make rebuild-valgrind && make test-valgrind` pass with no sanitizer errors and no leaks.
6. `make format-check` and `make tidy-check` report nothing.
7. `adapter_socket/` is unchanged: `find adapter_socket -type f -newer task-request-parser.md` prints nothing.

## Files to read, in this order:

1. `docs/request-controller.md` (Interfaces, Contract sources)
2. `request_parser/include/request_parser/parser/interface_request_parser.h` and `parse_problem.h`
3. `domain/include/domain/request/redirection_request.h`, `logging/logging.h`, `redirection/desired_redirection.h`
4. `request_parser/src/json_request_parser.cpp`, the only place protobuf and nlohmann appear
5. `request_parser/include/request_parser/mock/mock_request_parser.h`
6. The tests: `json_request_parser_test.cpp`, `parse_problem_test.cpp`, `mock_request_parser_test.cpp`, `private_link_test.cpp`
