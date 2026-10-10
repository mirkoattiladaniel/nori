# 20. Tooling

Nori ships two programs: `roll` (the project tool you use day to day) and `noric`
(the compiler `roll` drives). Both are written in Nori.

## `roll`

`roll` manages projects: build profiles, dependencies, tests, docs, and the
verification tooling. Run it in a directory with a `nori.manifest`.

| Command | Purpose |
|---|---|
| `roll init [NAME] [--lib]` | scaffold a project (or a library) |
| `roll build [--release] [--profile P] [--target T]` | compile (optimized / a profile / cross-compile) |
| `roll build --separate` | compile deps once into cached objects + interfaces, then link |
| `roll build --native` | build through the native backend (`noric --build --native`): no LLVM and no C compiler |
| `roll run [-- ARGS]` | build, then run, forwarding `ARGS` |
| `roll run --device` | run on the attached Android device instead of here: push the executable to `/data/local/tmp` and run it there, or install the APK and start it |
| `roll build --exe` | for an Android target, build a plain static executable instead of packaging an APK |
| `roll test` | run every `test_*` function in the project (a `tests/` file with none is checked and skipped) |
| `roll check` | type-check without building |
| `roll fmt [PATH…]` | format in place; the whole project, or the given files/directories |
| `roll doc` | generate docs from `///` / `//!` comments |
| `roll add` / `roll remove` | add / drop a dependency (resolves the manifest) |
| `roll update` / `roll vendor` / `roll prune` | manage the dependency lock and cache (`update deps` warns first about APIs the new versions removed) |
| `roll law` | fuzz `law` properties (`roll law prove <name>` scaffolds a Lean obligation) |
| `roll shadow` | run shadowed functions and tally divergences |
| `roll verify` | read back a proof-carrying evidence section |
| `roll caps` / `roll unsafe` | report capabilities / audit every `unsafe`/`extern`/`cstruct` |
| `roll xray` | value-lifeline + safety report |
| `roll db [IMAGE]` | what a persisted image holds, and whether this build loads it |
| `roll todo` / `roll fixme` | list `TODO`/`FIXME` markers |
| `roll learn` | the interactive tutorial (69 lessons in 14 stages), verified as you go |
| `roll lsp` | start the language server |
| `roll bench` | benchmarks (`--try-schedules` autotunes a `schedule`) |

Build profiles are `[profile.NAME]` sections in the manifest; a profile sets the
optimiser level, feature flags (`cfg = […]`), and the rest; every key is listed
in [the manifest](manifest.md). `roll build` uses `dev`, `--release` uses
`release`, `--profile NAME` uses that one.

Two other manifest sections travel with a library rather than a build: `[never]`
carries whole-program policies (see [Verification](19_verification.md)), and
`[removed]` carries tombstones for deleted public names, which `roll` collects
from every dependency and reports at the call site (see
[Modules](13_modules.md)).

## `noric`

The compiler. Usually invoked by `roll`, but directly useful for a single file or
to inspect a stage.

| Mode / flag | Purpose |
|---|---|
| `noric --build in.nori out` | build a native executable |
| `noric --run in.nori` | build to a temp and run |
| `noric --check in.nori` | type-check; report diagnostics only. It accepts and refuses exactly what a build does: the optional passes (auto-parallelism, the scalar ABI, loop interchange) run after the check, so no flag can change the answer |
| `noric --test in.nori` | run `test_*` functions |
| `noric --doc in.nori` | emit `///` / `//!` docs as Markdown |
| `noric --fmt in.nori` | format in place |
| `noric --build-unit in.nori out.so [--unit-name NAME]` | a shared object a running program loads: compiled against the `--sep-std` library DAG, so it carries neither std nor the runtime (see below) |
| `--import-root DIR` | add a root for `import` resolution |
| `--no-auto-free` | emit without the frees, deep copies and closure environments the ownership tree plans. Such a build leaks by construction, and nothing you ship should use it: a normal build plans the frees and there is no manifest key to turn them off. It stays for bootstrapping older compilers, and as a baseline to compare against: the same program built both ways must give identical output. A `--runtime` module is already planned without frees and does not need the flag. See [15](15_memory_model.md) |
| `--own-report FILE` | list every store a migration to the ownership tree must annotate (`sink`/`copy`/`view`) |
| `--debug` | located traps + call-chain backtraces (implies `--dwarf`) |
| `--dwarf` | DWARF line tables and call-frame information: `gdb` breaks by `file:line`, steps, and prints a backtrace with a source line on every frame, and `perf report` attributes samples to Nori lines. The LLVM path rewrites the front end's `// dwloc` statement markers into MLIR `loc(...)` attributes; a `--native` build writes `.debug_line`, a `.debug_info`/`.debug_abbrev`/`.debug_str` unit naming every function, and `.eh_frame` itself, with no LLVM, assembler or external linker involved. Neither path emits variable locations, so `print x` does not work; Linux x86-64 and Linux arm64 only; a native Windows build emits its `.pdata`/`.xdata` unwind tables and no DWARF. The generated code is the same with the flag and without it |
| `--overflow-checks` | signed `Int` `+ - *` trap on overflow instead of wrapping (`--no-overflow-checks` opts out; `roll` sets it from a profile) |
| `--cfg NAME` | activate a user `cfg` flag |
| `--no-scalar-abi` | keep a small all-scalar struct as a heap record across a call. On by default a build gives such a function a twin whose struct parameters are expanded to one scalar per field — transitively, so `Rect { min: Vec2, max: Vec2 }` expands to four floats — and whose struct return becomes one out-parameter per field. The original is kept, so containers, closures, traits and FFI never see a twin. The flag is the A/B: the same program must answer the same either way |
| `--never 'CLAUSE'` | add a `never` policy |
| `--xray` / `--memprof` | heap-lifeline report / per-site allocation profiler. The profiler is a malloc interposer written in C, so `--memprof` needs `--llvm`: the native back end refuses it by name rather than write a binary that would profile nothing |
| `--legacy-collections` | accept the removed free-function collection spellings (for bootstrapping only; not for new code) |
| `--db-info in.nori [IMAGE]` | what a persisted image holds, and whether this build loads / migrates / refuses it |
| `--suspendable` | report every function that can reach a pause point (await/sleep/channel/net), with the path |
| `--stackless` | accepted, a no-op: suspendable fns are *always* lowered to heap-frame state machines (there is no stackful mode to opt out of) |
| `--evidence` / `--verify` | proof-carrying binaries. The evidence section is appended to the finished image, so it rides on either back end |
| `--os T` / `--arch A` / `--target T` | choose / cross-compile the target (a triple sets both) |
| `--libc` | link libc (the Linux default is libc-free) |
| `--nolibc` | the other half of that dimension, and what `cfg(nolibc)` / `cfg(libc)` select on: link without libc. The Linux default; `--libc` is the opt-out. See [18](18_conditional_compilation.md) |
| `--freestanding` | build a **bare-metal image**: the program and the runtime are compiled for `--os bare` (so `cfg(bare)` blocks are the active floor and `cfg(linux)`'s syscall paths blank out), the bare floor replaces the C one, every function is stamped no-red-zone, and frame pointers are kept. The native back end only, and it needs a linker script and boot code; see the two rows below |
| `--lds FILE` / `--asm FILE` | for a freestanding image: the linker script it is laid out by (required), and the assembly file its boot code comes from (one at a time, and not needed at all if the program carries its own `@section` assembly block, see [17](17_unsafe_and_ffi.md)). `--flatbin` also writes the raw image beside the ELF |
| `--general-regs-only` | emit no SSE/AVX register at all. A kernel's interrupt handler runs before anything has saved FPU state, so code that touches a vector register there corrupts the interrupted program; this is how a kernel's own code is kept to the general registers. Goes with `--freestanding` and the native back end; naming it without either is an error that says so |
| `--cc PATH` | the C driver used to compile the shim and link (default `clang`) |
| `--sysroot PATH` | target sysroot passed to that driver |
| `--ccflag FLAG` | pass FLAG to that driver, compiling and linking (repeatable) |
| `--ldflag FLAG` | pass FLAG to the link step only (repeatable) |
| `--unit-host` | a program that loads units (`noric --build-unit`) at run time: export every symbol and route closures arriving from a unit |
| `--native` | **the default**, and a no-op alias for it: generate and link the executable with the compiler's own code generator and ELF linker — no LLVM, no clang, no C toolchain at all. Writing it out loud changes one thing: anything this back end cannot honour becomes an error naming the flag, instead of a reason to build through the other pipeline. Linux x86-64; Linux arm64 with `--target aarch64-linux-gnu`, or Android arm64 with `--target aarch64-linux-android` (or `--native` alone on an arm64 host): a static executable, SIMD vector types included, whose output matches the LLVM aarch64 build; a program that imports from a shared library is refused there, since the glibc-hosted floor is x86-64 only; and Windows x86-64 with `--target x86_64-w64-mingw32`: a PE32+ console executable over kernel32 with no mingw toolchain, whose output matches the LLVM build of the same target: async sockets, std/dl, header bindings to DLLs, windows through std/window's Win32 backend and sound through waveOut, both in Nori; a struct passed by value to C is not available there yet. A compiler running on Windows compiles through `--native-worker` processes of itself. The executable is static and libc-free unless the program can reach a function imported from a shared library (`extern fn … from "lib"`; an import nothing reaches does not count): then it is dynamically linked against glibc — ld.so loads glibc and the named libraries, the runtime allocates with glibc's `malloc` and starts its threads with `pthread_create` — still with no C compiler involved. C interop needs no C compiler for a header binding (`extern NS "header" link "lib"`): its glue is generated as Nori and the program imports the library, found through the `link` string's `-L` directories and `--ldir`, which the executable records as its run path (`$ORIGIN`-relative for a relative directory). The program's own inline `extern c { }` C is compiled by clang into `<program>.inline-c.so` beside the executable, which imports its functions from there. Windowing needs no C: a program that opens a window through std/window links the Nori implementation of its Wayland and X11 backends in place of the C shim, and needs no `--cfile`/`--clink`. Neither does audio: a program that plays sound through std/audio links a Nori backend in place of the libasound glue, speaking the PulseAudio native protocol to the sound server for `default`/`pipewire`/`pulse`/`sysdefault` and the kernel PCM ioctls for `hw:`/`plughw:`. std/dl loads shared libraries (the executable is then glibc-hosted), so a native host loads plugins built through LLVM and they call back into it. std/nori_ui's GPU painter links a Nori implementation of its wgpu glue that opens `libwgpu_native.so` at run time; offscreen canvases render as in an LLVM build, and a windowed canvas reports no GPU. The Nori modules that stand in for C glue are listed, with the reason each exists, in `std/native_seams`. The result is the same program, with identical output and exit status. The front end and the code generator run in one process, and the program's functions are compiled by forked workers (one per CPU, at most 8; `NORI_NATIVE_JOBS=N` sets the count, `1` compiles in one process) into the same executable byte for byte. `NORI_NATIVE_MLIR=1` also writes the program's MLIR to `OUT.mlir`. With `--dwarf` (or `--debug`) the backend writes its own DWARF — line tables, a compilation unit naming every function, and `.eh_frame` — so `gdb` and `perf` work on a native build; Linux x86-64 and arm64, not the Windows target. Builds in a fraction of the LLVM pipeline's time; a natively built compiler rebuilds itself to a fixed point. What it still cannot do — and what therefore needs `--llvm` — is listed in the `--llvm` row below |
| `--sep-std --native` | every std module in the import closure emitted once as MLIR behind an interface (`~/.nori/sepstd/*.nlib`); the front end then sees the program alone, and the native back end compiles program and std together, so calls into std are still copied into their callers. What `roll build` does for the dev profile. |
| `--llvm` | build through the LLVM pipeline: MLIR, `mlir-opt`, LLVM and `clang`. The native back end is the default; pass this for what it does not do: **macOS** (no Mach-O writer), **a relocatable object** (`--build-lib` without `--shared`, `--build-unit`, `--emit-interface`), **an arm64 or Windows shared library** (the native `.so` writer is Linux x86-64 only, so an Android APK's `libnoriapp.so` is built this way), **a build that links C** (`--cfile`, `--clink`, `--ccflag`, `--ldflag`, `--cc`, `--sysroot`; `--ldir` is honoured, except for a Windows target), **`--memprof`** (its profiler is a C interposer linked onto malloc), and **`--simd-force`**. Naming any of those without `--llvm` is an error that says so and names this flag; naming one alongside an explicit `--native` is an error too. The LLVM-only build modes select this pipeline on their own, because they exist only in it: `--incremental`, `--sep-std`, `--unit-host`, `--opt`, `--thin-lto`, `--no-lto`, `--lto-one`, `--no-native`. `NORI_TEST_LLVM=1` in the environment makes every build default to this pipeline |

Linux builds `x86_64` and `arm64`. The libc-free default needs no target sysroot, so a static
aarch64 binary cross-compiles from an x86-64 host with nothing installed at all (the back end that
writes it is the compiler's own), and through LLVM with nothing installed beyond clang:

```
noric --build prog.nori out --target aarch64-linux-gnu
noric --build prog.nori out --llvm --target aarch64-linux-gnu --nolibc
```

Android is the same build with a different triple, and still needs no NDK, because the kernel and
the syscall ABI are the same and the binary is static:

```
noric --build prog.nori out --target aarch64-linux-android --nolibc
```

The native backend takes the same triple, and then no C toolchain is involved at all:

```
noric --build prog.nori out --native --target aarch64-linux-android24
```

Test on a real phone: an emulator cannot stand in for a device here.

Linking against bionic instead does need the NDK. Keep using the host clang for it (the `.ll` was
produced by the host's LLVM, and an older NDK clang rejects attributes a newer LLVM emits) and give
it the NDK's sysroot and resource directory, which is where the `libclang_rt.builtins.a` and
`libunwind.a` it goes looking for actually live:

```
NDK=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64
noric --build prog.nori out --llvm --target aarch64-linux-android24 --libc \
      --sysroot $NDK/sysroot --ccflag -resource-dir=$NDK/lib/clang/18
```

`--cc` exists for toolchains that need their own driver end to end; Android does not.

### Units: code loaded into a running program

`noric --build-unit` builds a `.so` that shares the host's heap and handle tables instead of
bringing its own, so a `Vec` or a `Str` crosses in both directions. It compiles against
the same interfaces `--sep-std` builds and links with no runtime and no std: those symbols stay
undefined and resolve against the host at `dlopen`. The host is built with `--unit-host`, which
exports every symbol and gives its closure dispatcher the router a unit's closures travel through.
Whole-program, `--sep-std`, ThinLTO and full LTO hosts all work; the names match because a unit and
its host compile the same std sources with the same compiler:

```
noric --build host.nori host compiler/runtime.nori --import-root . --unit-host --opt 3 --thin-lto
noric --build-unit piece.nori piece.so --unit-name piece --import-root .
```

A unit's closures get their own id space, named by `--unit-name`, behind an exported
`<NAME>__closdisp`. The loader registers that dispatcher with the runtime (`nori_clos_dyn_add`)
before calling in, and a closure crossing between host and unit then routes like one crossing a
`--sep-std` library. The unit exports only its entry point, `<NAME>__closdisp` and
`<NAME>__nori_globinit`, so a second unit's identically named internals never bind to the first's.

`--ccflag` and `--ldflag` split for a reason. The C-interop glue is combined with `clang -r`, a
relocatable link, and `-r` refuses to sit beside `-shared` or `--export-dynamic`; so a flag every
object needs (`-fPIC`) and a flag only the final link takes (`-shared`) cannot travel together.
Building a shared library therefore reads:

```
noric --build app.nori libapp.so --llvm --target aarch64-linux-android24 --libc \
      --sysroot $NDK/sysroot --ccflag -resource-dir=$NDK/lib/clang/18 \
      --ccflag -fPIC --ldflag -shared
```

On Android that is how an app is built, together with the NativeActivity glue and the packaging.

## Learning the language: `roll learn`

`roll learn` is an interactive tutorial that turns the current directory into a
real `roll` project and works through 69 lessons grouped into 14 stages, from
"what is a function" to tasks, channels and `durable var`. Each stage teaches a
few ideas one exercise at a time and ends with a small **project** that uses them
together; the last stage ends with a capstone program.

```console
$ roll init myfirst && cd myfirst
$ roll learn                 # the current lesson: a short lecture, then a task
$ roll run                   # build and run your code, and see it work
$ roll learn check           # compile it with the lesson's hidden tests
```

| Command | What it does |
|---|---|
| `roll learn` | show the current lesson (scaffolding the workspace if needed) |
| `roll learn check` | verify `src/exercise.nori`; on success, advance to the next lesson |
| `roll learn hint` | reveal one more hint; each call gives the next one |
| `roll learn solution` | show the reference solution |
| `roll learn list` | every stage and lesson, with progress |
| `roll learn next` / `prev` | move without checking |
| `roll learn goto <n\|name>` | jump to a lesson by number, directory name, or title |
| `roll learn reset` | back to the first lesson |
| `roll learn verify-all` | check every shipped lesson |

The workspace is an ordinary project: `src/exercise.nori` is what you edit,
`src/main.nori` is a driver that calls it, and `roll build`/`run`/`check` all
work, so you are using the real toolchain from the first lesson rather than a
sandbox that behaves differently.

`check` compiles your file together with the lesson's tests and shows you the
compiler's own output when something is wrong, because that message is the most
useful thing on the screen at that moment. Unfinished work is preserved when you
jump between lessons, so `goto` never costs you an edit.

Lessons live in `$NORI_HOME/tools/learn/lessons/`, one directory each: a `meta`
(title, `kind`, and the `require`/`forbid`/`files` conditions), the `lecture.md` and
`task.md` a learner reads, a `hint.md` of `---`-separated hints, the
`starter.nori` and `driver.nori` installed into the workspace, and the
`verify.nori` and `solution.nori` that are not shown. `lessons/order` is the
table of contents, where a `## Title | blurb` line opens a stage.

## Testing and docs

Any top-level function named `test_*` is a test. `assert_eq`, `assert_true`,
`assert_streq`, etc. are available; `roll test` (or `noric --test file`) runs
them, with `--quiet` and `--timeout` options.

```nori,excerpt
fn test_add() { assert_eq(2 + 2, 4) }
```

A file under `tests/` with no `test_*` function has nothing to run; a `fn main`
harness, say. `roll test` type-checks it and reports it as **skipped**, never as
a pass, and its compile errors fail the run.

`noric --test` builds the harness the same way `noric --build` builds a program,
so a module with a C shim is tested by passing the shim along: `--cfile`,
`--clink`, `--ldir`, `--import-root` and `--sep-std` all reach the build. Under
`--sep-std` this is not optional; every function of every module in the closure
is emitted rather than only the reachable ones, so a seam the tests never call
still has to resolve at the link.

`///` documents the next declaration and `//!` the file; `roll doc` / `noric --doc`
render them to Markdown; the source of the API documentation.

## The formatter

`roll fmt` / `noric --fmt` is a lexer-aware, idempotent formatter: it normalizes
spacing and layout without changing meaning, and running it twice makes no further
change. It reindents by bracket depth, gives operators and separators canonical
spacing, strips trailing whitespace, and collapses runs of blank lines.

With no argument, `roll fmt` formats **the whole project**; every `.nori` file
under the project root, at any depth. It never descends into `build/`,
`vendor_nori/`, `testdata/`, or a dot-directory: those are build output, someone
else's source, and fixtures that may be deliberately malformed.

It also takes explicit targets, which need no `nori.manifest` at all; so this
works in any tree, including one that is not laid out as a roll project:

```sh
roll fmt                          # the whole project
roll fmt src/parser.nori          # one file
roll fmt std/archive              # a directory, recursively
roll fmt a.nori b.nori lib/       # several at once
```

A file named explicitly is formatted even if the walk would have skipped it —
you asked for it by name. A target that does not exist, or that is not Nori
source, is an error rather than a silent no-op.

A `struct` or `enum` declaration written on one line is expanded to one member per line:

```nori
struct Buf { p: Int, len: Int, cap: Int }
```

becomes

```nori
struct Buf {
    p: Int,
    len: Int,
    cap: Int
}
```

**Block bodies go one statement per line**, always, as rustfmt does. `fn f() -> Int { return 1 }`
becomes three lines, and so does every `if`, `while`, `foreach`, `match`, `unsafe` and `region` body.
`} else {` stays on one line. A struct literal (`Buf { p: 0, len: 1 }`) and an if-expression
(`var y = if c { 1 } else { 2 }`) are values, not blocks, and keep their braces on the line.

Two things about Nori make this the delicate part of the formatter. There are no statement
terminators, so what separates two statements written on one line is a convention, not syntax; a
boundary is detected structurally, as two complete expressions side by side with no operator joining
them. And `{` is overloaded across blocks, struct literals and if-expressions, so the brace's meaning
comes from what precedes it and whether the position is a statement one.

Newlines in Nori are not entirely insignificant: `break LABEL` must stay on one line, and a
statement opening with `(` fuses with the line above it, so token-identity alone would not prove a
reformat safe. The formatter's test suite therefore also builds the compiler from *fully
reformatted* sources and checks that it emits a byte-identical binary.

An `enum` splits on variants rather than commas, because its variants are not reliably
comma-separated (`enum Axis { AxisY AxisX }` and `enum Ev { A, B(Int) }` are both
written), and a comma is kept only where one was written; inventing one would
change the token stream, which is not a formatting change.

This is the one transform that adds lines rather than reindenting them, so it is
deliberately narrow. The line must be the whole declaration, with `struct` (or `pub
struct`) at the start and `}` at the end, so a trailing comment leaves it alone;
a comma inside a type (`Map<Str, Int>`) does not separate fields; a struct literal
in an expression (`Buf { p: 0, len: 1 }`) is not a declaration and is untouched;
and an empty body stays on its line. An already-expanded struct has nothing left
to split, which is why expanding is idempotent like everything else here.

## Inspection tools

- **`roll xray`** (`noric --xray`): the report of what the compiler does to your
  code: each heap value's lifeline (born → last use → freed), the safety posture,
  and every line-attributed optimization verdict (bounds-check erasure, SIMD,
  schedule reorder/tile, in-place string append, lock
  elision, Table auto-index rewrites, and the auto-par parallel/serial verdicts,
  `⟦par⟧`, with the reason whenever a loop stays serial).
- **`noric --memprof`**: a per-allocation-site profiler keyed by source
  location; prints the top allocators at exit. The first tool to reach for in a
  memory hunt.
- **`roll unsafe`**: the whole-tree audit of `unsafe`/`extern`/`cstruct`/`asm`.
- **`noric --vb-report ENTRY`**: where the value-binding keywords belong. The
  compiler walks every function it can reach and prints one record per site it
  can prove needs a marker: `recv <file>:<line> <fn>` for a method whose body
  mutates its receiver (so it should say `inout self`), `param <file>:<line> <fn>
  <name>` for a parameter the body writes through, and `view <file>:<line> <fn>`
  for a function that returns one of its own fields directly; an alias the
  caller could write through unless the signature says `-> view T`. This is a
  question no text search can answer, because a mutation may be a container
  method the sugar pass has not yet rewritten or a field write buried in a match
  arm. Records are keyed by file and line, so module mangling never confuses
  them.
- **`roll caps`** (`noric --caps`): classifies the tree's escape surface into capabilities
  (`net`, `fs`, `exec`, `env`, `time`, `rawmem`, `ffi`), and a manifest `[caps] NAME = net,exec`
  denies them for a dependency (reserved name `program` covers the whole tree), failing the build
  with the offending `file:line`. Classification is by *symbol name*, which is evidence about symbols
  resolving to the platform's libc; so when a build carries its own C (`--cfile`, `--clink`, or an
  inline-C block) no name counts as benign and every `extern` also carries `ffi`. Read the report as
  an audit of code you trust; it is not a defence against code written to fool it, since the name is
  chosen by whoever wrote the C.
- **`roll db [IMAGE]`** (`noric --db-info`): reads the header of a persisted
  image without running the program, and reports what a stored value would do on
  load: whether the file is intact (magic, format, CRC), which generation it is on
  (each commit bumps it; see the cross-process rules in
  [Variables](04_variables.md)), the shape it was written with against the shape
  declared now, and the verdict: *loads directly*,
  *migrates* (naming the converters that would run, in order), or *refused* with
  the clause you'd need to add. With no `IMAGE` it reports on every `durable var`
  the program declares; with one, on that file. It also notes a `.bak` left by a
  migration.

```
durable var d: Vec<User>
  d.ndb — 61 bytes, format 1, generation 3, crc ok
      declared : Vec<{name:Str,age:Int,tag:Str}>   0x00000000eabfc902
      on disk  : a different shape                 0x0000000075adf4a9
      verdict  : migrates — read as Vec<UserV1> -> up1 -> up2
```

## The language server

`roll lsp` starts the Nori language server (JSON-RPC over stdio): diagnostics and
document symbols, with hover/definition/completion. Point your editor's LSP client
at it for in-editor errors and navigation.

---

Next: [Keyword Reference](21_keyword_reference.md).
