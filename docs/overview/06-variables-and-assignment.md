[Back to overview](../overview.md)

# 6. Variables and Assignment

### 6.1 Declaring variables with `:=`

Each variable needs to be declared and initialized with `:=`:

```jik
x := 3
name := "Alice"
count := false
```

Variable values can later be modified using the `=` operator.

This syntax is useful when we want to explicitly initialize a variable upon creation.
However, inside functions, we can also use a type annotation to declare a variable. In that case, the variable
is given a default initial value.

```jik
a: int
s1: String
nums: Vec[int]
people: Dict[Vec[Person]]
```

Defaults per type are:

- **Integers**: `0`
- **Doubles**: `0.0`
- **Booleans**: `false`
- **Characters**: `0`
- **Strings** `""`
- **Structs**: struct with each field default initialized
- **Vectors**: empty vector of given type
- **Dictionaries**: empty dict of given type
- **Variants**: variant with active payload default initialized


At top level, globals are declared with `:=`.
Typed declarations such as `x: int` are for local variables inside functions.

### 6.2 Local scope and redeclaration rules

Local variables live in block scopes, but Jik does not currently allow C-style shadowing.

In practice:

- a local name may not be redeclared in an inner block if it already exists in an outer local scope of the same function
- parameters and loop variables follow the same rule
- a local variable also may not shadow a global declared in the same module

So code such as the following is a compile error:

```jik
value := 1

func main(flag: bool):
    if flag:
        value := 2   // not allowed
    end
end
```
