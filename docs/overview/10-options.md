[Back to overview](../overview.md)

# 10. Options

`Option[T]` is used when a value of type `T` may or may not be present.

### 10.1 Creating options

An option has two states:

- `Some{value}` means a value is present
- `None` means no value is present

Examples:

```jik
a := Some{3}
b: Option[int]
c: Option[String]
```

`a` has type `Option[int]`.
Both `b` and `c` default to `None`.

When writing `None` directly, the payload type must be known from context:

```jik
x: Option[int]
x = None
```

### 10.2 Checking options

Use `is Some` and `is None`:

```jik
item := Some{"hello"}

if item is Some:
    println("present")
end

if item is None:
    println("missing")
end
```

These checks always produce `bool`.

### 10.3 Extracting the payload

If `x` has type `Option[T]`, then `x?` has type `T`:

```jik
item := Some{7}
println(item?)
```

Applying `?` to `None` is a runtime error, so it is usually used after an `is Some` check when absence is possible.
Another way to extract the payload is to use the builtin function `unwrap`, so `unwrap(x)` is equivalent to `x?`.

### 10.4 Common uses

Options are the natural way to model:

- dictionary lookup results
- recursive links such as `next: Option[Node]`

Examples:

```jik
ages: Dict[int]
ages["Alice"] = 31

if ages["Alice"] is Some:
    println(ages["Alice"]?)
end
```

```jik
struct Node:
    next: Option[Node]
end
```

Recursive struct and variant cycles must pass through `Option[...]`.
Direct cycles such as `next: Node` or `items: Vec[Node]` are compile errors.

---
