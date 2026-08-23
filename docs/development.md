# Development notes and roadmap

## Repository layout

- `src/jik/` - C seed compiler source
- [`src/bootstrap/`](../src/bootstrap/) - Jik bootstrap compiler source (work in progress)
- `jiklib/` - Jik standard library modules
- `support/` - Jik support library
- `test/` - test suite
- `docs/` - language and development documentation
- `examples/` - example Jik programs
- `assets/` - repository assets
- `scripts/` - development scripts
- `tools/` - development tools, including syntax highlighting

## Building and testing

Build the C seed compiler from the repository root with `make`. Build the Jik
bootstrap compiler as `jik1` with `make boot`. Run the test suite with `make test`.

## Roadmap

### 0.1.0-alpha.x — implementation stabilisation and bootstrapping

- Keep the intended language syntax stable.
- Find and fix compiler and support-library issues.
- Extend the standard library where needed.
- Expand the test suite.
- Bootstrap the implementation by [rewriting Jik in Jik](../src/bootstrap/)

### Later

- Add concurrency support
