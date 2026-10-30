# Jik CLI Reference

Use `jik` to check, translate, build, and run Jik programs:

```sh
jik <command> [<args>...] [--<option> [<value>]]...
```

Run `jik help` to list available commands, or `jik help <topic>` to show help
for a command, language topic, or standard library entry.

Command arguments come before command options. For example, use
`jik run hello.jik --cc clang`.

## Commands

### `jik new <directory>`

Create a minimal project containing `src/<project-name>.jik`, `.gitignore`, and
`README.md`. The source filename uses the destination directory's name.
The destination must not exist, and its parent directory must already exist.
Git is not required and no Git repository is initialized.

```sh
jik new my-project
cd my-project
jik run src/my-project.jik
```

The README describes how to run, check, and build the program. The `.gitignore`
excludes the generated executable and C files alongside the source in `src/`.


### `jik run <filepath> [options] [-- <args>...]`

Translate a Jik source file to C, compile it with the selected C compiler, run
the executable, and then remove the temporary build files.

Options:

- `--cc <compiler_name>`: C compiler to use.
- `--ccflags <args>`: Extra C compiler arguments.
- `--release`: Compile with `-O3` optimization.
- `--unsafe-no-bounds-checks`: Disable runtime vector bounds checks
- `--region-stats`: Print runtime region statistics at program exit.
- `--region-tree`: Print the runtime region lifetime hierarchy.
- `--dump-ast`: Print the parsed syntax tree.
- `--verbose`: Print detailed pipeline status information.

Example:

```sh
jik run hello.jik --cc clang
jik run hello.jik --cc clang -- "hello world" --verbose
```

Arguments after `--` are passed unchanged to the program, including empty strings
and values beginning with `-`. Put compiler options before `--`. The program's
`main(args)` receives the executable path at index 0, followed by these arguments.

`--region-tree` draws one row per runtime region. Region IDs show creation order,
and indentation shows nested lifetimes:

```text
R1 examples/hello::main.local
  R2 src/parser::parse.local
```

### `jik build <filepath>`

Translate a Jik source file to C and compile it to an executable.

Options:

- `--out <filepath>`: Write the executable to this path. When omitted, the
  default is the input path without `.jik`, with `.exe` added on Windows.
- `--cc <compiler_name>`: C compiler to use.
- `--ccflags <args>`: Extra C compiler arguments.
- `--preview`: Print the compiler command without running it or creating an
  executable.
- `--release`: Compile with `-O3` optimization.
- `--unsafe-no-bounds-checks`: Disable runtime vector bounds checks
- `--region-stats`: Print runtime region statistics at program exit.
- `--region-tree`: Print the runtime region lifetime hierarchy.
- `--dump-ast`: Print the parsed syntax tree.
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
the platform for subsequent build directives in the same source file. `@profile(windows-x64-mingw)`
and `@profile(linux-x64-gnu)` additionally restrict subsequent native directives to a compatible
compiler target; `@profile(all)` removes that restriction. Profile matching is checked before the
host compiler is invoked. MSVC is not currently a supported native build profile.

### `jik tran <filepath>`

Translate a Jik source file to C.

Options:

- `--out <filepath>`: Write the generated C to this path. When omitted, the
  default is the input path with `.jik` replaced by `.c`.
- `--embed-core`: Embed the Jik support library into the generated translation.
- `--emit-source-loc`: Emit `// source: path:line:column` comments before generated
  declarations that correspond to Jik source declarations.
- `--format-c`: Format the generated C file in place with `clang-format`.
- `--unsafe-no-bounds-checks`: Disable runtime vector bounds checks
- `--region-stats`: Print runtime region statistics at program exit.
- `--region-tree`: Print the runtime region lifetime hierarchy.
- `--dump-ast`: Print the parsed syntax tree.
- `--verbose`: Print detailed pipeline status information.

Generated C includes the Jik support header as `#include "core.h"`. For manual compilation,
include the support include directory from `jik env`'s `core_include=<path>` value,
or use `--embed-core` to write a self-contained C translation.

Example:

```sh
jik tran hello.jik --embed-core
```

### `jik check <filepath>`

Check syntax, types, and regions without generating C or running a C compiler.

Options:

- `--verbose`: Print detailed pipeline status information.
- `--dump-ast`: Print the parsed syntax tree.

Example:

```sh
jik check hello.jik
```

### `jik test [input] [options]`

Discover, build, and run tests. Input defaults to the current directory.
Directories are searched recursively for `test_*.jik` files; an explicitly
selected `.jik` file may have any name.

Tests are top-level `test_*` functions taking exactly one `jik/testing::TestSuite`
parameter, returning `void`, and declared without `throws`. Types may be inferred:

```jik
use "jik/testing" as testing

func test_addition(suite):
    testing::suite_assert(suite, 2 + 2 == 4, site())
end
```

```sh
jik test
jik test tests
jik test tests/math_spec.jik
jik test tests --list
jik test tests --emit-suite suite.jik
```

Options:

- `--list`: Print discovered module/function names and totals, then stop.
- `--emit-suite <file>`: Write the generated runner, then stop. The destination
  must not exist. The runner references the original source files and can be
  executed with `jik run <file>`.
- Compiler and runtime options are the same as for `jik run`, including `--cc`,
  `--release`, and `--verbose`.

`--list` and `--emit-suite` are mutually exclusive and need no C compiler.
Type and region checks occur when the suite is compiled.

Files run in relative path order, with functions in declaration order. Tests
inherit the current working directory. Discovery skips linked directories,
rejects a linked directory input, and stays within the selected input rather
than expanding to imported dependencies. Temporary build files are cleaned up
on completion or failure.


### `jik env`

Print resolved Jik configuration values as `key=value` lines, including the Jik
version, platform, root directory, `jiklib` path, package path, support include
path, selected C compiler, its reported target, and Jik's normalized compiler
profile.

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
3. The first available compiler from `clang`, `gcc`, then `cc`.

If no compiler is found, `jik run` and `jik build` fail before compilation.

## Package Imports

Imports under `pkg/...` are resolved from the directory configured by the
`JIK_PKG_PATH` environment variable.

The package entry module:

```jik
use "pkg/csv"
```

resolves to:

```text
<JIK_PKG_PATH>/packages/csv/src/csv.jik
```

Package submodules resolve below the package `src/` directory:

```jik
use "pkg/csv/reader"
use "pkg/csv/utils/foo"
```

```text
<JIK_PKG_PATH>/packages/csv/src/reader.jik
<JIK_PKG_PATH>/packages/csv/src/utils/foo.jik
```


Use `jik env` to check the currently resolved value. It is printed as
`pkg_path=<path>`. If `JIK_PKG_PATH` is not set and a program imports
`pkg/...`, compilation fails with a package path error.

## Common Workflows

- Use `jik check <file>` to check syntax, types, and regions.
- Use `jik run <file>` while developing a program.
- Use `jik tran <file>` when you want to inspect or format the generated C.
- Use `jik build <file> --release` when you want an optimized executable.
- Use `jik env` to debug how the CLI resolved paths and compiler settings.
