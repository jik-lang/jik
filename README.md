# Jik

_A readable systems-oriented language that compiles to C._

<p align="center">
  <img src="assets/logo/jik-logo.png" alt="Jik logo" width="280" />
</p>

<p align="center">
  <a href="https://github.com/jik-lang/jik/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/jik-lang/jik/actions/workflows/ci.yml/badge.svg?branch=main&event=push"></a>
  <a href="https://jik-lang.org/"><img alt="Website" src="https://img.shields.io/badge/website-jik--lang.org-1d4ed8"></a>
  <img alt="Status" src="https://img.shields.io/badge/status-alpha-b45309">
  <img alt="Backend" src="https://img.shields.io/badge/backend-C-2563eb">
  <img alt="Memory" src="https://img.shields.io/badge/memory-region--based-0f766e">
</p>

## [Go to Jik Playground](https://jik-lang.org/playground)
_The fastest way to see Jik in action._

## [Try Jik in GitHub Codespaces](https://codespaces.new/jik-lang/try-jik?quickstart=1)
_Open a ready-to-use Jik development environment._

---

<a id="intro"></a>
## What is Jik?

Jik is a statically typed programming language designed to be readable and easy to learn. It uses a straightforward source-to-C compilation model, region-based memory management
and avoids garbage collection.

Jik provides practical language features such as type inference, optional type annotations, vectors, dictionaries, options, variants, and more, together with a small standard library. The
result is a language that keeps common code concise and expressive, while still compiling to readable and predictable C.

Jik's memory model is one of its defining ideas. Instead of garbage collection or manual heap allocation, composite values are allocated into explicit regions that determine their lifetime, with language rules that prevent invalid cross-region use.


## Examples

These examples provide a quick tour of common Jik features.

<details open>
<summary><strong>Parsing a config</strong></summary>

```jik
use "jik/string" as string

func main():
    text := "host = localhost\nport = 8080\nmode = dev\n"
    settings: Dict[String]

    for line in string::split(text, "\n", _):
        if line == "":
            continue
        end
        parts := string::split(line, "=", _)
        if len(parts) == 2:
            key := string::trim(parts[0], _)
            value := string::trim(parts[1], _)
            settings[key] = value
        end
    end

    host := settings["host"]
    port := settings["port"]
    if host is Some and port is Some:
        println("connecting to ", host?, ":", must string::to_int(port?))
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
    println("forced result: ", must safe_div(8.0, 2.0))
end
```
</details>

<details>
<summary><strong>Working with variants</strong></summary>

```jik
variant Packet:
    ID: int
    TEXT: String
    BYTES: Vec[int]
end

func inspect(pkt):
    if pkt is Packet.ID:
        pkt[Packet.ID] = pkt[Packet.ID] + 1
        println("next id: ", pkt[Packet.ID])
    end

    match pkt:
        case Packet.ID{id}:
            println("id packet: ", id)
        case Packet.TEXT{text}:
            println("text packet: ", text)
        case Packet.BYTES{bytes}:
            println("bytes packet, len = ", len(bytes), ", first = ", bytes[0])
    end
end

func main():
    pkt := Packet.ID{41}
    println("raw id: ", pkt[Packet.ID])
    inspect(pkt)

    inspect(Packet.TEXT{"hello"})
    inspect(Packet.BYTES{[10, 20, 30]})
end
```
</details>

<details>
<summary><strong>Newton method</strong></summary>

```jik
use "jik/math" as math

struct NewtonResult:
    root: double
    converged: bool
    xs: Vec[double]
end

func solve(x0: double, tol: double, max_steps: int, r: Region) -> NewtonResult:
    xs := [0 of 0.0][r]
    x := x0
    push(xs, x)

    for step = 0, max_steps:
        fx := math::cos(x) - x
        if math::abs(fx) <= tol:
            return NewtonResult{root = x, converged = true, xs = xs}[r]
        end
        x = x - fx / (-math::sin(x) - 1.0)
        push(xs, x)
    end

    return NewtonResult{root = x, converged = false, xs = xs}[r]
end

func main():
    res := solve(1.0, 1e-12, 20, _)
    println("root: ", res.root)
    println("steps: ", len(res.xs) - 1)
    println("converged: ", res.converged)
end
```
</details>

For more examples, look in the [examples](examples/) directory or inside the Jik [test suite](test/jik/).


<a id="quick-start"></a>
## Quick start

To start programming in Jik:

- [download the latest release](../../releases/latest) for your platform
- extract the release archive
- from the extracted directory, run `jik help` to confirm the executable works
- (optional) add the extracted directory to `PATH`
- if you plan to use `jik run` or `jik build`, either pass the compiler name with `--cc` or set `JIK_CC`, for example `clang` or `gcc`
- (optional) use `jik doctor` to inspect the resolved paths and selected compiler
- create a file `hello.jik`:

```jik
func main():
    println("Hello, Jik!")
end
```

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

- [Language overview](docs/overview.md)
- [Region based memory management](docs/overview/08-memory-management.md)
- [Standard library documentation](docs/overview/18-standard-library.md)

---

<a id="repo-layout"></a>
### Repository layout

- `src/` - compiler source
- `jiklib/` - Jik standard library modules
- `support/` - Jik support library
- `test/` - test suite
- `docs/` - design notes and language [documentation](docs/overview.md)
- `examples/` - examples of Jik code
- `assets/` - assets (logo, etc)
- `scripts/` - dev scripts
- `tools/` - tools (syntax highlighting, etc)

<a id="roadmap"></a>
### Roadmap

- **0.1.x - Implementation stabilisation**
  - keep the language syntax stable (intended to be frozen)
  - find and fix bugs in the compiler and in the support library (`core.h`)
  - extend the standard library as needed
  - expand the test suite
  - write real programs in Jik to validate the implementation

- **Later**
  - bootstrapping: rewrite Jik in Jik
  - concurrency support (may be implemented either in the C implementation or during bootstrapping)
