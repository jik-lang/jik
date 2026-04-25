# math

Minimal scalar math utilities.

This module is a thin wrapper over a small subset of C `<math.h>`.
Trigonometric functions use radians.

Notes:
- Floating-point behavior follows the platform C math library (NaN/Inf propagation).
- `is_nan` / `is_finite` return `int` (0 or 1) to avoid assuming a specific `bool` ABI.

## Types

_None._

## Functions

### `abs(x: double) -> double`

Compute the absolute value.

**Parameters**
1. `x: double` - Value.

**Returns**
- Absolute value of `x`.

---

### `approx_eq(a: double, b: double, eps: double) -> bool`

Check whether two values are equal within a tolerance.

**Parameters**
1. `a: double` - First value.
2. `b: double` - Second value.
3. `eps: double` - Non-negative tolerance.

**Returns**
- True if values are within tolerance, else false.

**Notes**
- Returns true if `abs(a - b) <= eps`.

---

### `atan2(y: double, x: double) -> double`

Compute the arctangent of `y/x` using the signs of `y` and `x`.

**Parameters**
1. `y: double` - y coordinate.
2. `x: double` - x coordinate.

**Returns**
- Angle in radians.

**Notes**
- The result is in radians.

---

### `cos(x: double) -> double`

Compute the cosine of an angle in radians.

**Parameters**
1. `x: double` - Angle in radians.

**Returns**
- Cosine of `x`.

---

### `tan(x: double) -> double`

Compute the tangent of an angle in radians.

**Parameters**
1. `x: double` - Angle in radians.

**Returns**
- Tangent of `x`.

---

### `floor(x: double) -> double`

Round down to the next integer value.

**Parameters**
1. `x: double` - Value.

**Returns**
- Largest integer value not greater than `x`.

---

### `ceil(x: double) -> double`

Round up to the next integer value.

**Parameters**
1. `x: double` - Value.

**Returns**
- Smallest integer value not less than `x`.

---

### `round(x: double) -> double`

Round to the nearest integer value.

**Parameters**
1. `x: double` - Value.

**Returns**
- Nearest integer value to `x`.

**Notes**
- Halfway cases follow the platform C math library.

---

### `pow(x: double, y: double) -> double`

Raise a value to a power.

**Parameters**
1. `x: double` - Base value.
2. `y: double` - Exponent.

**Returns**
- `x` raised to the power `y`.

---

### `log(x: double) -> double`

Compute the natural logarithm.

**Parameters**
1. `x: double` - Value.

**Returns**
- Natural logarithm of `x`.

---

### `exp(x: double) -> double`

Compute the natural exponential.

**Parameters**
1. `x: double` - Value.

**Returns**
- e raised to the power `x`.

---

### `fmod(x: double, y: double) -> double`

Compute the floating-point remainder (C `fmod`).

**Parameters**
1. `x: double` - Numerator.
2. `y: double` - Denominator.

**Returns**
- Remainder.

---

### `is_finite(x: double) -> int`

Test whether the value is finite.

**Parameters**
1. `x: double` - Value.

**Returns**
- 1 if finite (not NaN and not +/-Inf), else 0.

---

### `is_inf(x: double) -> int`

Test whether the value is positive or negative infinity.

**Parameters**
1. `x: double` - Value.

**Returns**
- 1 if `x` is +/-Inf, else 0.

---

### `is_nan(x: double) -> int`

Test whether the value is NaN.

**Parameters**
1. `x: double` - Value.

**Returns**
- 1 if NaN, else 0.

---

### `max(a: double, b: double) -> double`

Return the larger of two values.

**Parameters**
1. `a: double` - First value.
2. `b: double` - Second value.

**Returns**
- The larger value.

---

### `min(a: double, b: double) -> double`

Return the smaller of two values.

**Parameters**
1. `a: double` - First value.
2. `b: double` - Second value.

**Returns**
- The smaller value.

---

### `sin(x: double) -> double`

Compute the sine of an angle in radians.

**Parameters**
1. `x: double` - Angle in radians.

**Returns**
- Sine of `x`.

---

### `sqrt(x: double) -> double`

Compute the square root.

**Parameters**
1. `x: double` - Value.

**Returns**
- Square root of `x`.
