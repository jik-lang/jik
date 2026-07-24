[Back to overview](../overview.md)

# 8. Memory management in Jik

Jik manages composite values with **regions**. A region is an arena-like allocation area
into which composite values are allocated.

The key point for day-to-day programming is simple:

- primitive values such as `int`, `double`, `bool`, and `char` are plain values
- composite values such as `String`, `Vec[T]`, `Dict[T]`, structs, and variants live in regions

### 8.1 The local region

Every function that allocates composite values has a local region.
Composite literals without an explicit allocation target go there by default.

```jik
func main():
    name := "Alice"
    nums := [1, 2, 3]
end
```

Here both `name` and `nums` are allocated in `main`'s local region.

The local region is created at function entry, and destroyed at each return point
of the function. Because of that, we cannot return locally allocated values from
a function.

We can pass the local region to another function as an argument referred to as `_`.

Examples:

```jik
func make_label(r: Region) -> String:
    return "label"[r]
end

func main():
    label := make_label(_)   // allocate returned String in main's local region
    println(label)
end
```

#### 8.1.1 The global region

Composite globals are allocated in the global region. The global region is created
at program start and remains alive until program exit.

Global values, regardless if composite or not, are immutable: their members and container
elements cannot be changed. A function also cannot return a composite global
directly.

### 8.2 Choosing a non-local allocation target

Composite literals can also be allocated into another region:

```jik
p2 := Point{}[.p1]  // region of p1, where p1 is some composite value
p3 := Point{}[r]    // explicit region parameter r
```

Specifically, use:

- `[.x]` to allocate into the same region as an existing composite value `x`
- `[r]` to allocate into a parameter `r` of type `Region`

`[r]` and `[.x]` are called allocation specifiers. Allocation into a local region is only possible
without a given allocation specifier, so `[_]` is invalid syntax.

Allocation specifiers syntactically bind to their accompanying composite literal or type description.
This means the first trailing `[...]` after a composite literal is reserved for allocation syntax
such as `[r]` or `[.x]`.

Because of this, composite temporaries must be bound to a name before member access, subscripting,
mutation, or iteration. For example, writing `t := [1, 2][0]` is not allowed.

Example:

```jik
struct Person:
    name: String
end

func demo(p: Person):
    p1 := Person{name = "Alice"}     // locally allocated
    p2 := Person{name = "Bob"}[.p]   // allocated in region of "p"
    name := "foo"[.p]                // allocated in region of "p"
end
```

### 8.3 Returning composite values

Because a function's local region is destroyed when the function returns, a function cannot safely
return a composite value allocated in its own local region.

So functions that return composite values usually take a destination region:

```jik
struct Point:
    x: double
    y: double
end

func make_point(x, y, r: Region) -> Point:
    return Point{x = x, y = y}[r]    // OK: allocated caller-provided region 
end

func bad_point(x, y) -> Point:
    return Point{x = x, y = y}     // ERROR: allocated in this function's local region
end

func main():
    p1 := make_point(1.2, 0.23, _)
    p2 := Point{}
    p3 := make_point(0.01, 0.03, .p2)
end
```

This is an important pattern to remember: **the caller chooses where returned composite data lives**.

By convention, the region parameter is usually (but need not be) the last parameter in the function.


### 8.4 The same-region rule

#### 8.4.1 Motivation

A region owns all composite values allocated into it. When a region is destroyed,
all those composite values become invalid. Jik's region checks prevent a composite in one
region from storing references to composite data in an incompatible region.

In order for the Jik compiler to be able to check memory consistency without tracking
arbitrary object graphs, it imposes certain restrictions on user code.


#### 8.4.2 Same-region rule

For a call that the compiler cannot prove is **region-safe**, Jik enforces the
following rule:

- all non-`foreign` composite arguments and all `Region` arguments in one call must be in the same region

This default requirement is referred to as the **same-region rule**. Calls to
region-safe functions are a compiler-proven exception to the rule, and are described in
[Region-safe functions](#843-region-safe-functions).

As an example, take the following function:

```jik
struct Context:
    id: int
    parent: Option[Context]
end

func foo(v1: Vec[Context], v2: Dict[bool], ctx: Context, r: Region):
    push(v1, ctx)
end
```

The function above can store `ctx` in `v1`, so it is not region-safe, since `v1` and 
`ctx` can in general be allocated in different regions. No argument is prefixed with `foreign`,
so every call to it must pass `v1`, `v2`, `ctx`, and `r` from the same region.

Note once again that this applies only to composite arguments. Primitive values do not participate
in region checks and rules.

On the other hand, if the function was defined with:

```jik
func foo(foreign v1: Vec[int], foreign v2: Dict[bool], ctx: Context, r: Region) -> Context:
    return ctx
end
```

This function is not region-safe because it returns a composite value. Since there are multiple 
regions involved in the function signature, without the same-region rule, the compiler could not infer to which region the
returned value belongs. Its `foreign` inputs are excluded from the same-region rule, so `ctx` and
`r` must be in the same region.

Additionally for an argument of type `Region` "to be in a region" simply means that it equals
that region.

Examples:

```jik
struct Person:
    name: String
end

func rename(p: Person, name: String):
    p.name = name
end

func demo(p: Person):
    local_name := "Local"

    rename(p, local_name)       // ERROR: "p" and "local_name" are in different regions
    rename(p, "Alice"[.p])      // OK: "Alice" is explicitly allocated in region of "p"
    rename(p, "Alice")          // OK: compiler automatically allocates "Alice" in region of "p"
end
```


#### 8.4.3 Region-safe functions

The compiler does not apply the same-region rule to a user-defined function it
can prove is **region-safe**. A call to such a function may pass its composite
and `Region` arguments from different regions.

A function is region-safe when it cannot make one region reference a composite
value from another region. In practice, this means it may inspect values,
operate only on local values, or store an allocated value only into the same
 destination region. It must not return a composite value, store a
value from one parameter region into another, store a `foreign` parameter, or
call a function that is not itself region-safe.

For example, this read-only helper is region-safe, so its caller may pass 
values allocated in different regions:

```jik
func compare(left: String, right: String):
    println(left, right)
end

func demo(r: Region):
    compare("local", "other"[r])    // OK: `compare` is region-safe
end
```

The relaxation does not permit an unsafe store. This function is not
region-safe, so its callers still use the ordinary same-region rule:

```jik
func append(dst: Vec[String], value: String):
    push(dst, value)    // `value` may belong to a different caller region
end
```

If a value must be stored in another region, copy it explicitly into the
destination region first:

```jik
func append_copy(dst: Vec[String], value: String):
    push(dst, copy(value, .dst))
end
```

This is a compiler-inferred property, not a declaration or an opt-in. If the
function body later gains an unsafe operation, its calls again require the
same-region rule.

#### 8.4.4 More about `foreign`

A composite function argument marked as `foreign` will not be subject to the
same-region rule. The reason `foreign` exists is to provide a way to relax the
same-region rule for arguments which do not participate in composite stores,
but only need to be read from, or have their primitive contents changed.

`foreign` should not be added only because a function reads an input. If the helper is 
already inferred as region-safe, the compiler already permits its arguments to come from different
regions without `foreign`.

`foreign` has nothing to do with mutability. As long as the contents of a
`foreign` composite argument are primitive (for example, non-composite), they
can be freely mutated.

Furthermore, if multiple arguments are marked as `foreign`, stores between them are not allowed, as these
arguments can in general come from different regions, and this would break memory consistency.

Finally, returning an argument or a value rooted in a `foreign`-marked argument from a function
is not allowed. For a function call subject to the same-region rule, a composite
return value belongs to the same region as its non-`foreign` composite arguments
and any `Region` arguments (returning a local value is not allowed). Allowing a
`foreign` return would leave its region ambiguous.

This allows the Jik compiler to infer in which region class a function call result is allocated -
allowing `foreign` returns would make this impossible. 


#### 8.4.5 Consequences of the same-region rule

For a call subject to the same-region rule, the compiler can safely infer that
a composite return value belongs to the shared region of the function's
non-`foreign` composite parameters and any `Region` parameters. A function
therefore cannot return a locally allocated value, a `foreign` input, or a
composite global.

The main consequence is that the return value of a function which takes
composite parameters needs to be in the same region as the shared
region of the function's parameters.

This gives function return values a predictable lifetime without requiring the
compiler to track arbitrary object graphs. To return data in a different
region, copy it explicitly into a caller-provided destination region.


#### 8.4.6 Examples of store operations

Some examples of good vs. bad store operations:

```jik
func bad(y: Vec[Vec[int]]):
    t := [1, 2, 3]
    push(y, t)      // ERROR: stores local composite data into caller-owned vector
end

func good(y: Vec[Vec[int]]):
    t := [1, 2, 3][.y]
    push(y, t)      // OK, since "y" and "t" are in the same region
end
```

Composite values from different regions cannot be mixed through store
operations, as this could cause memory corruption. In the function `bad`, we
would write something stored in `bad`'s local region to a region outliving that
region (for example, the region where `y` is allocated), which would be wrong.

In the examples above, the compiler prevents storing local data in the region
of `y`.

Further examples:

```jik
func bad_store(names: Dict[String]):
    local_name := "Alice"
    names["user"] = local_name      // ERROR: local_name is locally allocated
end

func good_store(names: Dict[String]):
    names["user"] = "Alice"[.names]     // OK: "Alice" is allocated in region of "names"
    names["user2"] = "Bob"              // OK: compiler allocates "Bob" automatically in region of "names"
end
```


### 8.5 Region ergonomics and checking rules

For better ergonomics of everyday code, Jik introduces additional rules that make region programming 
more pleasant to deal with.

#### 8.5.1 Omitted final `Region` argument

If a function's final parameter has type `Region`, the caller may omit that argument.
In that case, the compiler automatically inserts `_`, the caller's local region. Since passing the
local region is a frequent scenario, this rule makes perfect sense.

Inside a global initializer, the omitted region uses the global region for that initializer.

```jik
func make_label(r: Region) -> String:
    return "label"[r]
end

func main():
    a := make_label()
    b := make_label(_)
end
```

Here `a` and `b` are equivalent.

#### 8.5.2 Literal retargeting

Although composite literals allocate in the local region in absence of a region specifier, Jik may retarget a local literal
allocation destination when the destination region is obvious. This is what makes common calls and stores manageable.
For example:

```jik
struct Person:
    name: String
    age: int
end

func set_name(p: Person, name: String):
    p.name = name
end

func main():
    p := Person{name = "Alice"}

    set_name(p, "Bob")   // "Bob" is allocated in p's region
    p.name = "Carol"     // "Carol" is allocated in p's region

    people := {"Alice": Person{}}
    people["Bob"] = Person{name = "Bob"}     // Person{name = "Bob"} is allocated in people's region

    v := [[1, 2], [3, 4]]
    push(v, [5, 6])     // [5, 6] is allocated in v's region
    v[0] = [0, 1, 2]    // [0, 1, 2] is allocated in v's region
end
```

#### 8.5.3 Nested composite literals

Composite literals that contain other composite values must be internally region-consistent.
The outer value and the contained composite values must belong to the same region.

This applies to:

- vector elements
- vector repeated initializers
- dictionary values
- struct initializer fields
- variant payloads
- `Some` payloads

For container literals, only an outer region specifier is possible. Hence, if container contents
are literals, these are automatically retargeted to the destination marked by the container's
allocation specifier.

For example:

```jik
func foo(v: Vec[String]):
    a := ["bar", "baz"][.v]   // "bar" and "baz" are automatically allocated in the region of v

    x := "local"
    y := ["foo", x][.v]   // compile error: x is local, but the vector is allocated in v's region
end
```

#### 8.5.4 Temporary containers passed to `foreign` parameters

A temporary `Vec` or `Dict` literal passed directly to a `foreign` parameter
may contain composite elements from different regions. This exception applies
only in that `foreign` argument position; ordinary container literals must
still be internally region-consistent. For example:

```jik
use "jik/process"


func foo(args: Vec[String], foreign cmd: String):
    res := must process::capture(cmd, 
        [
            "foo",
            "bar",
            args[0]
        ],
        _
    )
end
```

This works because the signature of `process::capture` is:

```jik
throws func capture(foreign program: String, foreign args: Vec[String], region: Region) -> Result
```

Since `args` in `process::capture` is a `foreign` vector, at the call site the elements of the vector literal (e.g. the strings)
need not be in the same region.


#### 8.5.5 Region-safe builtins

All currently provided builtins except `push` are region-safe, so their calls
do not need to satisfy the same-region rule. Some only inspect their composite
arguments; others, such as `concat` and `copy`, explicitly identify the
destination region for values they allocate.

`push` is checked as a store operation because it can retain its value argument
in a vector. A `push` of a primitive value is region-safe; for a composite
value, the compiler applies the usual store-safety checks.

`concat` is still explicit about its destination region: its last argument must be a `Region`, and the returned `String` is allocated there.
`copy` is also explicit about its destination region, but unlike ordinary calls its source value may come from any region.

```jik
func show(foreign left: String, right: String, r: Region):
    println(left, right)
    joined := concat(left, right, r)
    left_copy := copy(left, r)
end
```

### 8.6 Copying values between regions

Use the builtin `copy` to explicitly copy specific composite values into a chosen destination
region:

```jik
copy(value, region) -> T
copy(value)         -> T  // allocated in the local region
```

A concrete example:

```jik
func foo(v: Vec[int]):
    x := [1, 2, 3]
    x_new = copy(x, .v)   // copy x to the region of v
    v_new = copy(v, _)    // copy v to the local region
    return x_new
end
```

Supported copyable types are:

- `String`
- `Vec[P]`
- `Dict[P]`
- `Option[P]`
- structs whose fields are all `P`
- variants whose every payload type is `P`

Here `P` means `int`, `double`, `bool`, `char`, `String`, or an enum type. Strings are
primitive-like for this rule, but copied strings are freshly allocated in the destination
region.

Top-level primitives and enums are rejected because they are not composite:

```jik
copy(12, _)       // compile error
copy(State.ON, _) // compile error
```

Extern structs and nested composites such as `Vec[Vec[int]]` or a struct field of type
`Vec[String]` are not supported.


### 8.7 Summary

If you keep these rules in mind, the Jik region model is fairly straightforward:

1. Composite values live in regions.
2. `_` is the current function's local region.
3. Functions that return composite values usually take a destination `Region`.
4. Calls to functions whose final parameter has type `Region` may omit that argument. When omitted, the caller's local region is passed automatically.
5. Composite calls and stores must preserve region consistency, except calls to compiler-proven region-safe functions.
6. Use `foreign` for read-oriented inputs from other regions.
