# C seed compiler

This is Jik's original compiler, written in C. It was moved from `src/jik/`
to `src/seed/` in commit `24a3c8d`. Its version is `0.1.0-alpha.29`, as
recorded in [version.h](version.h). It keeps that version; the beta compiler
is the compiler written in Jik under [src/jik/](../jik/).

Normal `make` compiles this seed as `jik-seed`, then uses it to build the
primary compiler in `src/jik/` as `jik` (with `.exe` on Windows).
Only Make and a supported C compiler are needed.

Run from the repository root:

```sh
make seed                 # Build only the seed
make test-seed            # Run its frozen tests in test/seed/
make test-bootstrap-full  # Test the seed and three self-hosted stages
```

The seed is frozen; new language features belong in `src/jik/`. Revisit the
bootstrap strategy if it can no longer build the current compiler. See the
[development notes](../../docs/development.md) for verification and packaging.
