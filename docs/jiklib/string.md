# string

String utilities.

`String` stores UTF-8 bytes, but the operations in this module are byte-based.
Lengths, indices, and slices refer to byte offsets, not Unicode characters.

## Types

_None._

## Functions

### `concat(left: String, right: String, region: Region) -> String`

Concatenate two strings.

**Parameters**
1. `left: String` - Left string. Foreign parameter.
2. `right: String` - Right string. Foreign parameter.
3. `region: Region` - Allocation region for the result.

**Returns**
- Concatenated string.

---

### `ends_with(s: String, suffix: String) -> bool`

Return whether `s` ends with `suffix`.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `suffix: String` - Suffix to check. Foreign parameter.

**Returns**
- `true` if `s` ends with `suffix`, otherwise `false`.

---

### `find(s: String, needle: String) -> int`

Find the first occurrence of `needle` in `s`.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `needle: String` - Substring to search for. Foreign parameter.

**Returns**
- First match byte index or `-1`.

**Notes**
- Returns the start byte index of the first match, or `-1` if no match exists.

---

### `join(parts: Vec[String], delim: String, r: Region) -> String`

Join strings from `parts` with `delim` inserted between adjacent elements.

**Parameters**
1. `parts: Vec[String]` - Input string parts. Foreign parameter.
2. `delim: String` - Delimiter string. Foreign parameter.
3. `r: Region` - Allocation region for the result.

**Returns**
- A newly allocated joined string.

**Notes**
- If `parts` is empty, the function returns an empty string.

---

### `replace(s: String, needle: String, repl: String, region: Region) -> String`

Replace all occurrences of `needle` in `s` with `repl`.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `needle: String` - Substring to replace. Foreign parameter.
3. `repl: String` - Replacement substring. Foreign parameter.
4. `region: Region` - Allocation region for the result.

**Returns**
- A newly allocated string with all matches replaced.

**Notes**
- If `needle` is empty, the function returns a copy of `s`.

---

### `copy(s: String, region: Region) -> String`

Copy a string into a different allocation region.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `region: Region` - Allocation region for the result.

**Returns**
- A newly allocated string in the destination region.

---

### `slice(s: String, from: int, to: int, region: Region) -> String`

Return a substring of `s` from byte index `start` (inclusive) to byte index `end` (exclusive).

**Behavior**
- Throws on failure.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `from: int` - Start byte index (inclusive).
3. `to: int` - End byte index (exclusive).
4. `region: Region` - Allocation region for the result.

**Returns**
- A newly allocated substring.

---

### `split(s: String, delim: String, r: Region) -> Vec[String]`

Split a string by a delimiter.

**Parameters**
1. `s: String` - Input string. Foreign parameter.
2. `delim: String` - Delimiter string. Foreign parameter.
3. `r: Region` - Allocation region for output tokens and slices.

**Returns**
- A vector of tokens.

---

### `starts_with(s: String, prefix: String) -> bool`

Return whether `s` starts with `prefix`.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `prefix: String` - Prefix to check. Foreign parameter.

**Returns**
- `true` if `s` starts with `prefix`, otherwise `false`.

---

### `to_double(s: String) -> double`

Convert a string to a double.

**Behavior**
- Throws on failure.

**Parameters**
1. `s: String` - Input string.

**Returns**
- Parsed double value.

---

### `to_int(s: String) -> int`

Convert a string to an integer.

**Behavior**
- Throws on failure.

**Parameters**
1. `s: String` - Input string.

**Returns**
- Parsed integer value.

---

### `trim(s: String, region: Region) -> String`

Return a copy of `s` with leading and trailing whitespace removed.

**Parameters**
1. `s: String` - Source string. Foreign parameter.
2. `region: Region` - Allocation region for the result.

**Returns**
- Trimmed string.

**Notes**
- Whitespace is classified using `char::isspace`.
- Whitespace classification is byte-based.
