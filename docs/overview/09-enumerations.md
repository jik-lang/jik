[Back to overview](../overview.md)

# 9. Enumerations

Enumerations define a type whose values come from a fixed set of named cases. They are useful when
you want to model a small closed set of states or options.

An enumeration is defined like:

```jik
enum State:
    ON
    OFF
end
```

And used as follows:

```jik
s1 := State.ON
s2 := State.OFF
assert(s1 != s2)
assert(s1 == State.ON)
```

Enumeration values are qualified with the enum name, so `State.ON` and `State.OFF` are distinct
values of the type `State`.

Enums work naturally with control flow:

```jik
func is_on(s):
    if s == State.ON:
        return true
    end
    return false
end
```

When code needs to handle every possible enum value, use an exhaustive `match`. Every enum member
must be handled exactly once, so adding a new member makes affected matches fail at compile time
until they are updated.

```jik
func signal_name(s: State, r: Region) -> String:
    match s:
        case State.ON:
            return "on"[r]
        case State.OFF:
            return "off"[r]
    end
end
```

Enum cases must be qualified, including for imported enums. `case State.ON:` is valid; bare
`case ON:` is not.

Use enums when the possible values are known in advance and do not need to carry additional data.
If each case needs associated values, variants are the more general construct.

---
