# io

File I/O utilities.

This module provides a minimal wrapper around basic C stdio file operations.
Supported file modes are currently `"r"` for reading, `"w"` for writing, and `"a"` for appending.
All operations throw on error.

## Types

### `File`

Opaque file handle.

## Functions

### `throws close(file: File) -> void`

Close an open file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle.

---

### `throws open(foreign path: String, foreign mode: String, region: Region) -> File`

Open a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `foreign mode: String` - File open mode.
3. `region: Region` - Allocation region for file handle state.

**Returns**
- Open file handle.

**Notes**
- Supported modes are `"r"` (read), `"w"` (write), and `"a"` (append).

---

### `throws read(foreign file: File, region: Region) -> String`

Read the full contents of a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign file: File` - File handle.
2. `region: Region` - Allocation region for the returned string.

**Returns**
- Full file contents.

**Notes**
- The file must have been opened in read mode.

---

### `throws read_file(foreign path: String, region: Region) -> String`

Read an entire file by path.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `region: Region` - Allocation region for file handle state and the returned string.

**Returns**
- Full file contents.

---

### `throws write(file: File, foreign text: String) -> void`

Write a string to a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle.
2. `foreign text: String` - String to write.

**Notes**
- The file must have been opened in write or append mode.

---

### `throws write_file(foreign path: String, foreign text: String, region: Region) -> void`

Write a string to a file by path, truncating existing contents.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `foreign text: String` - String to write.
3. `region: Region` - Allocation region for file handle state.

---
