# Jik

_A readable, statically typed language that compiles to C and manages memory with regions._

<p align="center">
  <img src="assets/logo/jik-logo.png" alt="Jik logo" width="280" />
</p>

<p align="center">
  <a href="https://github.com/jik-lang/jik/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/jik-lang/jik/actions/workflows/ci.yml/badge.svg?branch=main&event=push"></a>
  <a href="https://jik-lang.org"><img alt="Website" src="https://img.shields.io/badge/website-jik--lang.org-111827"></a>
  <img alt="Status" src="https://img.shields.io/badge/status-alpha-b45309">
  <img alt="Backend" src="https://img.shields.io/badge/backend-C-2563eb">
  <img alt="Memory" src="https://img.shields.io/badge/memory-region--based-0f766e">
</p>

## [Try Jik in GitHub Codespaces](https://codespaces.new/jik-lang/try-jik?quickstart=1)
_Open a ready-to-use Jik development environment._

---

<a id="intro"></a>
## What is Jik?

Jik is a statically typed programming language with a straightforward source-to-C compilation model. It avoids both garbage collection and manual heap allocation.

It provides type inference, optional type annotations, vectors, dictionaries, options, variants, error handling, a standard library, and a growing package ecosystem. The result is concise source code and predictable generated C.

Jik's defining idea is its memory model: composite values are allocated into regions that determine their lifetime, while compiler checks prevent references across regions with incompatible lifetimes.

## Examples

<a id="hello-regions"></a>
<details open>
<summary><strong>Hello, Regions!</strong></summary>

Functions that return composite values let the caller choose the destination region:

```jik
func default_actions(r: Region) -> Vec[String]:
    return ["Open", "Save", "Quit"][r]
end

func main():
    actions := default_actions(_)
    println("first action: ", actions[0])
end
```

`_` is the current function's local region. For a longer-lived result, pass an explicit `Region` or the region of another composite value.

</details>

<details>
<summary><strong>Parsing a config</strong></summary>

```jik
use "jik/string"

func main():
    text := "host = localhost\nport = 8080\nmode = dev\n"
    settings: Dict[String]

    for line in string::split(text, "\n", _):
        parts := string::split(line, "=", _)
        if len(parts) != 2:
            continue
        end
        key := string::trim(parts[0], _)
        value := string::trim(parts[1], _)
        settings[key] = value
    end

    host := settings["host"]
    port := settings["port"]
    if host is Some and port is Some:
        println("connecting to ", host?, ":", port?)
    end
end
```

</details>

<details>
<summary><strong>Error handling</strong></summary>

```jik
throws func safe_div(x, y):
    if y == 0.0:
        fail("division by zero")
    end
    return x / y
end

func show_div(x, y):
    try value := safe_div(x, y):
        println(x, " / ", y, " = ", value)
    except:
        println("cannot divide ", x, " by ", y, ": ", error_msg())
    end
end

func main():
    show_div(12.0, 3.0)
    show_div(7.0, 0.0)
end
```

</details>

## Explore more

See the [examples](examples/) directory for [variants](examples/variants.jik), more [error handling](examples/error_handling.jik), [word count](examples/word_count.jik), [Dijkstra](examples/dijkstra.jik), [Newton's method](examples/newton.jik), [Game of Life](examples/game_of_life.jik), [Forth](examples/forth.jik), and [C interop](examples/ffi_demo.jik).

## Packages

[Jik packages](https://github.com/jik-lang/jik-packages) provide reusable libraries and C bindings for writing larger programs.

## Showcase

- [Play Missile Defence](https://jik-lang.org/showcase/missile-defence/) — a Jik game built with the [Raylib wrapper](https://github.com/jik-lang/jik-packages/tree/main/packages/raylib).
  - [Source code](https://github.com/jik-lang/jik-packages/tree/main/packages/raylib/examples/missile_defence)
  - Note: the browser demo is a separate WebAssembly build


<a id="quick-start"></a>
## Quick start

To start programming in Jik:

- [download the latest release](../../releases/latest) for your platform
- extract the release archive
- from the extracted directory, run `jik help` to confirm the executable works
- (optional) add the extracted directory to `PATH`
- if you plan to use `jik run` or `jik build`, either pass the compiler name with `--cc` or set `JIK_CC`, for example to `clang` or `gcc`
    - Windows: For a simple GCC setup, download from [WinLibs](https://winlibs.com/), and add its `bin` directory to `PATH`
- save a copy of [the example](#hello-regions) as `hello.jik`
- run it: `jik run hello.jik`
- generate C output: `jik tran hello.jik`
- build an executable: `jik build hello.jik`

Only `clang` and `gcc` host compilers were tested. MSVC was not tested.


<a id="build-from-src"></a>
## Building Jik from source

To build Jik from source, you will need `make` and a C compiler:

- download the Jik repository and navigate to the root
- run `make`

Additionally, to run the Jik test suite:

- run `make test`

Note that MSVC was not tested and is not the default build path in this repository

## Further reading

- [Documentation](docs/index.md)
- [Language overview](docs/overview.md)
- [Region-based memory management](docs/overview/08-memory-management.md)
- [CLI reference](docs/cli.md)
- [Standard library documentation](docs/overview/18-standard-library.md)
- [Development notes and roadmap](docs/development.md)
- [Official Jik website](https://jik-lang.org/)
