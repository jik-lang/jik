# Examples

This directory contains small Jik programs that demonstrate different parts of the language and
standard library.

## Suggested reading order

1. `hello.jik` - the smallest complete Jik program
2. `fib.jik` - functions, loops, recursion, and region-based allocation
3. `primes.jik` - loops and vectors
4. `word_count.jik` - structs, file I/O, and standard library use
5. `modules/main.jik` - multi-file programs, modules, and imports
6. `variants.jik` - enums, variants, and `match`
7. `error_handling.jik` - `throws`, `try/except`, `error_msg`, `error_code`, and `must`
8. `ffi_demo.jik` - calling C functions through Jik's FFI
9. `testing_demo.jik` - basic use of `jik/testing`
10. `cl_args.jik` - command-line argument handling
11. `process_capture.jik` - capture a process and inspect stdout/stderr

The remaining examples are larger demonstrations:

- `dijkstra.jik` - shortest paths on a graph
- `newton.jik` - numeric code using `jik/math`
- `game_of_life.jik` - terminal animation with randomness and system calls
- `forth.jik` - a minimal Forth interpreter

## Running examples

From the repository root:

```text
jik run examples/hello.jik
jik run examples/fib.jik
jik run examples/modules/main.jik
```

Some examples are interactive or terminal-dependent:

- `forth.jik` starts a REPL
- `game_of_life.jik` redraws the terminal repeatedly

Most examples are self-contained. The `modules/` example intentionally spans multiple files to show
how Jik modules are organized and imported.
