[Back to overview](../overview.md)

# 16. Foreign Function Interface (C interop)

Within Jik, one can embed C code and expose it through a foreign function interface, using the `extern` keyword. One can expose functions and structs. Structs are always opaque and need to be handled through a dedicated API.

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

Extern structs cannot be created, read nor modified the same way as non-extern structs, since they are opaque.
This needs to be achieved through a specifically constructed API, as demonstrated above.


---
