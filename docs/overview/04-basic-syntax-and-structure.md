[Back to overview](../overview.md)

# 4. Basic Syntax and Structure

Jik uses **explicit block delimiters** and does not rely on indentation. However, there is only one sane
way to write readable Jik code - which is consistent indentation per block.

A Jik module may consist of the following top-level entities:

- global declarations written with `:=`
- functions
- data type definitions (enums, structs, variants)


### 4.1 Functions

Functions are declared with `func` and closed with `end`:

```jik
func add(x, y):
    return x + y
end
```

- Arguments are written with or without types; the compiler infers / assigns C types from usages (calls)
- `return` exits the function with a value.
- each Jik program needs to have a `main` function defined, which is treated as the program entry point

A version of the same function with types would look like:

```jik
func add(x: int, y: int) -> int:
    return x + y
end
```

This is useful for writing library functions which have no usages/calls which can be used for inference by the
compiler.


### 4.2 Blocks and control keywords

Blocks are always closed with `end`, for example:

- `if ... end`
- `while ... end`
- `for ... end`
- `match ... end`
- `struct ... end`
- `variant ... end`
- `func ... end`

Each block introduces a new scope.

However, Jik currently uses stricter redeclaration rules than C-style block shadowing:

- an inner local scope may not redeclare a name that already exists in an outer local scope of the same function
- this includes parameters and loop variables from enclosing scopes
- a local declaration also may not shadow a same-module global

A colon `:` follows most control headers:

```jik
if n < 0:
    return -n
end
```

### 4.3 Comments

Use `//` for single-line comments:

```jik
// This is a comment
x = x + 1      // trailing comment
```

### 4.4 Arithmetic and Logical operators

Arithmetic and comparison operators look as in C-style languages:

```jik
x := 3 + 4 * (8 - 7)
ok := (x >= 10) and (x != 13)
```

Supported comparisons include: `==`, `!=`, `<`, `>`, `<=`, `>=`.


Boolean expressions use:

```jik
not a
a and b
a or b
```
with the usual short-circuit behavior (mirroring C).

---
