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

1. Translates the program and its imports into one C file.
2. Compiles that C file with the selected C compiler.
3. Runs the program and removes the temporary build files.

In other words, `jik run` is: **translate -> compile -> run**, in one step.

Two other important commands are:

- `jik tran hello.jik` to translate the program to C without compiling it
- `jik build hello.jik` to translate and compile the program without running it

`jik tran`, `jik build`, and `jik run` also support
`--unsafe-no-bounds-checks`, which disables runtime vector bounds checks for generated
`v[i]` reads and writes.

So the three most common commands are:

- `run`: translate, compile, and run
- `build`: translate and compile
- `tran`: translate only

Choose a C compiler with `--cc clang` or `--cc gcc`, or set `JIK_CC` to choose
a default. If neither is set, Jik looks for `clang`, `gcc`, then `cc`.

To get a list of commands and help for each one:

```
jik help
```

For a full list of commands and options, see the [Jik CLI reference](../cli.md).

---
