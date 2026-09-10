[Back to overview](../overview.md)

# 11. Variants

Variants are tagged unions with safe payload access. Their tags may or may not carry a payload.


### 11.1 Declaring variants

Examples:

```jik
variant Value:
    INT: int
    TEXT: String
    NUMS: Vec[int]
    EOF
end
```

### 11.2 Constructing variants

```jik
v1 := Value.INT{7}
// If initial value is not given, it is given the default initializer value for that type
// For this case, it is 0
v2 := Value.INT{}

v3 := Value.TEXT{"hello"}
v4 := Value.NUMS{[10 of 0]}
v5 := Value.EOF{}
```


### 11.3 Inspecting, extracting and changing tags

Use `is` to check which variant tag is active. Following up on the declarations above:

```jik
assert(v1 is INT)
assert(v2 is INT)
assert(v3 is TEXT)
assert(v4 is NUMS)
assert(v5 is EOF)
```

The variant type is inferred from the value on the left of `is`. Payloads use
checked member access:

```jik
nums := v4.NUMS
assert(nums[0] == 0)
```

This reads as "give me the `NUMS` payload of this `Value`". The generated code
checks that `NUMS` is active and reports a runtime error otherwise. A payload
may be updated through the same checked access without changing the active tag:

```jik
v1.INT = 8
```

We can modify a variant instance by setting another tag as active:

```jik
v3 = Value.INT{2}
assert(v3 is INT)
```

### 11.4 Pattern matching on variants

Jik supports `match` for more concise handling of variants and enums. A variant match uses tag
patterns and can bind payloads:

```jik
func handle(val):
    match val:
        case INT{v}:
            print("INT: ", v)
        case TEXT{msg}:
            print("TEXT: ", msg)
        case NUMS{vec}:
            print("NUMS: ", vec)
        case EOF:
            print("end of input")
    end
end
```

Each `case`:

- Tests the tag
- **Binds** the payload to a local name (`v`, `msg`, `vec`).

The matched expression determines the variant or enum type, so case tags do not
need qualification. A payload-bearing tag may omit `{name}` when its payload is
not needed. Construct payloadless tags with empty braces, as with `Value.EOF{}`,
and match them without braces, as with `case EOF:`. If every tag has no payload,
declare an `enum` instead of a `variant`.

When printed, variants show their type, active tag, and payload when present: for example,
`<Value INT=7>` and `<Value EOF>`.

By default, `match` is exhaustive: every variant tag or enum member must be handled by a
respective `case`, otherwise there is a compile error.

A final `other:` arm handles every tag not covered by an explicit case. It cannot bind a payload.
When `other:` is present, the match need not list every tag. Without it, the normal exhaustiveness
requirement applies.

```jik
match val:
    case INT{v}:
        print("integer: ", v)
    other:
        print("another value")
end
```

Variants can be placed freely into vectors, dictionaries, and other structs, and behave as regular values.

Recursive cycles involving variant payloads must also pass through `Option[...]`.
Payloads such as `Expr`, `Vec[Expr]`, or `Dict[Expr]` that recurse back without `Option` are compile errors.

### 11.5 Uniform function calls

A function in the same module as a variant may be called through a value of
that variant when its first parameter has an explicit annotation for the
variant type:

```jik
func is_text(value: Value) -> bool:
    return value is TEXT
end

v := Value.TEXT{"hello"}
assert(v.is_text()) // shorthand for is_text(v)
```

For an imported variant, the function is resolved in the module that defines
the variant, just as it is for an imported struct.

A [table](14-tables.md) can associate one stored value with every variant tag. Such a lookup
considers only the active tag and does not inspect its payload.

---
