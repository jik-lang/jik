# process

Shell-free process execution.

This module provides a small argv-based process API. It does not invoke a
shell, so arguments are passed as structured values instead of one quoted
command string.

## Types

### `Result`

Completed captured child process result.

Fields:

1. `code: int` - Child exit code.
2. `out: bytes::Bytes` - Captured stdout bytes.
3. `err: bytes::Bytes` - Captured stderr bytes.

## Functions

### `run(program: String, args: Vec[String]) -> int`

Run a child process with inherited stdio and wait for it.

**Behavior**
- Throws if the child cannot be started or if waiting for it fails.
- A nonzero child exit code is returned, not thrown.
- No shell is invoked.

**Parameters**
1. `program: String` - Executable name or path. Foreign parameter.
2. `args: Vec[String]` - Child process arguments. Foreign parameter.

**Returns**
- Child exit code.

**Notes**
- `args` contains only child arguments, not `argv[0]`.

---

### `capture(program: String, args: Vec[String], region: Region) -> Result`

Run a child process, capture stdout and stderr, and wait for it.

**Behavior**
- Throws if pipes cannot be created, the child cannot be started, waiting
  fails, or pipe reads fail.
- A nonzero child exit code is returned in `Result.code`, not thrown.
- No shell is invoked.
- Stdout and stderr are captured separately as raw bytes.
- Stdin is inherited from the parent process.

**Parameters**
1. `program: String` - Executable name or path. Foreign parameter.
2. `args: Vec[String]` - Child process arguments. Foreign parameter.
3. `region: Region` - Allocation region for the returned result and captured bytes.

**Returns**
- `Result` containing the child exit code, stdout bytes, and stderr bytes.

**Notes**
- `args` contains only child arguments, not `argv[0]`.
- Captured output may not be valid text. Use `bytes` helpers when decoding or
  inspecting it.
