[Back to overview](../overview.md)

# 5. Values and Types

Jik has a small core of built-in types, upon which richer data types can be built. Types in Jik are divided into
two categories - Primitives and Composites. Composite types are always dynamically allocated.

### 5.1 Primitive types

- **Integers**: `0`, `1`, `42`, `-7`
    - type name: `int`
    - translates to C type: `int32_t`
- **Doubles**: `3.22`, `0.5`, `3e-5`
    - type name: `double`
    - translates to C type: `double`
- **Booleans**: `true`, `false`
    - type name: `bool`
    - translates to C type: `bool`
- **Characters**: `'a'`, `'+'`, `'['`
    - type name: `char`
    - translates to C type: `unsigned char`
    - `char` is a single-byte value; it is not a Unicode code-point type


### 5.2 Composite types

Composite types are specific because they are allocated in dynamic memory. Jik handles
memory management with regions, where these types are allocated. More on that topic in the next section.

- **Strings**: `"fubar"`
    - type name: `String`
    - translates to C type defined in the Jik support library
    - strings store UTF-8 bytes
    - ordinary string literals support `\n`, `\t`, `\r`, `\\`, `\'`, and `\"`
    - ordinary and multiline string literals may contain UTF-8 text
    - source files that contain non-ASCII string text should be saved as UTF-8
    - string length, indexing, and slicing are byte-based
    - strings are currently NUL-terminated, so ordinary string literals do not support embedded `\0`
- **Options**: `Option[T]`, `Some{3}`, `None`
    - type name: `Option[T]`
    - represents either a present value (`Some`) or no value (`None`)
- **Vectors**: `[1, 2, 3]`, `[10 of 1]`, `Vec[Person]`
    - type name: `Vec[T]`
    - translates to C type defined in the Jik support library
- **Dictionaries**: `{"foo": 0.54, "bar": 2.33}`, `Dict[double]`
    - type name: `Dict[T]`
    - string-keyed map from `String` keys to values of type `T`
    - translates to C type defined in the Jik support library
- **Structs**
    - type name: `<struct_name>`
    - translates to C type defined in the Jik support library
- **Variants**
    - type name: `<variant_name>`
    - translates to C type defined in the Jik support library

In general, capitalized type names imply a composite type, e.g. one which is allocated in a region.

When referring to a user-defined type from another module, qualify it with the module alias, for example
`test::TestSuite`, `shapes::Person`, or `net::Message`.


---
