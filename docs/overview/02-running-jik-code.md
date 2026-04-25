[Back to overview](../overview.md)

# 2. Running Jik Code

Jik comes with a command-line tool called `jik`. Say we have the following Jik program:

```jik
// hello.jik

func main():
    print("Hello, Jik!")
end
```

To run the program, we do:

```
jik run hello.jik
```

This command:

1. Translates all reachable Jik source files into a **single C file**.
2. Compiles that C file with the C compiler given by the `JIK_CC` environment variable.
3. Produces an executable and runs it.

In other words, `jik run` is: **translate -> compile -> run**, in one step.

Two other important commands are:

- `jik tran hello.jik` to translate the program to C without compiling it
- `jik build hello.jik` to translate and compile the program without running it

For performance-oriented builds, `jik tran`, `jik build`, and `jik run` also support
`--unsafe-no-bounds-checks`, which disables runtime vector bounds checks for generated
`v[i]` reads and writes.

So the three most common commands are:

- `run`: translate, compile, and run
- `build`: translate and compile
- `tran`: translate only

By default, Jik uses the C compiler specified by the `JIK_CC` environment variable. You can also
choose a compiler explicitly with the `--cc` option when invoking `jik`.

To get a list of commands and help for each one:

```
jik help
```

---
