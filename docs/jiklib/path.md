# path

Path utilities.

## Types

_None._

## Functions

### `join(foreign parts: Vec[String], r: Region) -> String`

Join path segments using the platform separator.

**Parameters**
1. `foreign parts: Vec[String]` - Path segments to join.
2. `r: Region` - Allocation region for the result.

**Returns**
- Combined path string.

**Notes**
- Empty segments are ignored.

---

### `normalize(foreign path: String, r: Region) -> String`

Normalize a path lexically.

**Parameters**
1. `path: String` - Input path.
2. `r: Region` - Allocation region for the result.

**Returns**
- Lexically normalized path.

**Notes**
- Repeated separators and `.` components are removed.
- `..` components remove the preceding normal component when possible. Unresolved `..` components are preserved in relative paths and discarded at an absolute root.
- This function does not access the filesystem, resolve symlinks, expand home or environment references, or support Windows UNC and device paths.

---

### `basename(foreign path: String, r: Region) -> String`

Return the last path component.

**Parameters**
1. `foreign path: String` - Input path.
2. `r: Region` - Allocation region for the result.

**Returns**
- Last path component, or an empty string for root-like inputs.

---

### `dirname(foreign path: String, r: Region) -> String`

Return the parent directory.

**Parameters**
1. `foreign path: String` - Input path.
2. `r: Region` - Allocation region for the result.

**Returns**
- Parent directory path.

**Notes**
- Root-like inputs are kept stable.
- A path with no directory component returns `"."`.

---

### `extname(foreign path: String, r: Region) -> String`

Return the extension of the last path component.

**Parameters**
1. `foreign path: String` - Input path.
2. `r: Region` - Allocation region for the result.

**Returns**
- Extension text including the leading dot, or `""` when no extension exists.

---

### `is_absolute(path: String) -> bool`

Return whether `path` is absolute.

**Parameters**
1. `path: String` - Input path.

**Returns**
- `true` if the path is absolute, otherwise `false`.

**Notes**
- Windows paths accept both `\` and `/` separators.
- Drive-root forms such as `C:\foo` are treated as absolute.
