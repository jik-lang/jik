# Jik Grammar Reference

This document is an implementation-derived grammar reference for Jik.
It is based on the current lexer and parser in `src/jik/lexer.c` and
`src/jik/parser.c`.

It is intended to describe the accepted surface syntax of the language.
It is not yet a complete formal language specification, because some forms
are resolved later during semantic analysis.

## Scope

- This grammar describes syntax, not full typing or region semantics.
- Some constructs are context-sensitive. In particular:
  - `Type.Member` may denote enum values or variant tags.
  - `x[Tag]` may denote variant payload access only after semantic resolution.
  - allocation suffixes such as `[r]` and `[.x]` are syntactically regular, but
    semantically restricted.
- Newlines are significant between statements and after block headers.

## Lexical Structure

### Comments

```ebnf
comment ::= "//" { any-char-except-newline }
```

### Identifiers

```ebnf
identifier ::= letter { letter | digit | "_" } | "_"
qualified_identifier ::= identifier [ "::" identifier ]
```

Notes:

- `_` is a reserved standalone token for the local region.
- `Vec`, `Dict`, `Option`, `Some`, and `None` are reserved keywords.
- Identifiers are currently ASCII-only.

### Literals

```ebnf
integer_literal ::= digit { digit }
float_literal   ::= digit { digit } "." digit { digit }
                  | digit { digit } [ "." digit { digit } ] exponent
exponent        ::= ("e" | "E") [ "+" | "-" ] digit { digit }

char_literal    ::= "'" char-char "'"
string_literal  ::= '"' { string-char } '"'
ml_string       ::= '"""' { any-char } '"""'
```

Notes:

- The current lexer requires a float to begin with a digit, so `0.5` is valid
  while `.5` is not.
- Character escapes support `\n`, `\t`, `\r`, `\0`, `\xNN`, `\\`, `\'`, and `\"`.
- Ordinary string literals support `\n`, `\t`, `\r`, `\xNN`, `\\`, `\'`, and `\"`.
- Ordinary and multiline string literals may contain UTF-8 text.
- Source files that contain non-ASCII text should be saved as UTF-8.
- Ordinary string literals do not support `\0`; Jik strings are currently null-terminated.
- String length, indexing, and slicing are byte-based.

### Embedded C

```ebnf
embed_block ::= "@embed" "{" identifier "}" newline
                { any-line }
                identifier newline?
```

The closing delimiter must appear alone on its line, apart from trailing
whitespace.

Embedded C is copied through to the generated C translation. Non-ASCII text in
embedded C therefore remains dependent on the host C compiler's handling of C
source encoding.

## Types

```ebnf
type_desc ::= named_type [ alloc_suffix ]
            | "Vec" "[" type_desc "]" [ alloc_suffix ]
            | "Dict" "[" type_desc "]" [ alloc_suffix ]
            | "Option" "[" type_desc "]" [ alloc_suffix ]

named_type ::= qualified_identifier
```

Named types include built-in names such as:

- `int`
- `double`
- `bool`
- `char`
- `String`
- `Region`
- `Site`

They also include user-defined `struct`, `enum`, and `variant` names.

## Allocation Suffixes

Composite literals and some type descriptions may be followed by an allocation
suffix:

```ebnf
alloc_suffix ::= "[" identifier "]"
               | "[" "." identifier "]"
```

Examples:

```jik
Point{}[r]
Point{}[.p]
Some{"x"}[r]
None[r]
```

The parser also accepts the local region value `_` as an ordinary expression,
but allocation suffix syntax itself expects either `id` or `.id`.

Allocation suffix syntax takes precedence immediately after composite literals.
As a result, forms like `[1, 2][0]` are not parsed as direct subscripting of a
literal. In the current language, composite temporaries must first be bound to
a name before subscripting, member access, mutation, or iteration.

## Program Structure

```ebnf
program ::= { newline | use_decl | global_decl | top_decl | variant_decl
            | enum_decl | embed_block }
```

### Imports

```ebnf
use_decl ::= "use" string_literal [ "as" identifier ]
```

### Globals

```ebnf
global_decl ::= identifier ":=" expr
```

Top-level typed globals such as `x: int` are not parsed directly. The current
parser accepts only top-level `:=` declarations.

### Top-Level Declarations

```ebnf
top_decl ::= decl_prefix ( func_decl | struct_decl )
decl_prefix ::= { "extern" | "throws" }
```

Current parser restrictions:

- `extern` and `throws` may prefix `func`.
- `extern` may prefix `struct`.
- `throws struct ...` is rejected.

### Functions

```ebnf
func_decl ::= "func" identifier "(" [ params ] ")" [ "->" type_desc ] ":"
              newline block "end"
```

Extern functions use a different form:

```ebnf
extern_func_decl ::= "extern" [ "throws" ] "func"
                     identifier "as" identifier
                     "(" [ extern_params ] ")"
                     "->" type_desc
```

### Parameters

```ebnf
params ::= param { "," param }
param  ::= [ "foreign" ] identifier [ ":" type_desc ]

extern_params ::= extern_param { "," extern_param }
extern_param  ::= [ "foreign" ] identifier ":" type_desc
```

Notes:

- Ordinary function parameters may omit type annotations.
- Extern function parameters must have type annotations.

### Structs

```ebnf
struct_decl ::= "struct" identifier ":" newline
                { identifier ":" type_desc newline }
                "end"

extern_struct_decl ::= "extern" "struct" identifier "as" identifier
```

### Variants

```ebnf
variant_decl ::= "variant" identifier ":" newline
                 { identifier ":" type_desc newline }
                 "end"
```

### Enums

```ebnf
enum_decl ::= "enum" identifier ":" newline
              { identifier newline }
              "end"
```

## Blocks and Statements

```ebnf
block ::= { statement newline }
```

The current parser expects at least one newline after each statement in a block.

### Statements

```ebnf
statement ::= return_stmt
            | if_stmt
            | match_stmt
            | while_stmt
            | for_stmt
            | break_stmt
            | continue_stmt
            | try_stmt
            | decl_stmt
            | assign_stmt
            | call_stmt
            | must_call_stmt
```

```ebnf
return_stmt    ::= "return" [ expr ]
break_stmt     ::= "break"
continue_stmt  ::= "continue"

decl_stmt      ::= identifier ":=" expr
                 | identifier ":" type_desc

assign_stmt    ::= lvalue assign_op expr
assign_op      ::= "=" | "+=" | "-=" | "*=" | "/="

call_stmt      ::= call_expr
must_call_stmt ::= "must" call_expr
```

Valid lvalues in the parser are:

```ebnf
lvalue ::= identifier
         | primary "." identifier
         | primary "[" expr "]"
```

### If

```ebnf
if_stmt ::= "if" expr ":" newline block
            { "elif" expr ":" newline block }
            [ "else" ":" newline block ]
            "end"
```

### While

```ebnf
while_stmt ::= "while" expr ":" newline block "end"
```

### For

```ebnf
for_stmt ::= "for" identifier "=" expr "," expr ":" newline block "end"
           | "for" identifier "in" expr ":" newline block "end"
           | "for" identifier "," identifier "in" expr ":" newline block "end"
```

### Try / Except

```ebnf
try_stmt ::= "try" ( call_expr | identifier ":=" call_expr )
             ":" newline block
             "except" ":" newline block
             "end"
```

The parser currently permits only:

- a direct call, or
- a declaration initialized by a direct call

after `try`.

### Match

```ebnf
match_stmt ::= "match" expr ":" newline
               { case_clause }
               "end"

case_clause ::= "case" variant_pattern ":" newline block
variant_pattern ::= qualified_identifier "." identifier "{" identifier "}"
```

Current implementation note:

- `case Value.TAG{x}:` is the supported pattern form.
- The parser expects a bound identifier payload in the case pattern.

## Expressions

The parser uses the following precedence, from lowest to highest.

### Ternary

```ebnf
expr ::= ternary

ternary ::= logical_or [ "if" expr "else" ternary ]
```

This yields expressions such as:

```jik
2 if x == 3 else 12
```

### Logical Operators

```ebnf
logical_or  ::= logical_and { "or" logical_and }
logical_and ::= is_expr { "and" is_expr }
```

### `is`

```ebnf
is_expr ::= comparison [ "is" is_target ]
is_target ::= "Some"
            | "None"
            | qualified_identifier "." identifier
```

Examples:

```jik
x is Some
x is None
v is Value.INT
```

### Comparison and Arithmetic

```ebnf
comparison ::= term { ("==" | "!=" | "<" | ">" | "<=" | ">=") term }
term       ::= factor { ("+" | "-") factor }
factor     ::= unary { ("*" | "/" | "%") unary }
unary      ::= ( "not" | "-" | "must" ) unary | primary
```

### Primary Expressions

```ebnf
primary ::= atom { postfix }

postfix ::= "." identifier
          | "[" expr "]"
          | "?"
```

Examples:

```jik
point.x
values[i]
opt?
v[Value.INT]
```

### Atoms

```ebnf
atom ::= integer_literal
       | float_literal
       | char_literal
       | "true"
       | "false"
       | string_literal [ alloc_suffix ]
       | ml_string [ alloc_suffix ]
       | "Some" "{" expr "}" [ alloc_suffix ]
       | "None" [ alloc_suffix ]
       | qualified_identifier
       | call_expr
       | struct_literal [ alloc_suffix ]
       | variant_literal [ alloc_suffix ]
       | "(" expr ")"
       | vector_literal [ alloc_suffix ]
       | dict_literal [ alloc_suffix ]
       | "." identifier
       | "_"
```

### Calls

```ebnf
call_expr ::= qualified_identifier "(" [ args ] ")"
args      ::= expr { "," expr }
```

Special implementation rule:

- `unwrap(x)` is parsed specially and treated as equivalent to `x?`.

### Struct Literals

```ebnf
struct_literal ::= qualified_identifier "{"
                   [ struct_field { "," struct_field } ]
                   "}"

struct_field ::= identifier "=" expr
```

### Variant Literals

```ebnf
variant_literal ::= qualified_identifier "." identifier "{"
                    [ expr ]
                    "}"
```

Examples:

```jik
Value.INT{3}
Value.TEXT{"hi"}[r]
Value.INT{}
```

### Vector Literals

```ebnf
vector_literal ::= "[" expr "of" expr "]"
                 | "[" expr { "," expr } "]"
```

Examples:

```jik
[10 of 0]
[1, 2, 3]
```

### Dictionary Literals

```ebnf
dict_literal ::= "{"
                 [ dict_entry { "," dict_entry } [ "," ] ]
                 "}"

dict_entry ::= expr ":" expr
```

Keys are checked semantically and must be of type `String`.

## Semantic Resolution Notes

These forms are parsed first and refined later:

- `EnumName.VALUE` is initially parsed as member access, then rewritten as an
  enum value during semantic analysis.
- `VariantName.TAG` is initially parsed as member access, then rewritten as a
  variant tag marker during semantic analysis.
- `value[VariantName.TAG]` is syntactically a subscript expression and becomes
  variant payload access only after semantic resolution.

## Current Implementation Gaps and Constraints

- Top-level typed globals are not currently parsed.
- `match` supports the current `case Variant.Tag{name}:` form; richer patterns
  are not part of the current parser.
