# path

Path utilities.

## Types

_None._

## Functions

### `join(parts: Vec[String], region: Region) -> String`

Join path segments using the platform separator.

**Parameters**
1. `parts: Vec[String]` - Path segments to join. Foreign parameter.
2. `region: Region` - Allocation region for the result.

**Returns**
- Combined path string.

**Notes**
- Empty segments are ignored.

---

### `basename(path: String, region: Region) -> String`

Return the last path component.

**Parameters**
1. `path: String` - Input path. Foreign parameter.
2. `region: Region` - Allocation region for the result.

**Returns**
- Last path component, or an empty string for root-like inputs.

---

### `dirname(path: String, region: Region) -> String`

Return the parent directory.

**Parameters**
1. `path: String` - Input path. Foreign parameter.
2. `region: Region` - Allocation region for the result.

**Returns**
- Parent directory path.

**Notes**
- Root-like inputs are kept stable.
- A path with no directory component returns `"."`.

---

### `extname(path: String, region: Region) -> String`

Return the extension of the last path component.

**Parameters**
1. `path: String` - Input path. Foreign parameter.
2. `region: Region` - Allocation region for the result.

**Returns**
- Extension text including the leading dot, or `""` when no extension exists.

---

### `is_absolute(path: String) -> bool`

Return whether `path` is absolute.

**Parameters**
1. `path: String` - Input path. Foreign parameter.

**Returns**
- `true` if the path is absolute, otherwise `false`.

**Notes**
- Windows paths accept both `\` and `/` separators.
- Drive-root forms such as `C:\foo` are treated as absolute.
