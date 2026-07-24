# Development notes and roadmap

## Repository layout

- `src/` - compiler source
- `jiklib/` - Jik standard library modules
- `support/` - Jik support library
- `test/` - test suite
- `docs/` - language and development documentation
- `examples/` - example Jik programs
- `assets/` - repository assets
- `scripts/` - development scripts
- `tools/` - development tools, including syntax highlighting

## Building and testing

Build the compiler from the repository root with `make`. Run the test suite with
`make test`.

## Roadmap

### 0.1.x — implementation stabilisation

- Keep the intended language syntax stable.
- Find and fix compiler and support-library issues.
- Extend the standard library where needed.
- Expand the test suite.
- Validate the implementation by writing real Jik programs.

### Later

- Bootstrap the implementation by rewriting Jik in Jik.
- Add concurrency support, either in the C implementation or during bootstrapping.
