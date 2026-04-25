# region

Region allocator diagnostics.

This module exposes small read-only helpers for inspecting region allocator
state at runtime.

## Types

_None._

## Functions

### `used_bytes(region: Region) -> int`

Return the number of allocator-consumed bytes in the region.

**Parameters**
1. `region: Region` - Region to inspect.

**Returns**
- Total bytes currently marked as used across all region blocks.

**Notes**
- This is allocator-level usage, not exact live object size.
- It includes allocator padding and dead bytes left behind by region growth.

---

### `total_bytes(region: Region) -> int`

Return the total capacity of all blocks currently owned by the region.

**Parameters**
1. `region: Region` - Region to inspect.

**Returns**
- Total block capacity in bytes.

---

### `block_count(region: Region) -> int`

Return the number of allocation blocks currently owned by the region.

**Parameters**
1. `region: Region` - Region to inspect.

**Returns**
- Number of blocks in the region.

---

### `print_stats(region: Region) -> void`

Print a short region allocator summary to standard output.

**Parameters**
1. `region: Region` - Region to inspect.

**Notes**
- The printed summary includes the region address together with used bytes,
  total bytes, and block count.

---
