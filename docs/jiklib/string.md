# string

String utilities.

`String` stores UTF-8 bytes, but the operations in this module are byte-based.
Lengths, indices, and slices refer to byte offsets, not Unicode characters.

## Types

_None._

## Functions

### `concat(foreign left: String, foreign right: String, region: Region) -> String`

Concatenate two strings.

**Parameters**
1. `foreign left: String` - Left string.
2. `foreign right: String` - Right string.
3. `region: Region` - Allocation region for the result.

**Returns**
- Concatenated string.

---

### `ends_with(foreign s: String, foreign suffix: String) -> bool`

Return whether `s` ends with `suffix`.

**Parameters**
1. `foreign s: String` - Source string.
2. `foreign suffix: String` - Suffix to check.

**Returns**
- `true` if `s` ends with `suffix`, otherwise `false`.

---

### `find(foreign s: String, foreign needle: String) -> int`

Find the first occurrence of `needle` in `s`.

**Parameters**
1. `foreign s: String` - Source string.
2. `foreign needle: String` - Substring to search for.

**Returns**
- First match byte index or `-1`.

**Notes**
- Returns the start byte index of the first match, or `-1` if no match exists.

---

### `join(foreign parts: Vec[String], foreign delim: String, r: Region) -> String`

Join strings from `parts` with `delim` inserted between adjacent elements.

**Parameters**
1. `foreign parts: Vec[String]` - Input string parts.
2. `foreign delim: String` - Delimiter string.
3. `r: Region` - Allocation region for the joined string.

**Returns**
- A newly allocated joined string.

**Notes**
- If `parts` is empty, the function returns an empty string.

---

### `replace(foreign s: String, foreign needle: String, foreign repl: String, region: Region) -> String`

Replace all occurrences of `needle` in `s` with `repl`.

**Parameters**
1. `foreign s: String` - Source string.
2. `foreign needle: String` - Substring to replace.
3. `foreign repl: String` - Replacement substring.
4. `region: Region` - Allocation region for the result.

**Returns**
- A newly allocated string with all matches replaced.

**Notes**
- If `needle` is empty, the function returns a copy of `s`.

---

### `copy(foreign s: String, region: Region) -> String`

Copy a string into a different allocation region.

**Parameters**
1. `foreign s: String` - Source string.
2. `region: Region` - Allocation region for the result.

**Returns**
- A newly allocated string in the destination region.

---

### `throws slice(foreign s: String, from: int, to: int, region: Region) -> String`

Return a substring of `s` from byte index `start` (inclusive) to byte index `end` (exclusive).

**Behavior**
- Throws on failure.

**Parameters**
1. `foreign s: String` - Source string.
2. `from: int` - Start byte index (inclusive).
3. `to: int` - End byte index (exclusive).
4. `region: Region` - Allocation region for the result.

**Returns**
- A newly allocated substring.

---

### `split(foreign s: String, foreign delim: String, r: Region) -> Vec[String]`

Split a string by a delimiter.

**Parameters**
1. `foreign s: String` - Input string.
2. `foreign delim: String` - Delimiter string.
3. `r: Region` - Allocation region for the output vector.

**Returns**
- A vector of tokens.

---

### `starts_with(foreign s: String, foreign prefix: String) -> bool`

Return whether `s` starts with `prefix`.

**Parameters**
1. `foreign s: String` - Source string.
2. `foreign prefix: String` - Prefix to check.

**Returns**
- `true` if `s` starts with `prefix`, otherwise `false`.

---

### `throws to_double(s: String) -> double`

Convert a string to a double.

**Behavior**
- Throws on failure.

**Parameters**
1. `s: String` - Input string.

**Returns**
- Parsed double value.

---

### `throws to_int(s: String) -> int`

Convert a string to an integer.

**Behavior**
- Throws on failure.

**Parameters**
1. `s: String` - Input string.

**Returns**
- Parsed integer value.

---

### `trim(foreign s: String, region: Region) -> String`

Return a copy of `s` with leading and trailing whitespace removed.

**Parameters**
1. `foreign s: String` - Source string.
2. `region: Region` - Allocation region for the result.

**Returns**
- Trimmed string.

**Notes**
- Whitespace is classified using `char::isspace`.
- Whitespace classification is byte-based.
