# Jik CLI Reference

The `jik` command-line tool is used to parse, check, translate, build, and runs Jik
programs. Usage is:

```sh
jik <command> [<args>...] [--<option> [<value>]]...
```

Run `jik help` to list available commands, or `jik help <topic>` to show help
for a command, language topic, or standard library entry.

Command arguments come before command options. For example, use
`jik run hello.jik --cc clang`.

## Commands

### `jik run <filepath>`

Translate a Jik source file to C, compile it with the selected C compiler, run
the executable, and then remove the executable.

Options:

- `--cc <compiler_name>`: C compiler to use.
- `--ccflags <args>`: Extra C compiler arguments.
- `--release`: Build with release-oriented host compiler flags.
- `--unsafe-no-bounds-checks`: Disable runtime vector bounds checks
- `--region-stats`: Print runtime region statistics at program exit.
- `--verbose`: Print detailed pipeline status information.

Example:

```sh
jik run hello.jik --cc clang
```

### `jik build <filepath>`

Translate a Jik source file to C and compile it to an executable.

Options:

- `--out <filepath>`: Write the executable to this path. When omitted, the
  default output path is `<input_file>.exe` on Windows and `<input_file>` on
  Linux.
- `--cc <compiler_name>`: C compiler to use.
- `--ccflags <args>`: Extra C compiler arguments.
- `--preview`: Print the compiler command without running it or creating an
  executable.
- `--release`: Build with release-oriented host compiler flags.
- `--unsafe-no-bounds-checks`: Disable runtime vector bounds checks
- `--region-stats`: Print runtime region statistics at program exit.
- `--verbose`: Print detailed pipeline status information.

Example:

```sh
jik build hello.jik --release
```

To inspect the compiler command without running it:

```sh
jik build hello.jik --preview
```

Reachable modules may contain native build directives such as `@includedir`, `@libdir`, `@link`,
and `@copy`. Active directives are applied transitively by `build` and `run`; copied files are placed
beside the produced executable. `@platform(windows)`, `@platform(linux)`, and `@platform(all)` select
the platform for subsequent build directives in the same source file.

### `jik tran <filepath>`

Translate a Jik source file to C.

Options:

- `--out <filepath>`: Write the generated C to this path. When omitted, the
  default output path is `<input-basename>.c`.
- `--embed-core`: Embed the Jik support library into the generated translation.
- `--format-c`: Format the generated C file in place with `clang-format`.
- `--unsafe-no-bounds-checks`: Disable runtime vector bounds checks
- `--region-stats`: Print runtime region statistics at program exit.
- `--verbose`: Print detailed pipeline status information.

Generated C includes the Jik support header as `#include "core.h"`. For manual compilation,
include the support include directory from `jik env`'s `core_include=<path>` value,
or use `--embed-core` to write a self-contained C translation.

Example:

```sh
jik tran hello.jik --embed-core
```

### `jik check <filepath>`

Parse and analyze a Jik source file.

Options:

- `--verbose`: Print detailed pipeline status information.

Example:

```sh
jik check hello.jik
```

### `jik memchk <filepath>`

Translate a Jik source file to C, compile it with debug flags, and run
`valgrind` memcheck.

Options:

- `--cc <compiler_name>`: C compiler to use.

Example:

```sh
jik memchk hello.jik --cc gcc
```

### `jik env`

Print resolved Jik configuration values as `key=value` lines, including the Jik
version, platform, root directory, `jiklib` path, package path, support include
path, and selected C compiler.

Example:

```sh
jik env
```

### `jik version`

Print the Jik version.

Example:

```sh
jik version
```

### `jik help [topic]`

Show general CLI help, or help for a command, language topic, standard library
module, or standard library member.

Examples:

```sh
jik help
jik help run
jik help vector
jik help jik/string
jik help jik/io
```

Documentation topic keys are defined in `docs/help-index.txt`. `jik help topics`
prints the available documentation keys.

## Compiler Selection

Commands that compile generated C select the host C compiler in this order:

1. The `--cc <compiler_name>` option.
2. The `JIK_CC` environment variable.
3. The host default compiler, when available.

If no compiler is found, `jik run`, `jik build`, and `jik memchk` fail before
compilation.

## Package Imports

Imports under `pkg/...` are resolved from the directory configured by the
`JIK_PKG_PATH` environment variable.

For example:

```jik
use "pkg/csv"
```

resolves to:

```text
<JIK_PKG_PATH>/packages/csv/src/csv.jik
```

Use `jik env` to check the currently resolved value. It is printed as
`pkg_path=<path>`. If `JIK_PKG_PATH` is not set and a program imports
`pkg/...`, compilation fails with a package path error.

## Common Workflows

- Use `jik check <file>` for a fast parse and semantic check.
- Use `jik run <file>` while developing a program.
- Use `jik tran <file>` when you want to inspect or format the generated C.
- Use `jik build <file> --release` when you want an optimized executable.
- Use `jik env` to debug how the CLI resolved paths and compiler settings.
