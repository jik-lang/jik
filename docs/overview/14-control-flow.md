[Back to overview](../overview.md)

# 14. Control Flow

Jik's control flow is straightforward.

### 14.1 `if / elif / else`

```jik
func is_less_than_zero(x):
    if x < 0:
        return true
    elif x == 0:
        return false
    else:
        return false
    end
end
```

`elif` and `else` are both optional, and one can have multiple `elif` branches.

Jik also supports an expression form for simple conditional selection:

```jik
t := 2 if foo == 3 else 12
name := "yes"[r] if flag else "no"[r]
```

Both branches must produce compatible value types, and the middle condition must be `bool`.

### 14.2 `while` loops

```jik
func sum_to(n):
    i := 0
    s := 0
    while i <= n:
        s += i
        i += 1
    end
    return s
end
```

The loop condition is re-evaluated on each iteration; `break` and `continue` keywords are also supported,
as in other iteration constructs.

### 14.3 `for` loops

Jik has two `for` patterns: numeric ranges and vector / dictionary iteration.

**Numeric `for`:**

```jik
n := 100
for i = 0, n:
    // do something
end
```

**Container iteration:**

```jik
// Vector iteration
points := [3 of Point{x = 2}]

for p in points:
    print(p.y - p.x)
end

// Dictionary iteration
config := {"foo": 12, "bar": 1}

for key, value in config:
    print(key, ": ", value)
end
```

---
