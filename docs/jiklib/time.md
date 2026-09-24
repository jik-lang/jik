# time

Time utilities for measuring durations and recording basic log timestamps.

## Types

_None._

## Functions

### `monotonic_ms() -> int`

Return milliseconds elapsed since the first call in the current process.

Use this function to measure durations. The value is process-relative and is
not affected by wall-clock changes. It saturates at the largest `int` value.

---

### `unix_seconds() -> int`

Return the current Unix timestamp in seconds.

Use this function for basic machine-readable log timestamps. It returns `0` if
the current value cannot be represented by `int`.

---

### `sleep(ms: int) -> void`

Sleep for approximately `ms` milliseconds. Non-positive durations return
immediately.
