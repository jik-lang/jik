[Back to overview](../overview.md)

# 13. Dictionaries

Dictionaries map **string keys** to values.

### 13.1 Creating dictionaries

Like vectors, we can create empty dictionaries from type descriptions:

```jik
counts: Dict[int]
buckets: Dict[Vec[String]]
```

Also, we can directly specify the initial key-value pairs:

```jik
config := {
    "host": "localhost",
    "port": 8080,
}

commands := {
    "copy": {"overwrite": true, "recursive": false},
    "move": {"overwrite": true, "recursive": true},
}
```

### 13.2 Dictionary lookup

Access returns an `Option[T]`, where `T` is the dictionary value type.

Example:

```jik
counts: Dict[int]

counts["apple"] = 3
counts["banana"] = 5

item := counts["apple"]
if item is Some:
    print(item?)
end

missing := counts["orange"]
if missing is None:
    print("no oranges found")
end
```

Forced lookup is concise with `?`:

```jik
print(counts["apple"]?)
```

This throws a runtime error if the key is missing.

### 13.3 Dictionary iteration

This is achieved using a `for` loop:

```jik
for k, v in counts:
    print("key = ", k, ", value = ", v)
end
```

---
