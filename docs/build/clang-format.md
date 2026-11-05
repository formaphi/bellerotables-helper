# clang-format

## What problem does clang-format solve?

In a C++ project with multiple contributors, every developer has slightly different formatting habits — brace placement, indentation style, spacing around operators, line length. Code review time gets wasted on style debates, and diffs get polluted with irrelevant whitespace changes.

**clang-format** is a tool that enforces a single, machine-defined style across all source files. It reformats code automatically so that style is never a matter of opinion: the tool decides, consistently, every time.

---

## Configuration file

The style rules live in `.clang-format` at the root of the repository. clang-format automatically discovers and applies this file to any source file in the tree.

---

## Makefile targets

| Target | Description |
|---|---|
| `make format` | Apply clang-format in-place to all source files |
| `make format-check` | Dry-run check — exits non-zero if any file is not formatted (CI gate); runs `include-check` first |
| `make include-check` | Fails if a project header is included with `<>` (`#include <adapter_.../...>`) |

`format-check` is **not** in the pre-commit hook yet: the existing `adapter_iptc` code predates this configuration (2-space indent, same-line braces), so the gate would fail on code nobody touched. Format each file when it is rewritten; add `format-check` to the hook once the tree is clean.

---

## Configuration reference

The configuration mirrors the house style in the `cpp-style` skill (`references/formatting.md`). When the
two disagree, the skill wins and `.clang-format` is fixed.

| Field | Value | Why |
|---|---|---|
| `BasedOnStyle` | `Google` | Baseline; the fields below override it |
| `Standard` | `Latest` | The code is C++23 |
| `IndentWidth` / `TabWidth` / `UseTab` | `4` / `4` / `Never` | 4 spaces, no tabs |
| `AccessModifierOffset` | `-4` | `public:` / `private:` flush with the `class` keyword |
| `ColumnLimit` | `100` | Same limit as before |
| `BraceWrapping.AfterFunction`, `AfterClass`, `AfterStruct`, `AfterEnum`, `AfterControlStatement: Always` | `true` | Opening brace on its own line for functions, classes and control statements |
| `BraceWrapping.BeforeElse`, `BeforeCatch` | `true` | `else`, `else if` and `catch` start on their own line |
| `BraceWrapping.AfterNamespace` | `false` | `namespace lib {` keeps the brace on the same line |
| `FixNamespaceComments` | `true` | Closing line carries `} // namespace lib` |
| `NamespaceIndentation` | `None` | Namespace bodies are not indented |
| `BreakConstructorInitializers` | `BeforeComma` | Colon on its own line, one member per line with a leading comma |
| `PackConstructorInitializers` | `Never` | Never pull the initializer list back onto one line |
| `PointerAlignment` / `DerivePointerAlignment` | `Left` / `false` | `const sockaddr* sa`, `const Family& fam` |
| `AllowShort*OnASingleLine` | `Empty` / `Never` / `false` | No one-line `if`, loop or block bodies; only empty functions stay on one line |
| `SeparateDefinitionBlocks` | `Always` | One blank line between function definitions |
| `MaxEmptyLinesToKeep` | `1` | Collapse runs of blank lines |
| `SortIncludes` / `IncludeBlocks` | `CaseSensitive` / `Regroup` | Regroup every include into the blocks below and sort each block alphabetically |
| `IncludeIsMainRegex` | `([-_](test\|unittest))?$` | `foo.cpp` and `foo_test.cpp` keep `foo.h` first, alone in its block |
| `IncludeCategories` | `^"` → 1, `^<` → 2 | After the own header: project headers with `""`, then everything from outside the project with `<>` (C/POSIX, C++ standard library, gtest, libsystemd, ...) |
| `SpacesBeforeTrailingComments` | `1` | `} // namespace lib` |

---

## Include order

```cpp
#include "adapter_socket/socket_connection/unix_socket_connection.h"  // own header

#include "adapter_socket/deadline/interface_deadline.h"               // project: ""
#include "parameterized_case.h"

#include <cstddef>                                                    // outside the project: <>
#include <gtest/gtest.h>
#include <sys/socket.h>
```

clang-format only sorts and groups; it cannot turn `<adapter_socket/...>` into `"adapter_socket/..."`. That
half of the rule is checked by `make include-check`, which `format-check` runs first.
