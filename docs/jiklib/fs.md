# fs

Filesystem utilities.

This module provides filesystem object and directory operations.

## Types

_None._

## Functions

### `exists(path: String) -> bool`

Return whether a filesystem path exists.

**Parameters**
1. `path: String` - Filesystem path. Foreign parameter.

**Returns**
- `true` if the path exists, otherwise `false`.

---

### `is_file(path: String) -> bool`

Return whether a filesystem path refers to a regular file.

**Parameters**
1. `path: String` - Filesystem path. Foreign parameter.

**Returns**
- `true` only for regular files.

---

### `is_dir(path: String) -> bool`

Return whether a filesystem path refers to a directory.

**Parameters**
1. `path: String` - Filesystem path. Foreign parameter.

**Returns**
- `true` only for directories.

---

### `mkdir(path: String) -> void`

Create a directory.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - Directory path. Foreign parameter.

---

### `mkdir_all(path: String) -> void`

Create a directory path and any missing parents.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - Directory path. Foreign parameter.

---

### `remove_file(path: String) -> void`

Remove a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - File path. Foreign parameter.

---

### `remove_dir(path: String) -> void`

Remove an empty directory.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - Directory path. Foreign parameter.

---

### `remove_dir_all(path: String) -> void`

Remove a directory tree recursively.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - Directory path. Foreign parameter.

**Notes**
- Implemented in Jik by composing `read_dir`, predicates, and removal primitives.
- Symlink-specific behavior is not guaranteed in v1.

---

### `rename(old_path: String, new_path: String) -> void`

Rename or move a filesystem object.

**Behavior**
- Throws on failure.

**Parameters**
1. `old_path: String` - Source path. Foreign parameter.
2. `new_path: String` - Destination path. Foreign parameter.

---

### `copy_file(src_path: String, dst_path: String) -> void`

Copy a file.

**Behavior**
- Throws on failure.

**Parameters**
1. `src_path: String` - Source path. Foreign parameter.
2. `dst_path: String` - Destination path. Foreign parameter.

---

### `read_dir(path: String, region: Region) -> Vec[String]`

Return the names of entries directly inside a directory.

**Behavior**
- Throws on failure.

**Parameters**
1. `path: String` - Directory path. Foreign parameter.
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
