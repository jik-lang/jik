[Back to overview](../overview.md)

# 17. Builtins

Builtins are functions which are part of the language and are always available, without any additional `use` statements.

Builtin names may not be reused for non-callable ordinary declarations such as globals, locals, parameters,
loop variables, match-bound variables, or top-level type names.

---

### 17.1 Output

#### `print(...) -> void`

Takes any number of arguments and writes each argument to the standard output using the argument's
default textual representation.
Each argument can be of any type.
Options print as `<Some: val>` or `<None[T]>`, for example:

```jik
println(Some{12})
println(None[String])
```


#### `println(...) -> void`
Same as `print`, and additionally writes a newline after the arguments.

#### `concat(...String, Region) -> String`
Concatenates one or more strings and allocates the result in the final `Region`
argument.

All arguments except the last must be of type `String`. The last argument must
be a `Region`.

**Example**
```jik
msg := concat("hello", ", ", name, "!", _)
```

---

### 17.2 Container builtins

#### `push(Vec[T], T) -> void`
Pushes a value of type `T` to the end of a vector of type `Vec[T]`.

**Parameters**
1. Vector
2. Value to push

```jik
v := [1, 2]
push(v, 10)
```

#### `pop(Vec[T]) -> T`
Pops the last element from a vector of type `Vec[T]`.

**Parameters**
1. Vector

**Returns**
- A value of type `T`.

**Example**
```jik
v := [1, 2, 3]
assert(pop(v) == 3)
```

#### `len(T) -> int`
Returns the length of a container or string.

**Parameters**
1. Value of type `Vec`, `Dict` or `String`

**Returns**
- Length of the container or string

**Example**
```jik
n = len(v)
m = len(s)
```

#### `clear(T) -> void`
Resets the size of a vector or dictionary to 0 and does not free memory.

**Parameters**
1. Value of type `Vec` or `Dict`

**Example**
```jik
d := {"foo": 12, "bar": 13}
clear(d)
```

---

### 17.3 Debugging

Debugging builtins return information about a call site. Basic debugging information is obtained
through the opaque type `Site`.


#### `site() -> Site`
Returns an opaque structured representation of the current site information.

**Returns**
- value of type `Site`

#### `site_file(Site, Region) -> String`
Returns the source file path/name for the current site, as a string allocated in the given region.

**Parameters**
1. Site
2. Region to allocate the filepath

**Returns**
- filepath


#### `site_line(Site) -> int`
Returns the source line number for the current site.

**Parameters**
1. Site

**Returns**
- line number

#### `site_code(Site, Region) -> String`
Returns the Jik source code line for the current site, as a string allocated in the given region.

**Parameters**
1. Site
2. Region to allocate the codeline

**Returns**
- code line

**Example**
```jik
s := site()
println(site_file(s, _), site_line(s), site_code(s, _))
```

---

### 17.4 Misc

#### `assert(bool) -> void`
Checks a condition and fails (aborts execution) if the condition is false.

**Parameters**
1. Boolean expression


**Example**
```jik
v := [10 of 1]
assert(len(v) == 10)
```

#### `fail(String[, int]) -> void`
Marks the current throwing function as failed. See [Error Handling](15-error-handling.md).

#### `error_msg([Region]) -> String`
Returns the current error message. Most commonly used inside `except`. See [Error Handling](15-error-handling.md).

#### `error_code() -> int`
Returns the current error code. See [Error Handling](15-error-handling.md).
