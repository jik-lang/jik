# io

File I/O utilities.

This module provides a minimal wrapper around basic C stdio file operations.
Supported file modes are `"r"`, `"w"`, and `"a"` for text, and `"rb"`, `"wb"`,
and `"ab"` for binary data.
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
- Supported text modes are `"r"` (read), `"w"` (write), and `"a"` (append).
- Binary modes are `"rb"`, `"wb"`, and `"ab"`.

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
- The file must have been opened with `"r"`. Embedded NUL bytes cause an error.

---

### `throws read_bytes(foreign file: File, region: Region) -> bytes::Bytes`

Read all bytes from a binary file, starting at the beginning.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign file: File` - Seekable file handle opened with `"rb"`.
2. `region: Region` - Allocation region for the returned bytes.

**Returns**
- Full file contents, including any NUL bytes.

**Notes**
- Files larger than `INT32_MAX` bytes are rejected.

---

### `throws read_file(foreign path: String, region: Region) -> String`

Read an entire file by path.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `region: Region` - Allocation region for the returned string.

**Returns**
- Full file contents.

---

### `throws read_file_bytes(foreign path: String, region: Region) -> bytes::Bytes`

Read an entire file as bytes by path.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `region: Region` - Allocation region for the returned bytes.

**Returns**
- Full file contents, including any NUL bytes.

---

### `throws write(file: File, foreign text: String) -> void`

Write a string to a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle.
2. `foreign text: String` - String to write.

**Notes**
- The file must have been opened with `"w"` or `"a"`.

---

### `throws write_bytes(file: File, foreign data: bytes::Bytes) -> void`

Write all bytes to a binary file.

**Behavior**
- Throws on failure.

**Parameters**
1. `file: File` - File handle opened with `"wb"` or `"ab"`.
2. `foreign data: bytes::Bytes` - Bytes to write.

---

### `write_stderr(text: String) -> void`

Write a string to standard error.

**Behavior**
- Write errors are ignored.

**Parameters**
1. `text: String` - String to write.

---

### `throws write_file(foreign path: String, foreign text: String) -> void`

Write a string to a file by path, truncating existing contents.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `foreign text: String` - String to write.

---

### `throws write_file_bytes(foreign path: String, foreign data: bytes::Bytes) -> void`

Write bytes to a file by path, truncating existing contents.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.
2. `foreign data: bytes::Bytes` - Bytes to write.

---
