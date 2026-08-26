[Back to overview](../overview.md)

# 7. Structs

Structs are product types with named fields. While creating structs, each field has either an
explicit default value or an implicit one derived from its type.

### 7.1 Declaring structs

```jik
struct Config:
    host: String
    port: int
    verbose: bool
end
```

- Field names (`host`, `port`, `verbose`) are in scope inside the struct declaration.
- The right-hand side gives the **type** of the field


### 7.2 Constructing and using structs

```jik
c1 := Config{}                      // all fields take their default values
c2 := Config{host = "localhost", verbose = true}        // override some fields

c1.port = 3
c1.host = "foo"
```

When a visible value has the same name as a field, its field initializer may
omit `= value`:

```jik
host := "localhost"
port := 8080
verbose := true

c2 := Config{host, port, verbose}
c3 := Config{host = "example.com", port, verbose}
```

`Config{host, port, verbose}` is exactly equivalent to
`Config{host = host, port = port, verbose = verbose}`. These entries are always
named, not positional: their order does not need to match the declaration order.

Struct values can be read and updated through their fields using the usual `.` syntax.

### 7.3 Uniform function calls

A function in the same module as a struct may be called through a struct value
when its first parameter has an explicit annotation for that struct type:

```jik
func set_port(c: Config, port: int):
    c.port = port
end

c1.set_port(8080) // shorthand for set_port(c1, 8080)
```

For an imported struct, the function is resolved in the module that defines the
struct. This is just method-like syntactic sugar for an ordinary function call.

Recursive struct cycles must pass through `Option[...]`.
For example, `next: Option[Node]` is valid, while `next: Node` and `items: Vec[Node]` are compile errors.

---
