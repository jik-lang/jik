# sys

System utilities.

## Types

_None._

## Functions

### `exit(code: int) -> void`

Terminate the process with a status code.

**Parameters**
1. `code: int` - Exit status.

---

### `cwd(region: Region) -> String`

Return the current working directory.

**Parameters**
1. `region: Region` - Allocation region for the returned string.

**Returns**
- Current working directory as string, or empty string on failure.

---

### `throws executable_path(region: Region) -> String`

Return the absolute path of the running executable.

**Parameters**
1. `region: Region` - Allocation region for the returned string.

**Returns**
- Absolute path of the running executable.

**Behavior**
- Throws if the operating system cannot determine the executable path.
- Use `path::dirname` when locating files beside the executable. This is not
  the current working directory.

---

### `getenv(name: String, region: Region) -> Option[String]`

Get an environment variable value.

**Parameters**
1. `name: String` - Environment variable name.
2. `region: Region`

**Returns**
- Value as `Some[String]`, or `None` if missing.

---

### `platform(region: Region) -> String`

Return a short platform identifier string.

**Parameters**
1. `region: Region` - Allocation region for the returned string.

**Returns**
- Platform name as string

---

### `sleep(ms: int) -> void`

Sleep for a number of milliseconds (approximate).

**Parameters**
1. `ms: int` - Milliseconds.

---

### `system(cmd: String) -> int`

Execute a command using the host system shell.

**Parameters**
1. `cmd: String` - Command string.

**Returns**
- Command exit code (platform-dependent).
