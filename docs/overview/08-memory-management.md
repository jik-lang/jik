[Back to overview](../overview.md)

# 8. Memory management in Jik

Jik manages composite values with **regions**. A region is an arena-like allocation area:
composite values are allocated into it, and the whole region is reclaimed together.

The key point for day-to-day programming is simple:

- primitive values such as `int`, `double`, `bool`, and `char` are plain values
- composite values such as `String`, `Vec[T]`, `Dict[T]`, structs, and variants live in regions

### 8.1 The local region

Every function that allocates composite values has a local region, written as `_`.
Composite literals without an explicit allocation target go there by default.

```jik
func main():
    name := "Ana"
    nums := [1, 2, 3]
end
```

Here both `name` and `nums` are allocated in `main`'s local region.

### 8.2 Choosing an allocation target

Composite literals can also be allocated into another region:

```jik
p1 := Point{}       // local region
p2 := Point{}[.p1]  // region of p1
p3 := Point{}[r]    // explicit region parameter
```

Use:

- `_` for the current function's local region
- `[r]` to allocate into a region parameter
- `[.x]` to allocate into the same region as an existing composite value `x`

Allocation suffixes bind directly to composite literals and type descriptions.
This means the first trailing `[...]` after a composite literal is reserved for
allocation syntax such as `[r]` or `[.x]`.

Because of this, composite temporaries must be bound to a name before member
access, subscripting, mutation, or iteration.

For example, this is not allowed:

```jik
t := [1, 2][0]
```

Write:

```jik
tmp := [1, 2]
t := tmp[0]
```

### 8.3 Returning composite values

Because a function's local region is destroyed when the function returns, a function cannot safely
return a composite value allocated in its own local region.

So functions that return composite values usually take a destination region:

```jik
struct Person:
    name: String
    age: int
end

func make_person(name, age, r: Region) -> Person:
    return Person{name = name, age = age}[r]
end

func main():
    p1 := make_person("Ana", 30, _)
    p2 := Person{}
    p3 := make_person("Ivo", 40, .p2)
end
```

This is the main pattern to remember: **the caller chooses where returned composite data lives**.

### 8.4 The role of `foreign`

Sometimes a function needs to read composite inputs from one region and allocate its result in another.
That is what `foreign` is for.

```jik
func concat_names(foreign left: String, foreign right: String, r: Region) -> String:
    return string::concat(left, right, r)
end
```

Here `left` and `right` are read-oriented inputs for region checking, while the result is allocated in `r`.

Many standard-library functions use this shape: take one or more `foreign` inputs and a final destination `Region`.

### 8.5 The same-region rule

For ordinary function calls, Jik enforces a simple rule:

- all non-`foreign` composite arguments and all `Region` arguments in one call must agree on the same region class

In practice, this means you cannot freely mix:

- local composite values from `_`
- caller-owned composite values
- destination `Region` arguments

unless some inputs are explicitly marked `foreign`.

This is what prevents composite data from being accidentally written across incompatible lifetimes.

### 8.6 Composite stores cannot mix regions

The same idea applies to storing composite values.

If a destination lives in one region, you cannot store a composite value coming from a different non-matching region into it.
For example, it is illegal to store a local composite into a container that lives in a caller-owned region.

So, as a rough rule:

- primitive values can be assigned freely
- composite values can only be stored when source and destination belong to the same writable region class

This is one of the main reasons Jik asks you to be explicit about allocation destinations.

### 8.7 Useful conveniences

Jik applies a few rules to make region use less noisy:

- if a function's last parameter is `Region`, omitting it passes `_`
- allocated literals in common calls and stores are often placed into the destination region automatically

For example:

```jik
func set_name(p: Person, name: String):
    p.name = name
end

func main():
    p := Person{}
    set_name(p, "John")
end
```

The `"John"` literal is allocated in the region of `p`, so this common pattern works naturally.

These rules do not remove the region checks. They just make common code less noisy, so region-related compiler errors do not come as a surprise only after more complex examples.

### 8.8 Practical restrictions

The compiler enforces a few important rules:

- a region value cannot be created, aliased, or returned directly
- a local composite allocation cannot be returned from a function
- composite globals are immutable
- recursive struct and variant cycles must pass through `Option[...]`

If you keep three ideas in mind, most Jik code will feel natural:

1. composite values live in regions
2. `_` is the current function's local region
3. functions that return composite values usually take a destination `Region`
4. composite calls and stores cannot freely mix regions unless `foreign` is used appropriately

---
