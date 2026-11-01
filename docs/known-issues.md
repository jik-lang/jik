# Known limitations

Jik is in beta. Bugs and changes are still expected.

## Platforms and tools

- Windows x64 and Linux x64 are the tested release platforms. Use Clang or GCC;
  MSVC is not supported.
- Generated C uses some GCC/Clang extensions, including for `[n of expr]` vector
  initializers. Other C compilers may reject it.
- Generated C can produce compiler warnings.

## Source builds

The bundled C seed still runs build commands through a shell. Paths containing
special shell characters may cause problems. The primary Jik compiler passes
program arguments directly, without a shell.

## Memory

- Region memory may not meet the alignment needed by some C types.
