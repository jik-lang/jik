[Back to overview](../overview.md)

# 14. Tables

Tables are fixed, module-level associations from every member of an enum, or every tag of a
variant, to a value of one declared type. Their entries are initialized once and cannot be
replaced.

```jik
enum Opcode:
    PUSH
    POP
    PRINT
end

table StackDelta[Opcode] -> int:
    PUSH: 1
    POP: -1
    PRINT: 0
end

delta := StackDelta[Opcode.PUSH]
```

Each member must appear exactly once. Unknown members, duplicate entries, and missing entries are
compile errors. There is no default entry, so adding an enum member also requires updating every
table keyed by that enum. Source entry order is irrelevant.

A variant-tag table uses the active tag and ignores its payload:

```jik
variant Token:
    IDENT: String
    NUMBER: int
    END
end

table TokenNames[Token] -> String:
    IDENT: "identifier"
    NUMBER: "number"
    END: "end"
end

token := Token.IDENT{"name"}
name := TokenNames[token]
```

Entry expressions are evaluated once before global initialization. Composite entries therefore
have global lifetime and are shared: looking up the same entry does not construct or copy it. Use
`copy(TableName[key], region)` when an independent composite is required. As with globals, this is
[binding immutability rather than deep object immutability](06-variables-and-assignment.md#63-binding-immutability-and-object-mutability).

Table entries cannot call functions, including built-ins, or read global variables. They may look
up an earlier table.

Tables are initialized before globals, so global initializers may look up a table. A table entry
cannot look up itself or a table initialized later.

Imported tables use normal module qualification:

```jik
label := protocol::OpcodeNames[opcode]
```

Tables support lookup only. Their entries cannot be reassigned, and tables cannot be passed as
values, iterated, queried with `len`, or used as types. A lookup of a composite entry still
returns the shared composite value, so its contents may be mutated; use `copy` when an independent
value is required. Unlike `Dict`, a table has a fixed enum or variant key set, is checked for
completeness at compile time, and never returns `Option[T]`.
