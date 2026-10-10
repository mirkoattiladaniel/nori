# The manifest

Every project has a `nori.manifest` at its root. It is what makes a directory a
project: `roll` walks up from the file you are working on until it finds one,
and everything below that directory belongs to it.

The format is `key = value`, one per line, with `[section]` headers scoping the
keys beneath them and `#` starting a comment. There is no nesting and no quoting
to learn: a value runs to the first `#` after it, or to the end of the line.
That one rule is shared by all three readers (`roll`, the editor's highlighter,
and the schema that validates a manifest as you type), so a value and the note
you wrote beside it never disagree about where the value ended.

```
name = hello
kind = bin
version = 0.1.0
entry = src/main.nori

[profile.dev]
opt = 1
incremental = true

[profile.release]
opt = 3
lto = thin

[deps]
```

Every key below is optional except `name` and `kind`. An absent key is not the
same as a key set to `false`: absent means "whatever the compiler does by
default", which is documented per key and can change between releases, while
`false` is a decision this project has made and will keep.

`auto-free` is not a manifest key and nothing reads it: the ownership tree frees
every owner at the end of its scope in every build. If an older manifest carries
`auto-free = true`, delete the line. Building
*without* those frees is still possible and is a compiler flag rather than a
profile option: `noric --no-auto-free`, which exists for compiler development
and not as a build choice a project makes; see [tooling](20_tooling.md) and
[the memory model](15_memory_model.md).

## What `roll init` writes, and why

Each key below states the compiler's default (what you get when the key is
absent) and, where they differ, what `roll init` puts in the scaffolded
manifest. The two are not the same thing and were never meant to be: the
compiler defaults to the conservative choice, and `roll init` writes the choice a
new project usually wants.

| Key | Compiler default | `roll init` dev | `roll init` release |
|---|---|---|---|
| `backend` | the compiler's own code generator (`--llvm` / `backend = "llvm"` builds through LLVM) | absent | absent |
| `opt` | `2`, or `0` when `debug`/`dwarf` is on (LLVM pipeline only) | absent (so `0`) | `3` |
| `lto` | on, except under `dwarf`, freestanding, or explicit `vector` ops (LLVM pipeline only) | `false` | `thin` |
| `incremental` | off (LLVM pipeline only) | `true` | `true` |
| `overflow-checks` | off (wraps) | `true` | absent |
| `auto-order` | off | `false` | `true` |
| `auto-parallel` | off | `true` | `true` |
| `simd` | off | `true` | `true` |
| `erasure` | on | absent | absent |
| `debug` | off | `true` | absent |
| `dwarf` | off | via `debug` | absent |
| `auto-type` | on | commented out | — |

`opt`, `lto`, `incremental` and `unit_host` describe **LLVM's** optimizer and its per-unit path.
The default back end is the compiler's own, which has one optimization pipeline and compiles whole
programs, so on that path those keys name nothing and `roll` does not pass them on. A project that
means them asks for that pipeline: `backend = "llvm"` in the manifest, or `roll build --llvm`.
`roll` also switches to it on its own where the native back end cannot build the artifact at all: an
Android APK's arm64 `libnoriapp.so`, a macOS target, and a project whose `[native]` section (its
own or a dependency's) links C **that the native back end has no answer for**. The window shims,
the GPU glue and the libraries only they need are answered by Nori modules of its own
(`std/native_seams` lists them, under `=cfile`, `=clink` and `=ccflag`); a `[native]` section made
of nothing else stays on the native back end, and `roll` leaves those entries off its command
line. `roll build --native` says the opposite in so many words, and then the compiler refuses by
name rather than switching.

**What a dev build does on the default back end.** `roll build` (the `dev` profile) on Linux keeps
every std module in the project's import closure compiled once behind an interface
(`noric --sep-std --native`, under `~/.nori/sepstd`), so the front end checks and emits the project
alone; the back end still compiles project and std together, and takes every unchanged function
from its cache (`~/.nori/natcache`). The first build of a project makes the std modules it uses;
after that a rebuild pays for what was edited. `debug = true` works on this path. `roll build
--whole` opts out; a release build, a cross build, a project with `[never]` policies and a project
with dependencies are whole-program builds. The line `roll` prints says which it was:
`(native, std apart, …)`.

Keys that interact, all of which the compiler will tell you about at build time:

- `debug` implies `dwarf`, and `memprof` implies `debug`.
- A `dwarf` build with no explicit `opt` compiles at `0`. Setting `opt` above `0`
  alongside it keeps the line table and loses the variables, and `noric` says so
  on every such build. That is why the dev profile leaves `opt` commented out.
- A `dwarf` build declines LTO — the bitcode link drops the MLIR debug info — so
  `lto` in a debug profile sets nothing either way.
- `incremental` skips full LTO but honours `lto = thin`; ThinLTO's link is the
  one that combines with a per-module object cache. `lto = true` next to
  `incremental = true` is the combination that quietly does nothing.
- A program containing explicit `vector` operations declines full LTO too, to
  keep the fast-math and masked-compare lowering. `lto = thin` overrides that and
  says what it costs.

## `name`

What the project is called. It names the artefact that comes out of a build
(`build/dev/hello`), and it is the name other projects put in their `[deps]` to
depend on this one.

## `kind`

What to build: `bin` for an executable, `lib` for a library other projects can
depend on, or `plugin` for a shared object a host loads at runtime.

| Value | Produces |
|---|---|
| `bin` | an executable; needs an `entry` with a `fn main` in it |
| `lib` | a library other projects can depend on |
| `plugin` | a shared object a host loads at runtime; see [the plugin ABI](../std/nori_ui/plugin_abi.md) |

## `version`

The project's own version, as `major.minor.patch`. Dependencies are resolved
against it, and `roll` records it in the lock file.

## `entry`

The file a build starts from, relative to the manifest. Defaults to
`src/main.nori`. Everything it imports is compiled with it; nothing else in the
directory is, which is why a scratch file beside your source costs nothing.

## `exports`

Which modules a `lib` publishes. Absent publishes everything `pub`.

## `nori`

The minimum compiler version this project needs, as `major.minor.patch`. `roll`
checks it before building and says which version is required rather than failing
somewhere inside a compile.

## `auto-type`

`false` requires an explicit type on every `let` and `var`: `var a: I64 = 1`
rather than `var a = 1`. Inference is on by default, and `roll init` writes the
key commented out. A project-wide decision rather than a per-profile one: the
same source has to compile under every profile.

## `target`

The default target triple to cross-compile for. `roll build --target T`
overrides it.

## `[profile.NAME]`

A named set of build options. `roll build` uses `dev`, `roll build --release`
uses `release`, and `roll build --profile NAME` uses that one. A profile that
does not exist is not an error; it simply sets nothing.

The keys below all live inside a profile.

## `opt`

The optimiser level clang is run at: `0`, `1`, `2`, `3`, `s` or `z`. Default
`2`, except in a `debug` or `dwarf` build with no `opt` of its own, where it is
`0`, because that is what keeps every local in memory where a debugger can read
it. `roll init` writes `opt = 3` in release and leaves dev's commented out.

## `lto`

Link-time optimisation. On by default; `false` turns it off, and `thin`
selects ThinLTO: per-module optimisation in parallel plus summary-driven
inlining, which links far faster than full LTO for close to the same runtime.

"On by default" is the default *request*. Three things decline it on their own:
a `dwarf` build (the bitcode link drops the MLIR debug info), a freestanding
build, and a program with explicit `vector` operations (full LTO's codegen drops
the fast-math and masked-compare lowering). `thin` is honoured in all but the
first two, and prints what the vector program gives up.

## `incremental`

`true` caches each module's object and rebuilds only what changed, in parallel.
Off by default; `roll init` writes it in both profiles.

It combines with `lto = thin` and not with full `lto = true`: ThinLTO's
per-module backend is the one that survives a per-module object cache, and
`roll init`'s release profile pairs exactly those two. A `true` there is not an
error and is not applied.

## `unit_host`

`true` builds a program that loads units at run time: shared objects made with
`noric --build-unit`, which carry neither the runtime nor std and resolve both
against the program. The build
exports every symbol (`-Wl,--export-dynamic`) and gives the closure dispatcher the
router a unit's closures travel through. Works with every `lto` and `incremental`
setting. Off by default.

## `auto-order`

`true` lets the compiler interchange loop nests so the unit-stride dimension ends
up innermost, wherever it can prove the reordering legal. Off by default.
`roll init` writes `true` in release and `false` in dev; the analysis costs
compile time and MLIR size, which a release build is happy to pay for and a dev
rebuild is not.

(It is about loops, not struct layout. Field order is not a build option.)

## `auto-parallel`

`true` lets the compiler run `foreach` loops it can prove independent in
parallel, bit-identically to the serial order. Absent or `false` keeps them
serial; this is the one key where absent and `false` reach the compiler the same
way. `roll init` writes `true` in both profiles; the compiler and the standard
library stay serial because they do not.

## `simd`

`true` runs the cost model and vectorises what it judges worth vectorising,
`false` forbids it, and `force` vectorises even where the cost model would
decline. Off by default; `roll init` writes `true` in both profiles.

## `erasure`

`false` turns off generic type erasure, compiling a copy per instantiation
instead. Bigger output, no dynamic dispatch. Erasure is on by default and
`roll init` does not write the key.

## `overflow-checks`

`true` traps on signed `+`, `-` and `*` overflow instead of wrapping. Off by
default. `roll init` writes `true` in dev and leaves release wrapping, which is
the split worth keeping: each arithmetic operation pays for the check. Where a
release build *wants* defined behaviour on overflow, say it in the source with
`wrapping_*` or `saturating_*` rather than in the profile.

## `debug`

`true` emits source locations and backtraces for traps and `fail()`: `at
file:line` plus the call chain. Off by default; `roll init` writes it in dev.
Implies `dwarf`, and through it selects `opt 0` unless the profile names an `opt`
of its own.

## `dwarf`

`true` emits DWARF line tables, so `gdb`, `lldb` and `perf` can map an address
back to a file and a line. Off by default, and implied by `debug`.

With no `opt` in the profile it selects `opt 0`. With an `opt` above `0` the line
table survives and the variables do not (the optimiser promotes them out of
memory before the debug information is written), and the build says so:

```
noric: --debug with --opt 1: the line table survives, the variables do not
```

That warning is not spurious and is not suppressible; the fix is to drop `opt`
from the debug profile, which is what `roll init` now scaffolds.

## `memprof`

`true` turns on per-allocation-site memory profiling, dumped to stderr at exit.
Off by default. Implies `debug`, and so `dwarf` and `opt 0` with it; a profile
that sets `memprof` alongside an `opt` gets the same warning `dwarf` describes.

## `cfg`

Feature flags for this profile: `cfg = ["devtools", "telemetry"]`. Each name
makes `cfg(name) { … }` blocks compile in, and — just as importantly — makes
them provably absent from every profile that does not list it. Putting
`cfg = ["devtools"]` in `[profile.dev]` keeps the dev tools out of
`roll build --release` with no source edits and no dead branch to trust.

## `[deps]`

What this project depends on, one `name = version` per line. `roll add` and
`roll remove` edit it; `roll update`, `roll vendor` and `roll prune` manage the
lock and the cache.

## `[native]`

C sources and libraries to compile and link alongside: `cfile` names a source,
`clink` a library, `ldir` a directory to search.
A `[native.PLATFORM]` section applies only on that platform, which is how a
plugin links an import library on Windows and nothing on Linux.

## `[never]`

Whole-program policies that travel with a library rather than with a build: what
this code must never do, checked across everything that depends on it. See
[Verification](19_verification.md).

## `[removed]`

Tombstones for deleted public names. `roll` collects them from every dependency
and reports them at the call site, so a name that went away is an error that
says where it went rather than one that says it was never there. See
[Modules](13_modules.md).

## `[freestanding]`

For a build with no operating system under it: `asm` lists assembly sources,
`linker` names a linker script, and `flatbin = true` emits a flat binary rather
than an ELF (beside the ELF, which keeps the symbols). `general-regs-only = true`
keeps the kernel's own code out of the SSE registers (`--general-regs-only`).

`roll build --native` composes with it: the whole image (the boot assembly, the
layout the linker script names, and the flat binary) is then noric's own work,
with no clang, lld or GNU as under it.
