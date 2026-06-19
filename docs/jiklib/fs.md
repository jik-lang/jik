# fs

Filesystem utilities.

This module provides filesystem object and directory operations.

## Types

_None._

## Functions

### `exists(foreign path: String) -> bool`

Return whether a filesystem path exists.

**Parameters**
1. `foreign path: String` - Filesystem path.

**Returns**
- `true` if the path exists, otherwise `false`.

---

### `is_file(foreign path: String) -> bool`

Return whether a filesystem path refers to a regular file.

**Parameters**
1. `foreign path: String` - Filesystem path.

**Returns**
- `true` only for regular files.

---

### `is_dir(foreign path: String) -> bool`

Return whether a filesystem path refers to a directory.

**Parameters**
1. `foreign path: String` - Filesystem path.

**Returns**
- `true` only for directories.

---

### `throws mkdir(foreign path: String) -> void`

Create a directory.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - Directory path.

---

### `throws mkdir_all(foreign path: String) -> void`

Create a directory path and any missing parents.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - Directory path.

---

### `throws remove_file(foreign path: String) -> void`

Remove a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - File path.

---

### `throws remove_dir(foreign path: String) -> void`

Remove an empty directory.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - Directory path.

---

### `throws remove_dir_all(foreign path: String) -> void`

Remove a directory tree recursively.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - Directory path.

**Notes**
- Implemented in Jik by composing `read_dir`, predicates, and removal primitives.
- Symlink-specific behavior is not guaranteed in v1.

---

### `throws rename(foreign old_path: String, foreign new_path: String) -> void`

Rename or move a filesystem object.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign old_path: String` - Source path.
2. `foreign new_path: String` - Destination path.

---

### `throws copy_file(foreign src_path: String, foreign dst_path: String) -> void`

Copy a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign src_path: String` - Source path.
2. `foreign dst_path: String` - Destination path.

---

### `throws read_dir(foreign path: String, region: Region) -> Vec[String]`

Return the names of entries directly inside a directory.

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign path: String` - Directory path.
2. `region: Region` - Allocation region for the returned vector and strings.

**Returns**
- Directory entry names.

**Notes**
- Entry names are returned without directory prefixes.
- Entry ordering is platform-dependent.

---

### `temp_dir(region: Region) -> String`

Return the platform temporary directory path.

**Parameters**
1. `region: Region` - Allocation region for the returned string.

**Returns**
- Temporary directory path.
