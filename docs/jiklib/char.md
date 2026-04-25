# char

Wrapper library for functions operating on single-byte character values.

These functions are byte-oriented wrappers around the host C library. They are
not Unicode-aware character classification or case-mapping operations.

## Types

_None._

## Functions

### `isalnum(ch: char) -> bool`

Wrapper around isalnum.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is an alphabet letter or digit, false otherwise.

---

### `isalpha(ch: char) -> bool`

Wrapper around isalpha.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is an alphabet letter, false otherwise.

---

### `isdigit(ch: char) -> bool`

Wrapper around isdigit.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is a digit, false otherwise.

---

### `islower(ch: char) -> bool`

Wrapper around islower.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is a lowercase alphabet letter, false otherwise.

---

### `isspace(ch: char) -> bool`

Wrapper around isspace.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is a whitespace character, false otherwise.

---

### `isupper(ch: char) -> bool`

Wrapper around isupper.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is an uppercase alphabet letter, false otherwise.

---

### `isxdigit(ch: char) -> bool`

Wrapper around isxdigit.

**Parameters**
1. `ch: char` - Character.

**Returns**
- true if character is a hexadecimal digit, false otherwise.

---

### `tolower(ch: char) -> char`

Wrapper around tolower.

**Parameters**
1. `ch: char` - Character.

**Returns**
- Lowercase variant if possible, otherwise returns input.

---

### `toupper(ch: char) -> char`

Wrapper around toupper.

**Parameters**
1. `ch: char` - Character.

**Returns**
- Uppercase variant if possible, otherwise returns input.
