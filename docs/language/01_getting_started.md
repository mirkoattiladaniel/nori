# 1. Getting Started

## Hello, world

Nori programs are organized as **projects**, managed by `roll`. Nori's build and
package tool. Create one, then build and run it:

```console
$ roll init hello
$ cd hello
```

`roll init` scaffolds a project: a `src/main.nori` entry file and a
`nori.manifest` describing it. Open `src/main.nori`:

```nori
// src/main.nori
fn main() -> Int {
    printl("Hello, world!")
    return 0
}
```

Every executable has an entry point; a function named `main` returning `Int`
(the process exit code; `0` means success). `printl` writes a line to standard
output. It takes exactly **one** value — there is no comma-separated form — so
build the line with `+` and convert non-strings with `to_str`:

```nori,excerpt
printl("10 + 10 = " + to_str(add(10, 10)))
```

Build and run with:

```console
$ roll run
Hello, world!
```

`roll run` compiles the project and runs it. `roll build` just compiles (leaving
an executable you can run directly). That is the whole loop you need to start.

## The shape of a program

Nori source is a sequence of **top-level items**: functions, type declarations
(`struct`, `enum`, `trait`), globals, `import`s, and a few others. Statements and
expressions only appear inside function bodies; there is no top-level executable
code and no top-level `let`.

```nori
import "std/os" as os          // bring in a module

global SCALE: Int = 2          // a top-level constant

struct Point { x: Int, y: Int }

fn dist2(p: Point) -> Int { return SCALE * (p.x * p.x + p.y * p.y) }

fn main() -> Int {
    printl(to_str(dist2(Point { x: 3, y: 4 })))   // 50
    return 0
}
```

Whitespace and newlines are not significant beyond separating tokens; statements
are not terminated by semicolons. A block `{ … }` groups statements, and the last
expression of certain blocks can be their value (see
[Control Flow](07_control_flow.md)).

## Working with `roll`

`roll` is the tool you use day to day. A project is a directory with a
`nori.manifest`:

```toml
# nori.manifest
name = hello
entry = src/main.nori

# `roll build` uses [profile.dev]; `--release` uses [profile.release];
# `--profile NAME` uses [profile.NAME]. Add as many as you like.
[profile.dev]

[profile.release]
# optimized; dev-only features (see below) are compiled out

[dependencies]
# name = { git = "…", tag = "…" }
```

The commands you will use most:

| Command | Does |
|---|---|
| `roll init [NAME]` | scaffold a new project (`--lib` for a library) |
| `roll run [-- ARGS]` | build, then run (forwarding `ARGS` to the program) |
| `roll build [--release]` | compile (optionally optimized) |
| `roll test` | run every `test_*` function in the project |
| `roll fmt [PATH…]` | format in place; the whole project, or the given files/directories |
| `roll doc` | generate documentation from `///` / `//!` comments |
| `roll add NAME …` | add a dependency and resolve it |

`roll` reads the manifest, resolves dependencies (semver, with minimal-version
selection so a diamond resolves to one version), caches compiled dependencies for
fast incremental rebuilds, and applies the selected build profile. Feature flags
declared in a profile (`cfg = ["devtools"]`) are compiled in for that profile and
provably absent from others, see
[Conditional Compilation](18_conditional_compilation.md). The full `roll` surface
(including `check`, `xray`, `law`, `shadow`, `verify`, `lsp`, `vendor`) is in
[Tooling](20_tooling.md).

## The compiler, `noric`

Underneath, `roll` drives **`noric`**, the Nori compiler (itself written in Nori
and self-hosting). You normally never invoke `noric` directly, because `roll` handles it, but it exists, and maintainers and advanced users use it to compile a lone file
or inspect a stage. Its most common modes are `noric --build in.nori out`,
`noric --run in.nori`, and `noric --check in.nori`. Its flags are documented in
[Tooling](20_tooling.md); throughout this reference, "the compiler accepts …"
refers to `noric`, whichever way you reached it.

## The standard library

The standard library lives under `std/` and is imported by path:

```nori
import "std/io" as io
import "std/os" as os
import "std/json" as json
```

It is written in Nori, with a small unsafe core for syscalls and FFI, and ranges
from data structures and text to a JSON parser, an HTTP server, a TrueType
rasterizer, windows and a UI toolkit. The
[standard library reference](../std/) covers every module.

---

Next: [Lexical Structure](02_lexical_structure.md); the tokens Nori is built from.
