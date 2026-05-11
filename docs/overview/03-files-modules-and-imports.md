[Back to overview](../overview.md)

# 3. Files, Modules, and Imports

Jik code lives in plain `.jik` files. Each file is its own **module**.

### 3.1 Importing modules

You import modules with `use`:

```jik
use "jik/testing" as test
use "app/utils"
```

- The string is a **module path** which points to a `.jik` file with the same name
- Module paths always use `/` as the separator, on all platforms.
- `as` gives the module a short alias (`test` in this example).
- If you omit `as`, the module is referred to by its basename (`utils` in the example above).
- Import aliases must be unique within a module. This applies to both explicit `as` aliases and implicit basename aliases.

Modules are resolved relative to the directory of the file that contains the `use` declaration.
Standard library modules are prefixed with `jik` and are resolved relative to the `jik`
installation directory.

During compilation, Jik resolves all modules reachable from the main module (e.g. the one which is the input to
the compiler), and creates a meaningful compilation order for the modules.


### 3.2 Qualified names

Symbols from a module are accessed using `::`:

```jik
use "jik/io"
use "jik/testing" as test

func main():
    ts := test::TestSuite{}
    text := must io::read_file("notes.txt", _)
    test::suite_assert(ts, len(text) > 0, site())
end
```

Top-level symbols from another module are accessed via `module::name`.

This includes:

- functions
- global declarations
- structs
- enums
- variants

For example, qualified type names such as `test::TestSuite`, `shapes::Person`, and `msg::Message` are valid in the same way as qualified functions such as `io::read_file`.

Jik does not currently present separate user-visible namespaces for types versus other top-level declarations inside a module.
By convention, symbols whose names start with `_` are considered internal implementation details.
This is not enforced by the compiler in the current language.

---
