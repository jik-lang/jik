# io

File I/O utilities.

This module provides a minimal wrapper around basic C stdio file operations.
Supported file modes are currently `"r"` for reading, `"w"` for writing, and `"a"` for appending.
All operations throw on error.

## Types

### `File`

Opaque file handle.

## Functions

### `close(file: File) -> void`

Close an open file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle.

---

### `open(path: String, mode: String, region: Region) -> File`

Open a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - File path. Foreign parameter.
2. `mode: String` - File open mode. Foreign parameter.
3. `region: Region` - Allocation region for file handle state.

**Returns**
- Open file handle.

**Notes**
- Supported modes are `"r"` (read), `"w"` (write), and `"a"` (append).

---

### `exists(path: String) -> bool`

Return whether a filesystem path exists.

**Parameters**
1. `path: String` - File path. Foreign parameter.

**Returns**
- `true` if the path exists, otherwise `false`.

---

### `read(file: File, region: Region) -> String`

Read the full contents of a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle. Foreign parameter.
2. `region: Region` - Allocation region for the returned string.

**Returns**
- Full file contents.

**Notes**
- The file must have been opened in read mode.

---

### `read_file(path: String, region: Region) -> String`

Read an entire file by path.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - File path. Foreign parameter.
2. `region: Region` - Allocation region for file handle state and the returned string.

**Returns**
- Full file contents.

---

### `remove(path: String) -> void`

Remove a file from the filesystem.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - File path.

---

### `write(file: File, text: String) -> void`

Write a string to a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle.
2. `text: String` - String to write. Foreign parameter.

**Notes**
- The file must have been opened in write or append mode.

---

### `write_file(path: String, text: String, region: Region) -> void`

Write a string to a file by path, truncating existing contents.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - File path. Foreign parameter.
2. `text: String` - String to write. Foreign parameter.
3. `region: Region` - Allocation region for file handle state.

---
