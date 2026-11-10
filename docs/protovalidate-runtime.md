# Runtime validation of the contract rules

Status: accepted, implemented (H-104).

## Decision

The helper does not link protovalidate-cc. The `buf.validate` rules written in `contracts` are turned into C++ at
build time by our own protoc plugin, `tools/protoc-gen-bvalidate`, and the parser calls the generated
`collectViolations()` functions after the JSON has been mapped to the generated message.

## Why

- protovalidate-cc evaluates the rules with CEL at runtime. That adds the CEL runtime and its dependencies to a
  one-shot binary that ships on a small Yocto image, next to `libprotobuf` and Abseil, which we already accept.
- The rules the contract uses today are three: `int32.gte`, `string.max_len` and `string.uuid`. A generator for
  that set is small, and the contract rules stay the single source: nothing is rewritten by hand in this repo.
- A rule the generator does not know fails the build (`UnsupportedRule` in `validate_generator.cpp`), so the
  contract cannot gain a rule that the helper silently ignores.
- `contracts_cpp/test/contracts_validation_test.cpp` checks the generated validators against the contract's own
  messages.

## What it means in the code

`JsonRequestParser::parse` runs in this order: JSON read, envelope and `desired` mapped to the generated message,
rules checked, domain request built. A rule failure is a `ConstraintViolation` whose detail lists the field path
and the rule id, never the value. A value such as `logging.group = -1` is therefore rejected by the helper
(`NegativeGroup` in `json_request_parser_test.cpp`). The sprint plan expected the opposite, because it assumed no
validation at runtime; that expectation is replaced by this document.

`correlation_id` and `id` are checked as the contract's `Accepted` message, so the UUID rule comes from the
`.proto`.

## What would reverse it

- A contract rule that needs CEL (a custom `cel` expression, a message-level or oneof rule). The generator refuses
  it, and the choice is then between extending the plugin and linking protovalidate-cc.
- A second consumer in C++ that needs the full rule set.
- The footprint measured on the image showing that CEL is affordable.

## Related

- Hub `docs/decisions/protobuf-consumer-facade.md`: the facade this refines (it still names protovalidate-cc
  for the Java half and as the original C++ plan).
- `docs/request-controller.md`, "Contract sources".
