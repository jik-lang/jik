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

Use enums when the possible values are known in advance and do not need to carry additional data.
If each case needs associated values, variants are the more general construct.

---
