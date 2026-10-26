# Development notes and roadmap

## Repository layout

- [`src/jik/`](../src/jik/) - primary compiler source, written in Jik
- `src/seed/` - C seed compiler source
- `jiklib/` - Jik standard library modules
- `support/` - Jik support library
- `test/jik/` - maintained compiler and language tests
- `test/seed/` - frozen seed compiler tests
- `docs/` - language and development documentation
- `examples/` - example Jik programs
- `assets/` - repository assets
- `scripts/` - development scripts
- `tools/` - development tools, including syntax highlighting

## Building and testing

Run these commands from the repository root with Make and Clang or GCC:

```sh
make                       # Build jik using the bundled C seed
make test                  # Build jik and run compiler/language tests
make clean                 # Remove generated build and release outputs
make help                  # Show available build commands
```

Use `make` for everyday development. It compiles `src/seed/` as `jik-seed`, then
builds `src/jik/main.jik` as `jik`. No installed Jik or download is required. Both
executables are rebuilt on each call and have `.exe` on Windows.

Use `make CC=gcc` or `make CC=clang` to select the C compiler. Quote assignments
containing spaces, for example `"CC=C:/Program Files/LLVM/bin/clang.exe"`.

`make test-seed` runs the seed's frozen tests. `make test-bootstrap-full` runs
those tests, then builds and tests three optimized compilers:

```text
jik-seed -> jik-stage1 -> jik-stage2 -> jik-stage3
```

Stages 2 and 3 must generate identical C, both with and without the embedded
runtime. Comparison output is retained in `build/bootstrap-sequence/`.

Compiler executables stay at the repository root and use the checkout's
`jiklib/`, `support/`, and `docs/` directly. CLI builds clean up their temporary
C automatically. New language features belong in `src/jik/`; the bootstrap
strategy can be revisited if the seed can no longer build the compiler.

## Release builds

`make release` performs the full bootstrap above and stages the exact tested
stage-2 compiler as `release/jik` (`release/jik.exe` on Windows). The development
compiler is built by the C seed with default optimization; the release compiler
is built by the self-hosted stage-1 compiler with `-O3` and receives full
self-hosting verification. Both implement the same language.

`make release-archive VERSION=v0.1.0-beta.0` performs the same checks and creates
an archive in `artifacts/`, containing the stage-2 executable named `jik`/`jik.exe`,
`LICENSE`, `jiklib/`, `support/`, `docs/`, and `examples/`. Stage 3 verifies
self-hosting; it is not distributed.

## Roadmap

### 0.1.0-beta.0

- Keep the intended language syntax stable.
- Find and fix compiler and support-library issues.
- Extend the standard library where needed.
- Expand the test suite.
- Build and test the [compiler written in Jik](../src/jik/).

### Later

- Add concurrency support
