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
v5 := Value.EOF
```


### 11.3 Inspecting, extracting and changing tags

Use `is` to check which variant tag is active. Following up on the declarations above:

```jik
assert(v1 is Value.INT)
assert(v2 is Value.INT)
assert(v3 is Value.TEXT)
assert(v4 is Value.NUMS)
assert(v5 is Value.EOF)
```

Payload extraction is done with an index-like syntax:

```jik
nums := v4[Value.NUMS]
assert(nums[0] == 0)
```

This reads as "treat this value as the `Value.NUMS` case and give me its payload".
In the translated C code, it is checked if the active tag is accessed or not. In the latter case, a runtime error is thrown.

We can modify a variant instance by setting another tag as active:

```jik
v3 = Value.INT{2}
assert(v3 is Value.INT)
```

### 11.4 Pattern matching on variants

Jik also supports `match` for more concise variant handling:

```jik
func handle(val):
    match val:
        case Value.INT{v}:
            print("INT: ", v)
        case Value.TEXT{msg}:
            print("TEXT: ", msg)
        case Value.NUMS{vec}:
            print("NUMS: ", vec)
        case Value.EOF:
            print("end of input")
    end
end
```

Each `case`:

- Tests the tag
- **Binds** the payload to a local name (`v`, `msg`, `vec`).

Tags may omit their payload type. Construct and match those tags without braces, as with
`Value.EOF`; braces always indicate a payload. If every tag has no payload, declare an `enum`
instead of a `variant`.

When printed, variants show their type, active tag, and payload when present: for example,
`<Value INT=7>` and `<Value EOF>`.

It is important to note that `match` is exhaustive, which means it requires all tags to be treated
by a respective `case`, otherwise there is a compile error.

Variants can be placed freely into vectors, dictionaries, and other structs, and behave as regular values.

Recursive cycles involving variant payloads must also pass through `Option[...]`.
Payloads such as `Expr`, `Vec[Expr]`, or `Dict[Expr]` that recurse back without `Option` are compile errors.

---
