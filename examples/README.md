# Examples

Small, runnable Jik programs to explore, copy, and modify. Browse by topic or
start with the first few language examples; each program has its own purpose.
For detailed explanations, see the [language overview](../docs/overview.md).

## Language examples

- [hello.jik](hello.jik) — the smallest complete program.
- [values.jik](values.jik) — values, inferred and explicit declarations, default initialization, operators, and assignment.
- [functions.jik](functions.jik) — inferred helpers, explicit signatures, and nested calls.
- [control_flow.jik](control_flow.jik) — branches, numeric ranges, `while`, `break`, and `continue`.
- [cl_args.jik](cl_args.jik) — raw command-line arguments passed to `main`.
- [strings.jik](strings.jik) — trimming, searching, and byte-based indexing and slicing.
- [vectors.jik](vectors.jik) — construction, mutation, filtering, indexed iteration, and slices.
- [structs.jik](structs.jik) — defaults, named fields, initializer shorthand, and uniform function calls (UFCS).
- [options.jik](options.jik) — search results with `Some`, `None`, and checked payload extraction.
- [dictionaries.jik](dictionaries.jik) — string-keyed stock counts, safe lookup, and iteration.
- [enum_match.jik](enum_match.jik) — exhaustive enum matching.
- [variants.jik](variants.jik) — input events, payload bindings, `other:`, and checked payload access.
- [tables.jik](tables.jik) — exhaustive, immutable enum mappings and state transitions; dictionaries instead support dynamic string keys.
- [regions_basic.jik](regions_basic.jik) — basic local regions, explicit and implicit region arguments, anchored allocation, and `@`
- [region_ergonomics.jik](region_ergonomics.jik) — automatic literal allocation, shared argument regions, and `@`.
- [regions_copy.jik](regions_copy.jik) — retaining selected temporary data with `foreign` and copying, and which types support copying.
- [error_handling.jik](error_handling.jik) — validation, recovery, propagation, `must`, and postfix `!`.
- [modules/main.jik](modules/main.jik) — local imports with [modules/stats.jik](modules/stats.jik).
- [testing_demo.jik](testing_demo.jik) — assertions with `jik/testing`.
- [ffi_demo.jik](ffi_demo.jik) — advanced C interop with embedded C, opaque structs, and region allocation.

## Standard-library examples

- [filesystem.jik](filesystem.jik) — read-only filesystem inspection with path and file I/O utilities.
- [binary_data.jik](binary_data.jik) — binary-safe immutable bytes and a growable byte buffer.
- [strbuf_demo.jik](strbuf_demo.jik) — efficient string construction from many small pieces.
- [text_processing.jik](text_processing.jik) — splitting text, string/vector slices, and comparisons.
- [argparse_demo.jik](argparse_demo.jik) — generated help, parsed arguments, and normalized paths.
- [process_capture.jik](process_capture.jik) — child-process exit status, stdout, and stderr bytes.

## Algorithms and larger programs

- [fib.jik](fib.jik) — recursive and iterative Fibonacci, with inferred function types.
- [primes.jik](primes.jik) — prime numbers with loops and vectors.
- [word_count.jik](word_count.jik) — line, word, and byte counts from an input file.
- [newton.jik](newton.jik) — numeric code using `jik/math`.
- [dijkstra.jik](dijkstra.jik) — shortest paths on a graph.
- [game_of_life.jik](game_of_life.jik) — terminal animation using randomness and system calls.
- [forth.jik](forth.jik) — a small interactive Forth interpreter.

## Running examples

Examples are included in compiler release archives. From the repository root
after building Jik, or from the extracted archive directory:

```text
jik run examples/hello.jik
```

Some examples are interactive or terminal-dependent:

- `forth.jik` starts a REPL
- `game_of_life.jik` redraws the terminal repeatedly

Most examples are self-contained. The `modules/` example contains multiple files to show
how Jik modules are organized and imported.

`run` and `build` require a compatible host C compiler such as GCC or Clang.
Select it by setting `JIK_CC` to the preferred C compiler, or with the flag `--cc`.
See the [CLI reference](../docs/cli.md) for further info.
