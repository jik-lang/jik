# std

Standard utility functions.

## Types

_None._

## Functions

### `throws input(region: Region) -> String`

Read a line of input from standard input.
Strips LF or CRLF and preserves a final line without a newline. Returns an empty
string at EOF. Throws on read errors, embedded NUL bytes, or lines exceeding
Jik's string-length limit.

**Parameters**
1. `region: Region` - Allocation region for the returned string.

**Returns**
- Input line as a String.

---

### `panic(msg: String) -> void`

Terminate the program with an error message.

**Parameters**
1. `msg: String` - Message.
