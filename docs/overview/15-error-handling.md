[Back to overview](../overview.md)

# 15. Error handling

Jik uses **throwing functions** and structured handling at the call site.
The current model is centered around four language constructs:

- `throws func` marks a function as one which may fail
- `fail(String[, int])` marks failure inside a throwing function
- `try ... except ... end` handles a failure locally
- `must f(...)` requires a throwing call to succeed

Errors are therefore handled as **control flow**.

### 15.1 Declaring a throwing function

A function which may fail must be declared with `throws func`:

```jik
throws func div_safe(x, y):
    if y == 0:
        fail("division by 0")
    end
    return x / y
end
```

Inside such a function, `fail(...)` marks the operation as failed. `fail(msg)` uses the default
error code `1`, while `fail(msg, code)` lets you set an explicit integer error code.
`fail(...)` also terminates the current control-flow path. Any statement that follows `fail(...)`
in the same block is unreachable and is a compile error.

### 15.2 Handling failures with `try / except`

A throwing call can be wrapped in a `try` block:

```jik
try x := div_safe(3, 0):
    print("ok: ", x)
except:
    print("error")
end
```

If the call succeeds, execution continues in the `try` body.
If it fails, the `except` branch runs instead.

The variable introduced in `try x := ...` is only valid in the `try` body.
Referring to it inside `except` or after the entire `try/except/end` construct is a compile error.

If the throwing function returns nothing, we simply write:

```jik
try foo():
    print("OK")
except:
    print("err")
end
```

### 15.3 Propagating failures

Within a `throws func`, `try` can be used in a declaration to pass a failure to
the caller instead of handling it locally:

```jik
throws func half_of_safe_value():
    x := try div_safe(10, 2)
    return x / 2
end
```

The value declared by `x := try ...` is in the enclosing scope. If the call
fails, the function returns through its error path before any following
statement runs.

### 15.4 Requiring success with `must`

When a failure does not need to be handled separately, use `must`:

```jik
x := must div_safe(10, 2)
```

`must` calls a throwing function and panics if the call results in an error.

The postfix form `call()!` has the same behavior and is convenient within
expressions:

```jik
x := div_safe(10, 2)!
print(div_safe(10, 2)! == 5)
```

`!` applies only to function calls. Use `try ... except ... end` when failure
needs local recovery.

### 15.5 Inspecting the current error

Inside an `except` block, the current error can be inspected with:

- `error_msg([Region]) -> String`
- `error_code() -> int`

`error_msg()` may be called with no argument, in which case the local region `_` is used.

Example:

```jik
try result := div_safe(10, 0):
    println("result = ", result)
except:
    println("failed: ", error_msg())
    println("code = ", error_code())
end
```

### 15.6 Example

```jik
throws func div_safe(x, y):
    if y == 0:
        fail("division by 0")
    end
    return x / y
end

func main():
    try result := div_safe(10, 0):
        println("result = ", result)
    except:
        println("division failed")
    end

    ok := must div_safe(10, 2)
    println(ok)
end
```

---
