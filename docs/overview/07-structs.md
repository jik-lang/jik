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

Struct values can be read and updated through their fields using the usual `.` syntax.

Recursive struct cycles must pass through `Option[...]`.
For example, `next: Option[Node]` is valid, while `next: Node` and `items: Vec[Node]` are compile errors.

---
