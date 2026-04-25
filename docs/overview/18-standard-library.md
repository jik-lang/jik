[Back to overview](../overview.md)

# 18. Standard library

Jik provides a small but useful standard library. The modules below cover common tasks such as
strings, input/output, math, randomness, and testing.

- [jik::std](../jiklib/std.md) - basic utility functions
- [jik::char](../jiklib/char.md) - character-related helpers
- [jik::string](../jiklib/string.md) - string operations
- [jik::strbuf](../jiklib/strbuf.md) - mutable string buffer support
- [jik::io](../jiklib/io.md) - file and stream I/O
- [jik::region](../jiklib/region.md) - region allocator diagnostics
- [jik::rand](../jiklib/rand.md) - pseudo-random number generation
- [jik::sys](../jiklib/sys.md) - system-level utilities
- [jik::math](../jiklib/math.md) - mathematical functions and constants
- [jik::testing](../jiklib/testing.md) - support for writing tests

These modules are imported in the same way as other Jik modules, but using paths under `jik/...`. For
example:

```jik
use "jik/math"
use "jik/testing" as test
```

---
