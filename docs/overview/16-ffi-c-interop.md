[Back to overview](../overview.md)

# 16. Foreign Function Interface (C interop)

Within Jik, one can embed C code and expose it through a foreign function
interface, using the `extern` keyword. One can expose functions and structs.
Extern structs are opaque handle types. Their fields are not visible to Jik,
so construction and access must go through extern functions.

### 16.1 Extern functions

Extern functions are declared like so:

```jik
extern func impl_adder as adder(x: int, y: int) -> int
```

This assumes that `impl_adder` is either available through the Jik support library (`core.h`), or that it is embedded
within Jik code, like so:

```jik
@embed{C_END}

int32_t
impl_adder(int32_t x, int32_t y)
{
    return x + y;
}

C_END
```

Here, `C_END` is a user-defined label which marks the end of embedded C code. The idea is that this label should
not be contained in any of the C code, so it can correctly be processed by the Jik compiler.


One can then use it like any other function:

```jik
assert(adder(3, 2) == 5)
```

### 16.2 Extern structs

Regarding structs, here is an example:

```jik
extern struct impl_Point as Point
extern func impl_Point_new as point_new(x: double, y: double, region: Region) -> Point
extern func impl_Point_x as point_x(p: Point) -> double

@embed{C_END}

typedef struct impl_Point {
    double x;
    double y;
} impl_Point;

impl_Point *
impl_Point_new(double x, double y, JikRegion *r)
{
    impl_Point *p = jik_region_alloc(r, sizeof(impl_Point));
    p->x = x;
    p->y = y;
    return p;
}

double
impl_Point_x(impl_Point *p)
{
    return p->x;
}


C_END

```

Typically, when creating composite objects, we need to pass a region where they should be allocated.

Extern structs cannot be read or modified the same way as non-extern structs,
since they are opaque. Field access and mutation must be exposed through a
specifically constructed API, as demonstrated above.

### 16.3 Default initialization for extern structs

Some extern structs have a meaningful empty value. These can declare one
extern function as the type's default initializer:

```jik
extern struct impl_Buffer as Buffer

extern init func impl_Buffer_new as
    new(region: Region) -> Buffer
```

An `extern init func` must:

- return an extern struct type
- take exactly one `Region` parameter
- not be `throws`
- be the only init function declared for that extern struct type

The public function name is not special. `new` is only a convention; the
`init` marker is what makes the function a default initializer.

When an extern struct has an init function, default construction uses it:

```jik
b1: Buffer
b2 := Buffer{}
```

Both declarations above are lowered as if they called the marked init function
with the current allocation region:

```jik
b := new(_)
```

Only empty construction is allowed. Extern structs are still opaque, so a
fielded construction such as `Buffer{x = 1}` is invalid.

Extern structs without an init function cannot be default-constructed:

```jik
f: io::File // error because io::File has no extern init function
```

### 16.4 Native build requirements

An FFI module can declare the include paths, library paths, libraries, and runtime files needed to
build programs that import it:

```jik
@includedir("../vendor/raylib/include")

@platform(windows)
@libdir("../vendor/raylib/lib/windows")
@link("raylib", "gdi32", "winmm")
@copy("../vendor/raylib/bin/windows/raylib.dll")

@platform(linux)
@libdir("../vendor/raylib/lib/linux")
@link("raylib", "m", "pthread", "dl")
```

Every module begins in platform `all`. A `@platform` directive selects the platform for later build
directives in the same physical file; `@platform(all)` restores platform-independent behavior.
Relative paths are resolved from the file containing the directive.

Requirements are collected transitively from every reachable module. `@includedir` adds header
search directories, `@libdir` adds library search directories, `@link` links named libraries, and
`@copy` copies files beside the executable after a successful compilation. Each directive accepts
one or more comma-separated string arguments.

---
