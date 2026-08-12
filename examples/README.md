# Examples

This directory contains small Jik programs that demonstrate different parts of the language and
standard library.

## Suggested reading order

1. `hello.jik` - the smallest complete Jik program
2. `fib.jik` - functions, loops, recursion, and region-based allocation
3. `regions_copy.jik` - returning copied composite values in a caller-chosen region
4. `primes.jik` - loops and vectors
5. `word_count.jik` - structs, file I/O, and standard library use
6. `text_processing.jik` - string/vector slices, indexed iteration, and comparisons
7. `modules/main.jik` - multi-file programs, modules, and imports
8. `variants.jik` - enums, payload-less variants, `match`, and UFCS
9. `error_handling.jik` - `throws`, recovery, propagation, `must`, and postfix `!`
10. `ffi_demo.jik` - calling C functions and opaque C structs through Jik's FFI
11. `testing_demo.jik` - basic use of `jik/testing`
12. `cl_args.jik` - raw command-line argument handling
13. `argparse_demo.jik` - parsed arguments, generated help, and path normalization
14. `process_capture.jik` - capture a process and inspect stdout/stderr

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

`argparse_demo.jik` prints its generated help with no arguments. To pass arguments to it, build it
first and run the resulting executable:

```text
jik build examples/argparse_demo.jik
examples/argparse_demo.exe source.txt out/../target.txt --verbose
```

Some examples are interactive or terminal-dependent:

- `forth.jik` starts a REPL
- `game_of_life.jik` redraws the terminal repeatedly

Most examples are self-contained. The `modules/` example contains multiple files to show
how Jik modules are organized and imported.
