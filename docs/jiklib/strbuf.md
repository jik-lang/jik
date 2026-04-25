# strbuf

Growable string buffer.

`StrBuf` stores a NUL-terminated byte buffer in a region. Growth allocates a
new buffer in the same region and copies existing content.

Notes:
- This is a byte buffer; it does not enforce UTF-8 boundaries.
- All allocations are performed in the provided `Region`.

## Types

### `StrBuf`

Opaque string buffer allocated in a `Region`.

## Functions

### `append(buf: StrBuf, text: String) -> void`

Append a string to the end of the buffer.

**Parameters**
1. `buf: StrBuf` - Buffer.
2. `text: String` - String to append. Foreign parameter.

---

### `append_char(buf: StrBuf, ch: char) -> void`

Append a single byte to the end of the buffer.

**Parameters**
1. `buf: StrBuf` - Buffer.
2. `ch: char` - Byte to append.

---

### `clear(buf: StrBuf) -> void`

Clears the buffer.

**Parameters**
1. `buf: StrBuf` - Buffer.

**Notes**
- Does not deallocate or free anything, just resets the buffer pointer.

---

### `len(buf: StrBuf) -> int`

Get the length of the buffer in bytes.

**Parameters**
1. `buf: StrBuf` - Buffer.

**Returns**
- The number of bytes in the buffer.

---

### `new(text: String, region: Region) -> StrBuf`

Create a new buffer from initial content.

**Parameters**
1. `text: String` - Initial content. Foreign parameter.
2. `region: Region` - Allocation region.

**Returns**
- A new `StrBuf`.

---

### `pop(buf: StrBuf) -> char`

Pop a single byte from the end of the buffer.

**Parameters**
1. `buf: StrBuf` - Buffer.

**Returns**
- A byte from the end of the buffer.

---

### `print(buf: StrBuf) -> void`

Prints the buffer.

**Parameters**
1. `buf: StrBuf` - Buffer.

---

### `to_string(buf: StrBuf, region: Region) -> String`

Converts the buffer contents to a String.

**Parameters**
1. `buf: StrBuf` - Buffer. Foreign parameter.
2. `region: Region` - Region to allocate the String.

**Returns**
- Contents of the buffer as a String.
