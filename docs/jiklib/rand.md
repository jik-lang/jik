# rand

Pseudo-random number generation.

## Types

### `Rng`

Opaque random number generator state.

## Functions

### `new(seed: int, region: Region) -> Rng`

Create a new RNG with a deterministic seed.

**Parameters**
1. `seed: int` - Seed value.
2. `region: Region` - Allocation region for RNG state.

**Returns**
- A new RNG.

---

### `new_time(region: Region) -> Rng`

Create a new RNG seeded from the current time.

**Parameters**
1. `region: Region` - Allocation region for RNG state.

**Returns**
- A new RNG.

---

### `next_double(rng: Rng) -> double`

Generate a random floating-point number.

**Parameters**
1. `rng: Rng` - RNG.

**Returns**
- Uniform double in [0.0, 1.0).

---

### `next_int(rng: Rng) -> int`

Generate a non-negative random integer.

**Parameters**
1. `rng: Rng` - RNG.

**Returns**
- Uniform int in [0, 2^31 - 1].

---

### `range_int(rng: Rng, lo: int, hi: int) -> int`

Generate a random integer in a half-open interval.

**Behavior**
- Throws on failure.

**Parameters**
1. `rng: Rng` - RNG.
2. `lo: int` - Lower bound (inclusive).
3. `hi: int` - Upper bound (exclusive).

**Returns**
- Uniform int in [lo, hi).
