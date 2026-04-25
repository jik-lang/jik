[Back to overview](../overview.md)

# 12. Vectors

Vectors are dynamic arrays which can hold any type, primitive or composite.
All elements in a vector need to be of the same type. Vector elements can be initialized
from any literal or composite object.

Vector creation and initialization syntax is flexible, as there are multiple ways to do that:

### 12.1 Vectors initialized from type

```jik
// create an empty vector of integers, which is initialized to an empty
// vector which holds type int with default initializer value
v: Vec[int]
```

### 12.2 Vectors initialized with a repeat initializer

```jik
// create a vector of 10 Persons with name "foo"
v := [10 of Person{name="foo"}]
```

The form `[n of value]` creates a vector of length `n`, with each element initialized from `value`.

### 12.3 Vectors initialized from arbitrary initializer

```jik
v := [false, true, false]
```


### 12.4 Basic operations with vectors

Examples.

```jik
numbers: Vec[int]
push(numbers, 1)
push(numbers, 2)
assert(len(numbers) == 2)
numbers[1] = 3
assert(pop(numbers) == 3)
assert(len(numbers) == 1)
```

`push`, `pop` and `len` are builtin functions which work on vectors and dictionaries.


### 12.5 Iterating over vector elements

One can iterate over vector elements with a `for` loop:

```jik
chars := ['a', 'b', 'c']
for ch in chars:
    if ch == 'd':
        print("d found")
    end
end
```
