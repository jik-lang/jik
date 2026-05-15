# bytes

Binary-safe byte sequences and growable byte buffers.

`Bytes` stores immutable raw byte data. `ByteBuf` is a mutable growable byte
buffer for constructing binary data. Neither type is NUL-terminated by
contract, and both can contain byte value `0`.

Notes:
- `String` is text. Use `from_string` or `buf_append_string` to copy text bytes
  into binary storage.
- Use `to_hex` for binary display.
- Use `to_string_ascii` only when the bytes are known ASCII text.
- All allocations are performed in the provided `Region`.

## Types

### `ByteBuf`

Mutable growable byte buffer allocated in a `Region`.

### `Bytes`

Immutable byte sequence allocated in a `Region`.

## Functions

### `buf_append(buf: ByteBuf, bytes: Bytes) -> void`

Append all bytes from an immutable byte sequence.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.
2. `bytes: Bytes` - Bytes to append.

---

### `buf_append_string(buf: ByteBuf, text: String) -> void`

Append a string's exact bytes.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.
2. `text: String` - String to append. Foreign parameter.

---

### `buf_clear(buf: ByteBuf) -> void`

Clear a byte buffer without reclaiming region memory.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.

---

### `buf_from_string(text: String, region: Region) -> ByteBuf`

Create a byte buffer initialized from a string's exact bytes.

**Parameters**
1. `text: String` - Source string. Foreign parameter.
2. `region: Region` - Allocation region.

**Returns**
- New `ByteBuf`.

---

### `buf_get(buf: ByteBuf, index: int) -> char`

Get one byte from a byte buffer by index.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.
2. `index: int` - Byte index.

**Returns**
- Byte at `index`.

---

### `buf_len(buf: ByteBuf) -> int`

Get the length of a byte buffer.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.

**Returns**
- Number of bytes currently stored.

---

### `buf_new(region: Region) -> ByteBuf`

Create an empty mutable byte buffer.

**Parameters**
1. `region: Region` - Allocation region.

**Returns**
- Empty `ByteBuf`.

---

### `buf_push(buf: ByteBuf, byte: char) -> void`

Append one byte.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.
2. `byte: char` - Byte to append.

---

### `buf_push_int(buf: ByteBuf, byte: int) -> void`

Append one integer value after validating it is a byte.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.
2. `byte: int` - Integer byte value.

**Throws**
- If `byte` is outside `0..255`.

---

### `equals(left: Bytes, right: Bytes) -> bool`

Return whether two byte sequences have identical contents.

**Parameters**
1. `left: Bytes` - Left byte sequence.
2. `right: Bytes` - Right byte sequence.

**Returns**
- `true` if lengths and contents are equal.

---

### `from_string(text: String, region: Region) -> Bytes`

Copy a string's exact bytes into a byte sequence.

**Parameters**
1. `text: String` - Source string. Foreign parameter.
2. `region: Region` - Allocation region.

**Returns**
- Copied `Bytes`.

---

### `get(bytes: Bytes, index: int) -> char`

Get one byte by index.

**Parameters**
1. `bytes: Bytes` - Byte sequence.
2. `index: int` - Byte index.

**Returns**
- Byte at `index`.

---

### `len(bytes: Bytes) -> int`

Get the length of a byte sequence.

**Parameters**
1. `bytes: Bytes` - Byte sequence.

**Returns**
- Number of bytes.

---

### `new(region: Region) -> Bytes`

Create an empty byte sequence.

**Parameters**
1. `region: Region` - Allocation region.

**Returns**
- Empty `Bytes`.

---

### `slice(bytes: Bytes, start: int, stop: int, region: Region) -> Bytes`

Copy a byte range.

**Parameters**
1. `bytes: Bytes` - Byte sequence.
2. `start: int` - Start byte index, inclusive.
3. `stop: int` - Stop byte index, exclusive.
4. `region: Region` - Allocation region.

**Returns**
- Copied byte range.

---

### `to_bytes(buf: ByteBuf, region: Region) -> Bytes`

Copy a byte buffer into an immutable byte sequence.

**Parameters**
1. `buf: ByteBuf` - Byte buffer.
2. `region: Region` - Allocation region.

**Returns**
- Copied `Bytes`.

---

### `to_hex(bytes: Bytes, region: Region) -> String`

Convert bytes to lowercase hexadecimal text.

**Parameters**
1. `bytes: Bytes` - Byte sequence.
2. `region: Region` - Allocation region.

**Returns**
- Hexadecimal representation.

---

### `to_string_ascii(bytes: Bytes, region: Region) -> String`

Convert known ASCII text bytes to a `String`.

**Parameters**
1. `bytes: Bytes` - Byte sequence.
2. `region: Region` - Allocation region.

**Returns**
- Copied ASCII text.

**Throws**
- If any byte is `0` or greater than `127`.
