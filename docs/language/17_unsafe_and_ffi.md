# 17. Unsafe and FFI

Nori's safety guarantees hold for all ordinary code. To step outside the model
deliberately — raw memory, atomics, calling C — you use `unsafe`, and the
compiler stops enforcing (and can no longer guarantee) safety within that block.
This is how the standard library's low level and the runtime itself are written.

## `unsafe` blocks

Raw-memory and atomic operations are only allowed inside an `unsafe { … }` block:

```nori,excerpt
unsafe {
    let p = malloc(16)
    poke(p, 42)                 // write an i64 at address p
    let x = peek(p)             // read it back
    poke8(p + 8, 255)           // write a byte
    free(p)
}
```

The raw primitives are `malloc`/`free`, `peek`/`poke` (i64), and their sized
variants `peek8`/`peek16`/`peek32`, `poke8`/`poke16`/`poke32`, plus float access
`peekf32`/`peekf64`/`pokef32`/`pokef64`. Atomics (`atomic_load`, `atomic_store`,
`atomic_add`, `atomic_cas`) are likewise `unsafe`. Addresses are plain `Int`s.

`peek`/`poke` are ordinary loads and stores: the optimizer may hoist a read out of
a loop that writes nothing, answer it from an earlier store, merge two stores to
one address, or drop a store nothing reads. For memory something else changes or
watches — a device's registers, a descriptor ring a device writes by DMA — use the
volatile variants `vpeek`/`vpeek8`/`vpeek16`/`vpeek32` and
`vpoke`/`vpoke8`/`vpoke16`/`vpoke32` (C's `volatile`, Rust's `read_volatile`):
each access is made where it is written, once, at its width, and in order with
every other volatile access. A register polled in a loop is read on every turn:

```nori,excerpt
unsafe { while (vpeek32(status) & READY) == 0 { } }
```

Volatile constrains the compiler only; it is not a fence, and orders nothing the
CPU does. They are `unsafe` like the rest, and allocate nothing.

`unsafe` is a promise you make to the compiler; a misuse is your bug, not a
diagnosed error. Keep unsafe regions small and wrap them in safe APIs (as
`std/os`'s `open_file`/`read_fd` do over the raw syscalls).

## Declaring C functions: `extern fn`

`extern fn` declares a function provided by the C ABI (libc, a linked library, or
the runtime floor). It keeps its real symbol name (no mangling) and is called
like any function, from within `unsafe` when it touches memory.

```nori
extern fn getenv(Int) -> Int        // char* getenv(const char*)
extern fn write(Int, Int, Int) -> Int
```

### Importing from a shared library: `from`

`from "LIB"` after the signature names the shared library the function comes
from. `LIB` is the name the dynamic loader looks up; a soname such as
`libz.so.1`, or a path.

```nori,excerpt
extern fn compressBound(Int) -> Int from "libz.so.1"
extern fn crc32(Int, Int, Int) -> Int from "libz.so.1"
extern fn cos(Float) -> Float from "libm.so.6"
```

- An LLVM build adds the library to the link (`-l:LIB`); `libc.so.6` adds
  nothing for an Android target, whose C library is already in the link.
- A `--native` build whose program can reach an imported function is a
  dynamically linked executable hosted by glibc; an import that is declared
  (say, by an imported std module) but never reached leaves it static. `LIB`
  and `libc.so.6` are loaded at startup, and every import is resolved before
  `main` runs. A name the library does not export,
  or one that is not a function, is a link error.
- In that build, C names the program uses without `from` (`malloc`, `strlen`,
  `snprintf`, …) bind to glibc or libm. `malloc` and `free` are glibc's, so a
  pointer can be freed on either side of the boundary.
- A cstruct in the signature passes by value (see *Calling C directly* below).
- A variadic function (`snprintf`) is declared with the arguments you pass.
  In a `--native` build every call through an import sets the vector-register
  count a variadic callee reads (`%al`), so these calls follow the SysV ABI.
- Only functions can be imported, not data symbols such as `environ` or `stderr`.

## C-ABI structs: `cstruct`

A top-level `cstruct` declares a struct with C memory layout, fixed-width fields
laid out contiguously, for building records to hand to C. Fields use
the sized integer types (or `Ptr`). `cnew(Name)` allocates a zeroed instance,
`cfree` releases it, and `s.field` reads/writes the right bytes at the right
offset; `sizeof(Name)` / `offsetof(Name, f)` / `addr(s)` are available for
layout and chaining. Each is resolved to a literal when the program is built, and
one that names nothing is a build error: a record that is not a cstruct, a field
the record does not have, or a dotted path that goes on through a field that is
not itself a record (`offsetof(Flat, a.x)` where `a` is a `U32`). A misspelled
field is never quietly offset 0.

```nori
cstruct Pt { x: I32, y: I32 }       // two 4-byte ints, C layout

fn main() -> Int {
    unsafe {
        var s = cnew(Pt)            // a zeroed Pt in raw memory
        s.x = 3                     // one field-write per line
        s.y = 4
        printl(to_str(s.x + s.y))   // 7
        cfree(s)
    }
    return 0
}
```

A cstruct type name used as a **type annotation** (`let v: Pt = ptr`, `var v:
Pt = ptr`, a param `f(v: Pt)`, a global `global cur: Pt = 0`) makes the value a
*view*: a name for a record that lives at an address the program already holds.
The annotation becomes an `Int` — the handle is the address itself — and
`v.field` reads and writes the right bytes at the right offset with no
allocation and nothing owned: the memory belongs to whoever allocated it. This
is how code lays named fields over memory it did not get from `cnew`; a kernel
over memory it `malloc`ed itself and shares with assembly, a program over a
handle another library handed it:

```nori
cstruct Evdev { buf: Ptr, r: I64, w: I64 }

fn ev_ready(ev: Evdev) -> Bool { unsafe { return ev.w > ev.r } }

fn main() -> Int {
    unsafe {
        let d = cnew(Evdev)
        d.w = 5  d.r = 3
        printl(ev_ready(d))             // true
        cfree(d)
    }
    return 0
}
```

The same sugar covers an array of records: a base, an index, `sizeof` as the
stride, one view per element.

```nori
cstruct Evdev { buf: Ptr, r: I64, w: I64 }

fn third(table: Int) {
    unsafe {
        let d: Evdev = table + 2 * sizeof(Evdev)    // element 2 of an array of Evdev
        d.w = 42
    }
}
```

A binding lives from its declaration to the end of its enclosing group (the
function body for a parameter, the braces for a `let`/`var`, the end of the
program for a global), so two functions that share a parameter name typed as
different cstructs never reach through each other's layouts. A copy of a view
(`var w = v`, with exactly one identifier on the right) stays a view of the same
record: it copies the handle, aliases the memory, and deep-copies nothing.

Fields may be fixed arrays (`[Byte; 16]`, as C's `char name[16]`) and other
cstructs, laid out inline under C rules; a nested type may be declared after
its user. A whole array or record field reads as *the address of that field*
(the way C arrays decay), so it can be bound as a view of its own type or handed
to code that takes an address; an element access reads and writes the element
itself:

```nori
cstruct Inner { a: I32, b: I32 }
cstruct Outer { tag: I64, in_: Inner }
cstruct Person { id: I64, name: [Byte; 16] }

fn demo(p: Person, o: Outer, x: Int) -> Int {
    unsafe {
        p.name[3] = ord("a")            // one byte at p + offsetof + 3
        poke8(p.name + 5, x)            // the same, by address
        o.in_.b = 5                     // through a nested record, one hop at a time
        let e: Inner = o.in_            // the sub-record as a view of its own type
        e.a = 1
        return offsetof(Outer, in_.b)   // dotted paths walk nested records
    }
}
```

Writing a *whole* array or record field (`p.name = …`, `o.in_ = …`) is not
expressed; the checker refuses it loudly ("`Int` has no fields") rather than
guessing what it would mean.

Several field writes may share a line, and a write's right-hand side may be any
expression: a call, an index, an `if`-expression, or one that wraps across lines:

```nori,excerpt
s.x = 3  s.y = 4                    // two writes, one line
s.x = 1 + 2  s.y = s.x * 10         // the second reads the first
s.z = (1 + 2) *
      (3 + 4)                       // a wrapped right-hand side
```

> **Note.** The `s.field` sugar is a source rewrite that emits the same
> `peek`/`poke` you would write by hand; the two spellings build byte-identical
> binaries, and field access costs no more than the load it names. It is
> unsafe-gated like the raw operations under it, so a handler marked `@irq` can
> name fields (naming allocates nothing; the checker proves that for whole call
> trees and refuses a tree that allocates), and it works under `--freestanding
> --general-regs-only`. A view write's right-hand side ends at the first point
> the expression is *complete*: a line break closes it only if nothing is left
> open (a trailing operator or an unclosed bracket continues onto the next
> line). Chaining stops where the type is not a cstruct: a `Ptr` field reads as
> the pointer's value; bind the pointee as a view yourself (`let par: Proc =
> peek(p + offsetof(Proc, parent))`) and name its fields from there.

Bindings generated by C interop (below) also expose C `struct`s this way; you
read `p.x` on a by-value struct returned from a C call.

## Records: raw layouts as types {#records}

A `cstruct` view is a rewrite: its type is gone before the checker runs, and an `Int` can stand for
any of them. A **record** is the same kind of raw layout as a type the compiler keeps, for memory a
program manages itself, a kernel's task and process records above all:

```nori
record Timer { at: Int, armed: Bool }
record Thread {
    id: I32,                    // a scalar: Int/I64/U64, I32/U32, I16/U16, I8/U8/Byte, Bool
    next: ref Thread,           // the address of a Thread, or null
    owner: ref any,             // the address of some record, which one known only at run time
    wake: Timer,                // a record inline
    regs: [Int; 4],             // a fixed array inline (of scalars, records, references or functions)
    run: fn(Int) -> Int,        // a function reference
}

let t = rnew(Thread)            // zeroed; the program's allocator is told the type (below)
t.id = 0 - 7                    // a store of the field's width
t.wake.armed = true             // an inline record's field
t.regs[3] = 42                  // an element, the index checked against the length
let n = t.next                  // a reference reads as its record
if rnil(n) { printl("no next thread") }
```

- **A record value is an address.** It passes and stores as an `Int` does, and nothing about it is
  freed or cloned; a `Vec<Thread>` holds addresses. It is not an `Int`: an integer becomes a record
  only through `rview(R, a)`, in `unsafe` code, and a record an integer through `raddr(r)`; a record of
  one type is not one of another.
- **The layout is C's.** Each field sits at its alignment, the record's alignment is its widest
  field's, its size is rounded up to that; `sizeof(R)` says it, and `offsetof(R, f)` where field `f`
  is (for assembly, and for code still reading the layout by hand). A signed narrow field is
  sign-extended when read, and every narrow field is truncated when written.
- **Fields.** Reading `r.f` is the load at its offset; writing it, the store. An inline record or
  array reads as *its* address (`r.wake` is a `Timer`), and is written a field at a time; assigning
  one whole is refused. A `ref R` field takes an `R` (or `rnull(R)`); a `ref any` field takes any
  record, and what is read from one is named with `rview` before its fields are used. Both spellings are
  types anywhere — a struct's field, a global, a parameter — `ref R` being `R` itself. A function
  field is checked as any function-reference slot is, and called as a method: `r.run(3)`.
- **Making and giving back.** `rnew(R)` calls `nori_rec_new(size, type id)` and `rfree(r)` (unsafe)
  `nori_rec_free(address, type id)`; the runtime's are `calloc` and `free`, and a program may define
  its own (a kernel keeps each type's live records on a list; a live update finds them there). The
  type id is a hash of the record's name, the same in every build.
- **Interrupt handlers.** Field access allocates nothing, so an `@irq` function may read and write
  records freely; `rnew` and `rfree` go through the allocator and are not allowed there.

## Packed C-ABI structs: `packed cstruct` {#packed-cstruct}

C's natural alignment is exactly right for binding C structs, and exactly
wrong for the tables x86 actually reads. A `packed cstruct` lays every field at
offset 0, 1, 2, …: alignment 1, the record's own alignment 1, and no trailing
round-up to the record's alignment. Everything else about a cstruct is
unchanged: `cnew`/`cfree`, `sizeof`, `offsetof`, views, nested records, fixed
arrays.

```nori
packed cstruct Gdtr { limit: U16, base: U64 }   // base at 2, sizeof 10
```

`cstruct` is C layout; a plain `cstruct Gdtr { limit: U16, base: U64 }` puts
`base` at 8, but the GDTR wants it at 2, and nothing complains when you get it
wrong: `sizeof` is wrong, `offsetof` is wrong, every store goes somewhere
plausible, and the machine triple-faults on the first interrupt from ring 3.
`packed` is how the kernel names those tables:

```nori,excerpt
packed cstruct Tss {
    rsvd:  U32,                    // +0
    rsp0:  U64, rsp1: U64, rsp2: U64,   // +4, +12, +20
    rsvd1: U64,                    // +28
    ist1:  U64, ist2: U64, ist3: U64, ist4: U64, ist5: U64, ist6: U64, ist7: U64,   // +36 .. +84
    rsvd2: U64,                    // +92
    rsvd3: U16,                    // +100
    iomap: U16                     // +102 ; sizeof 104
}
packed cstruct IdtGate { lo: U16, sel: U16, ist: U8, attr: U8, mid: U16, hi: U32, rsvd: U32 }
    // sizeof 16, every field at its byte
```

A `//` layout note inside the braces is fine; it ends at its line.

**A packed field may be misaligned, which is free on x86-64 and costly
elsewhere.** On x86-64 each field access is a single `peek`/`poke` (an
unaligned load or store), which the hardware handles with no penalty for these
sizes. On a strict-alignment target (ARM in `SMP` mode, many DSP cores) a
misaligned 64-bit field faults. `packed` states the *intent* that the record is
an on-the-wire or CPU-defined layout; it does not on its own make every target
fast at a misaligned access. If a program's packed records must run on
non-x86-64 hardware, the writer of that layout is responsible for what an
unnatural access costs (or for not naming such a field, `peek8`/`poke8` a byte
at a time).

**A nested `packed cstruct` propagates packing; an *unpacked* one may not sit
at a misaligned offset.** Inside a packed record, a nested cstruct field whose
offset is not a multiple of the nested type's own alignment must itself be
`packed`; otherwise its base would not have the alignment its own layout
claims, and code that binds it as a view of its own type would reach a pointer
with the wrong alignment. That is refused:

```text
noric: `Blob.item` of type `Item` is at the misaligned offset 1 inside packed `Blob`: a sub-record misaligned by packing must itself be `packed` (its own layout would claim an alignment its base cannot have)
```

A `packed` sub-record (alignment 1) may sit anywhere; an unpacked one at a
*aligned* offset is fine (its own layout is then correct). The rule is
strictly the safe reading: silently laying an unpacked record out at a
misaligned offset would be the worst of the three, and it is not allowed.

> **Note.** `packed` is a declaration form for the raw, top-level cstruct,
> the one a kernel writes directly. It is not
> C's `#pragma pack`, and it does not change how C-interop (bindgen/`extern c`)
> lays out the structs whose bytes a C compiler owns: those stay whatever the
> compiler and headers say, because the C code reads them. Byte order and
> sub-byte fields are separate features: `packed` gives you the bytes, not the
> meaning of a big-endian field or the bit within a dword a hardware register
> wants.

The `s.field` sugar, `sizeof`/`offsetof`, `cnew`, and views are the cheapest by
construction: this is a source rewrite that emits the same `peek`/`poke` you
would write by hand, so a packed program and the same program with its offsets
written out compile to byte-identical binaries.

## Calling C directly

`extern fn` imports, C function pointers and Nori functions handed to C follow
the C calling convention of the target — the x86-64 System V ABI on Linux — with
no generated C glue.

### Structs by value

In the signature of an `extern fn`, of a `CFn` type and of a function passed to
`cfn_addr`, a cstruct name means the struct **by value**, as in C. A pointer to a
struct is an `Int` there.

- An argument is the address of the struct's bytes (a `cnew` block or a view),
  and the call copies those bytes.
- A result arrives in a fresh zeroed block of the struct's size. The caller owns
  it and releases it with `cfree`. Bind it with the cstruct as the annotation to
  read its fields.

```nori,excerpt
cstruct Vec2 { x: F64, y: F64 }
extern fn vec2_add(Vec2, Vec2) -> Vec2 from "libgeom.so.1"

fn doubled() {
    unsafe {
        var a = cnew(Vec2)
        a.x = 1.0  a.y = 2.0
        let s: Vec2 = vec2_add(a, a)   // a fresh block: s.x == 2.0, s.y == 4.0
        cfree(s)
        cfree(a)
    }
}
```

Each struct is classified as the ABI requires: a struct of at most 16 bytes
travels in integer and vector registers, one per 8-byte part (a part holding only
`F32`/`F64` fields takes a vector register); a larger one, or one that no longer
fits in the registers left, is copied onto the stack, and the arguments after it
still take registers. A result of at most 16 bytes comes back in `rax`/`rdx` and
`xmm0`/`xmm1`; a larger one is written through a hidden pointer the caller
supplies.

### Function pointers: `CFn`

`CFn(T, …) -> R` is the type of a C function pointer with that signature;
`CFn(T, …)` returns nothing. Its parameter and result types are `Int`, the sized
integers, `F32`, `F64`/`Float`, cstructs (by value), and `Ptr` or another `CFn`
(both pointer-sized integers). A `CFn` value is an address: an `Int` binds to a
`CFn`-typed name, and a `CFn` value passes wherever an `Int` does.

Calling a `CFn`-typed local, parameter or global is a C call through the pointer,
and is an `unsafe` operation; nothing checks that the address is a function of
that signature.

```nori
extern fn get_compare() -> CFn(Int, Int) -> I32 from "libcompare.so.1"

fn compare_through(a: Int, b: Int) -> Int {
    var r = 0
    unsafe {
        let cmp: CFn(Int, Int) -> I32 = get_compare()
        r = cmp(a, b)
    }
    return r
}
```

A call goes through a name. A `CFn` held in a struct field or a container is
bound to a local first; inside a closure, a `CFn` name must have the same type
everywhere in the program. Variadic C function pointers are not supported.

### Nori functions as callbacks: `cfn_addr`

`cfn_addr(f)` is the address of a C-callable entry for the top-level function
`f`, as an `Int`; it is `unsafe`. Every parameter and the result of `f` must be
one of the types above, taken as a plain value (not `inout`, `sink` or `set`).

```nori
extern fn qsort(Int, Int, Int, CFn(Int, Int) -> I32) from "libc.so.6"

fn ascending(a: Int, b: Int) -> I32 {
    var r: I32 = 0
    unsafe {
        if peek(a) < peek(b) { r = 0 - 1 }
        if peek(a) > peek(b) { r = 1 }
    }
    return r
}

fn sort_ints(buf: Int, n: Int) {
    unsafe { qsort(buf, n, 8, cfn_addr(ascending)) }
}
```

- C may call the entry at any time and from any thread, including one the Nori
  runtime did not start. The entry makes the runtime usable on that thread before
  `f` runs, so a callback allocates, prints and calls Nori functions as any code
  does. Taking an address with `cfn_addr` switches the runtime to its
  thread-safe mode.
- The entry keeps the registers C expects a callee to preserve.
- A by-value cstruct parameter reaches `f` as a view of a copy that lives for the
  duration of the call. A cstruct result must be a fresh `cnew` block: the entry
  copies its bytes to C and frees it.
- A closure has no C-callable address — C passes no environment — so the argument
  must name a top-level function.

## Binding C libraries

Two forms generate the marshalling glue for you. Both are **namespaced**; the
name after `extern` (`ml`, `curl`, …) is the namespace you call the bindings
through (`ml::add(…)`); a bare, non-namespaced `extern "h" link "l"` is rejected.

- **Header binding with a block**: list the `cstruct`s and `fn`s to bind; the
  compiler marshals `Str`s, structs, enums, arrays, callbacks, and varargs across
  the boundary.

  ```nori
  extern ml "mylib.h" link "mylib" {
      cstruct Pt { x: I32, y: I32 }
      fn sum_pt(p: Pt) -> Int          // call as ml::sum_pt(…)
      fn greet(name: Str, n: Int)
  }
  ```

- **Header binding without a block (bindgen)**: the compiler reads the header
  with [std/c](../std/c.md), as a C compiler does (preprocessor, includes,
  macros, layouts), and binds the library it declares.

  ```nori
  extern curl "curl/curl.h" link "curl"    // the library's declarations, as curl::…
  ```

  What is bound: the functions, enumerators, integer `#define` constants,
  structs and function-pointer typedefs the header declares. A header that
  declares no functions and only gathers others binds the library files it
  reaches instead: those it includes with `#include "…"`, and those under its
  own directory when that is not a system include directory
  (`alsa/asoundlib.h` binds `alsa/*.h`; no binding includes `stdio.h`).
  Typedefs are followed to what they name. A struct Nori can hold by value
  becomes a `cstruct`; every bound struct gets `T_get_f` / `T_set_f`
  accessors and `T__new` / `T__free` / `T__sizeof`. A variadic function binds
  its fixed parameters.

  The header is read for the target: building for Windows
  (`--target x86_64-w64-mingw32`) reads it as mingw-w64 GCC does, from the
  mingw-w64 headers (`--sysroot` replaces `/usr/x86_64-w64-mingw32`), with
  `_WIN32` defined and the LLP64 data model. There `long` and `unsigned long`
  are 32 bits: they bind as `Int` and `U32`, an argument keeps its low 32 bits,
  a result is sign- or zero-extended, a `long*` parameter is a `Vec<I32>` /
  `Vec<U32>`, and struct layouts are Windows' (4-byte longs, `-mms-bitfields`).

  What is left out: a function taking a `va_list`, a union by value, a callback
  that is variadic, takes more than six parameters or a type with no mapping,
  a struct Nori cannot hold by value, or `long double` / `__int128` /
  `_Float16` / complex values. `noric --symbols app.nori --out syms
  --bindgen-report` lists each file covered, each function bound, and each
  function left out with its reason.

  A function the header declares but the linked shared library does not export
  still links; calling it stops the program with a message naming it.

- **Inline C**: `extern c { … }` embeds a snippet of C directly; the compiler
  compiles and links it and exposes its functions.

In a `--native` build no C compiler runs for a header binding: the glue of both header
binding forms is generated as Nori and calls the library through its exports, so the
executable is dynamically linked against it (a `link` name resolves to the soname of its
`lib<name>.so`). A `-L<dir>` in the `link` string, and `--ldir <dir>` (a manifest's
`[native] ldir`), are searched for those libraries and recorded as the executable's run
path (`DT_RUNPATH`): a relative directory from the executable's own directory
(`$ORIGIN/...`), so `link "-Lvendor mylib"` runs without `LD_LIBRARY_PATH`, also after the
project is moved whole. The bindings, their names and their marshalling are the same as in an
LLVM build, with these differences:

- a `static inline` header function the library does not export is left out
  (`--bindgen-report` says `inline function the library does not export`); in a
  block, a function the library does not export is an error at the binding;
- a struct by value is left out (in a block, refused) when its members do not lay
  out as a flat `cstruct` of its scalar members would;
- the C of an inline `extern c { … }` block is compiled with the system C compiler
  (clang) into a shared object beside the executable, `<program>.inline-c.so`, and its
  functions are imported from there with the same glue a header function gets; the
  executable's run path names its own directory, so the two must stay together. A block
  whose functions are all answered by a Nori module listed in `std/native_seams` (as the
  standard library's own blocks are) needs no C compiler. Linux x86-64 executables only:
  for Windows, arm64 and a native shared library, such a block is an error naming `--llvm`.

These power the bindings for `curl`, X11, and `wgpu` used by parts of the standard
library. Interop details (how each type marshals) are in the C-interop notes.

## Inline assembly and raw entry points

For the lowest level — a libc-free runtime, a syscall, a bare-metal entry — Nori
has:

- **`asm( … )`**: inline assembly / a raw syscall, lowering to LLVM inline asm.
- **`naked fn`**: a function with no prologue/epilogue, for context switches and
  process entry (`_start`).

```nori,excerpt
fn nori_sys3(n: Int, a: Int, b: Int, c: Int) -> Int {
    var r = 0
    unsafe { r = asm(...) }         // syscall instruction
    return r
}
```

On Linux/x86-64 these let Nori build fully libc-free, with its own `_start`, TLS, and
raw `socket`/`clone`/`futex`/`epoll`, and the whole runtime in Nori. This is
advanced, platform-specific territory; ordinary programs never need it.

## Placing a block: `@section`, `@align`, `@code32`

A `naked fn` can say **where in the image its bytes go** and **in which mode they are
assembled**, with annotations in front of it:

| | |
|---|---|
| `@section(".name")` | the section its bytes land in: `.text`, `.rodata`, `.data`, `.bss`, `.mbheader` |
| `@align(N)` | align it to `N` bytes where it is placed |
| `@code32` | assemble its first instruction in 32-bit mode (`.code64` is the default) |

An annotated `naked fn` is an **assembly block**: its body is one operand-free
`asm("…", "")`, and that text is assembler source, a subset of GNU as,
directives and labels included, with no `$`
doubling and no `$N` operands. The function's own name becomes the block's first
label, and every other label it defines becomes a symbol of the image.

```nori,excerpt
@section(".mbheader") @align(4)
naked fn _mb_header() {
    unsafe { asm(".long 0x1BADB002\n" +
                 ".long 0x00010003\n" +
                 ".long _start\n", "") }
}

@section(".text") @code32
naked fn _start() {
    unsafe { asm("cli\nmovl $_stack_top, %esp\n…", "") }
}
```

This is how a kernel writes its own boot trampoline in Nori; see
`examples/systems/kernel/boot.nori`, which is a multiboot header, a 32→64-bit long-mode
switch, a GDT and a stack, and leaves a kernel with no `.S` file at all.

### Reserving storage, not code

Nothing says the bytes have to be instructions. Because the block's text is
assembler source and its name becomes a label, an annotated `naked fn` is also
how a freestanding program declares **a fixed block of memory**: the thing a
kernel needs before it has a heap, and which `global` cannot express (a `[T]`
global is a data-segment array, but it carries no section, no alignment beyond
its element's, and no way to be filled from a file):

```nori,excerpt
@section(".bss") @align(4096)          // a page-aligned table
naked fn idt_table() {
    unsafe { asm(".skip 4096\n" +
                 "idt_desc: .skip 16\n", "") }
}

@section(".bss") @align(64)            // a cache line to itself, so no two locks share one
naked fn pmm_lock_word() { unsafe { asm(".skip 64\n", "") } }

@section(".rodata") @align(16)         // a file compiled into the image
naked fn font8x16() { unsafe { asm(".incbin \"src/kernel/font8x16.inc\"\n", "") } }
```

`.skip N` reserves N zeroed bytes and `.incbin "path"` inlines a file's bytes;
both are ordinary GNU-as directives, so anything else in that subset works too.
`@align` is doing real work here; a cache line for a lock word, a page for a
table the hardware requires aligned.

Such a block is **not called**. What you want is its address, and an ordinary
`asm` with an operand gets it:

```nori,excerpt
fn idt_addr() -> Int {
    var v = 0
    unsafe { v = asm("movabsq $$idt_table, $0", "=r") }
    return v
}
```

The `$$` is a literal `$` escaped: this `asm` has operands (`$0` is the output),
which is exactly the rule the annotated block above does not follow; an
operand-free block's text is passed through as written.

**Only `noric --build … --native --freestanding` places bytes by section name.**
Every other output path — the LLVM path, a PE image, arm64 — refuses a program
carrying one of these annotations by name rather than laying the bytes somewhere
else.

## Interrupt handlers: `@irq` {#irq}

An interrupt arrives between any two instructions of the code it interrupts; the
runtime's allocator included, halfway through updating its lists. So an interrupt
handler must not allocate, and `@irq` in front of a function makes the checker hold
it to that: the function, and **everything it calls, however deep**, may not make a
runtime heap value.

`@irq` means the other half of what interrupt handler means, too: it must never
block. Nothing can wake the task it interrupted; the blocked handler would sleep
forever, on whatever CPU it landed. So the same walk proves the handler's whole
call tree never waits: no `@sleeps` callee, no `await`, no `lock` statement, no
`parallel foreach` join. (A spin-wait, testing a device register in a tight loop,
is not blocking, and stays fine.) On an `extern fn` or a `naked fn`, which have no
body to walk, the mark is the author's word for both halves.

```nori,excerpt
@irq
fn irq_dispatch(v: Int) {
    if v == VEC_TIMER() { timer_tick()  lapic_eoi()  return }
    kputs("unexpected interrupt, vector ")   // fine: a literal argument never allocates
    kput_dec(v)
}
```

What counts as allocating is what the code generator turns into a call that takes
from the heap:

- a struct, an enum value (even one without a payload), a tuple, a closure;
- a string concatenation, and a literal a variable is given (`let s = "…"`, an
  assignment, a `return`: each is a copy the variable owns); a literal passed as an
  argument, compared, or matched against is the static one and allocates nothing;
- a map entry written with `m[k] = v`, `try`, `spawn`, `lock`, `region`;
- every builtin but those known to allocate nothing: bit and integer arithmetic,
  `peek`/`poke` and `vpeek`/`vpoke`, `atomic_*`, `len`, lookups (`get`, `has`,
  `v[i]`), `ord`, `chr`.

Code the checker cannot see into counts as allocating too: calling a closure, a
method it cannot resolve, inline assembly that calls or jumps outside itself (a jump
to one of its own numeric labels, `jnz 2f`, is fine), and a function without a Nori
body, such as an `extern fn` or a `naked fn`. Mark such a declaration `@irq` as well, and
the checker takes that as its author's word that it allocates nothing:

```nori,excerpt
@irq extern fn nori_inb(Int) -> Int      // the floor's `in` instruction
```

The error names the whole chain from the handler to the allocation:

```text
◆ [1] `@irq` function `timer_tick` can allocate: it calls `sched_tick`, which calls `log_tick`, which concatenates strings
```

Freeing is not checked; a handler has nothing of its own to free unless it
allocated it first.

## Locks: `@takes`, `@needs`, `@sleeps` {#locks}

A kernel runs most of itself under locks, and the rules about what a lock
holder may do are written down (which lock may be taken while holding which,
which may sleep, which may not) and then kept by hand, one call site at a
review. Nothing about a function that takes `PROC_LOCK` looks wrong in
isolation; the wrongness is somewhere in its callers. A spinlock taken twice
is not an exception a test catches: the task spins with interrupts off, and
the machine stops minutes later, somewhere else entirely.

Four annotations say the rules where the checker can hold code to them:

```nori,excerpt
@takes(PROC_LOCK)     // takes it itself: it, and everything it calls, holds it
fn proc_ref(p: Int) { … }

@needs(PROC_LOCK)     // the caller must already hold it
fn sig_send_proc(p: Int, sig: Int) { … }

@sleeps               // may block: an error anywhere under a spinlock
fn msleep(us: Int) -> Int { … }

@nospin               // verified never to block (the complement of @sleeps)
fn submit(t: Int) { … }
```

and a `lockorder` block says which locks exist, the order they may be taken
in, and which of them spin:

```nori,excerpt
lockorder {
    sleeping: MOUNT_LOCK, JOURNAL_GATE, RENAME_MUTEX, DIR_LOCK nest
    big: BKL nest
    spinning: PS2_LOCK, TTY_LOCK nest, PROC_LOCK, FUTEX_LOCK, WAIT_LOCK
}
```

The order is the requirement: a lock may only be taken while holding locks
that come **before** it in the declared order; the rule that prevents
deadlock, not merely self-deadlock. A lock within one line may be taken
freely above a lock on a later line, never the reverse. The groups say how
each lock behaves:

- `sleeping:` waiting for one sleeps, and the holder of the big kernel lock
  gives it up while it waits (the scheduler does it, in the switch out). A
  `@takes` of a sleeping class blocks, so the checker treats the function as
  `@sleeps` as well.
- `big:` the big kernel lock. It is unordered against the sleeping locks, by
  declaration and in the kernel's prose: a task waiting for a sleeping lock
  lets it go meanwhile, and a page fault taken under a filesystem's mutex
  takes the big kernel lock for the fault alone.
- `spinning:` interrupts off while held, and never anything that sleeps
  under one: the wakeup the sleeper waits for would run on the very CPU that
  is spinning.

`nest` after a class says one instance of it may lawfully be held while
another is taken; one lock per terminal, one per mounted filesystem. The
checker cannot tell two terminals apart (see the limit, below), so without
`nest` it refuses what only looks like re-entry; with it, it stays silent and
the per-instance discipline stays with the reader.

`@nospin` is the "does not block" promise, the complement of `@sleeps` (they
annotate opposites on the same axis; a function or a type cannot carry both).
On a Nori body it is **verified**, exactly as `@irq` is: the checker proves the
function's whole call tree never waits (no `@sleeps` callee, no `await`, no
`lock`, no join) and refuses the function if it cannot (or if the function
carries `@sleeps`). On an `extern fn` or a `naked fn` it is the author's word
that the body never blocks, which is the hook a driver's transmit or timer
call-back uses when it may *allocate* (so `@irq` would be the wrong word) but
never sleeps: with the queue lock held by its caller, such a call-back is
stored into a `@nospin`-typed slot, verified at the store, and then trusted —
and callable under a spinlock.

The rules, as a call-graph property:

- a `@takes(L)` or `@needs(L)` function holds L for its whole body and
  **everything it calls, however deep**: the held state flows down through
  functions that carry no annotation at all, the same walk `@irq` does over
  allocations;
- calling a `@needs(L)` function with L not held is an error;
- calling a `@takes(L)` function while L is already held is an error; the
  self-deadlock, the spinlock taken twice;
- calling a `@sleeps` function while holding a lock of the `spinning` group is
  an error, and so is any wait the checker can see (`await`, `parallel
  foreach`, the runtime's `lock`), and so is a site it cannot see into: an
  `extern fn` or a `naked fn` without annotations, a closure called through a
  value, a method it cannot resolve, inline assembly that calls or jumps out;
- taking L while holding M requires M to come before L in the declared order.

The error names the whole chain from the annotated function to the call that
breaks the rule:

```text
◆ [1] `PROC_LOCK` is taken while it is held already: `proc_link` calls `proc_ref` — a lock taken twice spins for ever
  ▸ fix · take the lock once where the work begins, and give the work to a helper that assumes it (the `_l` helpers — `proc_ref_l`, `proc_find_l` — are the shape), or let it go before this call
  ▸ read · ref::unsafe_and_ffi::locks
```

**What a lock is, here: a class, not an instance.** In the kernel a lock is
`fn PROC_LOCK() -> Int`, an address; the annotation names the identifier, and
the checker treats it as an opaque token; it never evaluates it, and it
cannot tell one terminal's lock from another's. That keeps the check a
call-graph walk, and it is enough for the order rule, which is about classes.
It cannot prove two instances of one class never nest: `nest` says the kernel
allows it, and the per-instance discipline (a directory before any directory
below it; a run queue only ever tried for, never waited on; two directory
locks ordered by inode number) is not checked.

**What the checker does not do, on purpose: infer held state from bodies.**
The kernel takes locks as `let fl = lock_irqsave(PROC_LOCK())` …
`unlock_irqrestore(PROC_LOCK(), fl)`: pairs a flow-sensitive pass would have
to track. Annotation-only cannot be wrong in the silent direction: a missing
annotation is a missing check, never a false proof. The discipline it asks is
the one the `_l` helpers already follow: an annotation says the lock is held
across calls. A function that takes a lock, calls nothing under it, and lets
it go needs no annotation; a function that lets a lock go mid-body and calls
on needs the released part factored into a helper, because the checker holds
the annotation to the whole body.

**Adoption is incremental, because unannotated functions are transparent.**
They contribute nothing to the held state and are still walked, so annotating
the few functions that take a lock (the `lock_irqsave` sites) checks their
whole subtree at once; one file at a time, no flag day. A `@needs` called
from a chain that holds nothing is an error as soon as any annotated root is
above it; code no annotated root reaches is simply not checked yet. An
annotation on an `extern fn` or a `naked fn` is its author's word about a body
the checker cannot see, and that word is then held at every call site. One
word already in the vocabulary clears a site: `@irq`; a declaration marked
`@irq` that the checker cannot see into is its author's word that it
allocates nothing, and (what may run from an interrupt never blocks) that it
does not sleep either.

Zero cost, by construction and by test: the annotations and the declaration
are read by the parser, stored under keys the code generator never reads, and
emit nothing; a program and the same program with the annotations stripped
build byte-identical binaries.

## Function references: the type carries the effects {#function_references}

A kernel that loads drivers cannot name its interrupt handlers: each driver
fills a table the dispatch walks. A *function reference* is that table's slot —
a non-capturing reference to a named top-level function whose **type is its
signature and its effects**. The value is the code's address: no closure
object, no allocation, an ordinary pointer stored in a global, a struct field,
a `Vec`, or a cstruct field, and calling it is one indirect call with the
ordinary calling convention and as many arguments as the direct call allows.

A function reference is a type, spelled like a declaration with the name
removed:

```nori,excerpt
global irq_handlers: Vec<@irq fn(Int) -> ()>   // a table of interrupt handlers

@irq
fn virtio_blk_irq(v: Int) { … }

fn register() {
    irq_handlers.push(virtio_blk_irq)          // naming the function produces the reference
}

@irq
fn irq_dispatch(v: Int) {
    // calling through a slot: the checker uses the type's declared effects
    irq_handlers[v](v)
}
```

The type carries the same annotations a declaration does: `@irq` (and its
`@nospin` sibling), `@sleeps`, `@takes(L)`, and `@needs(L)`. Each annotates
exactly the function referenced:

```nori,excerpt
global probe: @sleeps fn(Int) -> Int            // a probe may sleep
global setter: @nospin @takes(WAIT_LOCK) fn(Int) -> Int  // never blocks; takes WAIT_LOCK when called
global plain: fn(Int) -> Int                    // no annotations: unknown code, the conservative case
```

`@nospin` is the "verified never to block" mark, the complement of `@sleeps`
(they are opposites on the same axis: a type or declaration cannot be both).
It exists for the call backs a kernel dispatches to with a queue lock held: a
driver's *submit*, a network *transmit*, a *timer* call-back may allocate, but
may never sleep, and may take only the locks its type names. `@irq` implies
`@nospin` — what may run from an interrupt never blocks — so an `@irq` value
fits a `@nospin` slot.

**Assigning** a function (or another reference) to a slot checks it against
the slot's type in two ways. The *signature* must match exactly; a reference
is called through the slot's signature, and an indirect call carries no types
of its own, so a `fn(Str) -> Int` stored where `fn(Int) -> Int` is declared
would read an `Int` as a string handle. And the *effects* must fit; what is
compared is the stored function's **computed call-graph summary**, not its
annotation alone; the fit runs over the whole tree, the way the checkers
themselves do:

- `@irq` and `@sleeps` are one-sided: a proven-safe function may occupy a
  weaker slot; but a `wrap` function that merely *calls* a `@sleeps` helper
  still sleeps (its call tree reaches it), and is refused from any
  not-sleeping slot; a callee's `@takes(TTY_LOCK)` under a function that
  carries no annotation still refuses a slot that does not name it.
- `@takes`/`@needs` are the slot's word to its callers, against the computed
  lock set: the union of `@takes` of the whole tree, and the locks the tree
  requires of its caller (what it needs minus what it takes itself). Every
  caller is checked against the slot's sets, so the function's must lie
  within them: one that takes or needs a lock the slot does not name is
  refused (a caller holding it would deadlock, or was never asked to hold it),
  and one that takes or needs *fewer* fits; its callers were checked against
  more, which is stricter. That is how one table holds drivers that take
  different locks. For the subset to be sound a slot's type may not name one
  lock in both `@needs` and `@takes`.
- a `@nospin` slot demands proof: the stored function's tree must sleep
  nowhere, and take only locks the slot names. A plain (unknown) value cannot
  be stored into one.

A mismatch is refused with the function, the slot and the effect named
(a four-driver table like `irq_dispatch`'s meets this: the
handlers' wake path takes `WAIT_LOCK` through `obj_signal`, so the table's
type has to name it as `@irq @takes(WAIT_LOCK) fn(Int) -> ()`):

```text
◆ [1] cannot store `nvme_irq` (`@irq fn(Int) -> ()`) into `@irq fn(Int) -> ()`: the function takes `WAIT_LOCK`, but the slot's type does not name it
```

**Calling** through a reference feeds the two checkers exactly as a direct
call's annotations do: an `@irq` handler may call an `@irq`-typed slot (the
handler above checks out); calling the *unannotated* one from a handler is
refused as code the check cannot see; a `@sleeps` slot called under a
spinlock is the same error a direct `@sleeps` call is. Under a *spinlock*,
only a reference type that **proves** its callee never blocks may be called —
an `@irq` or `@nospin` type, whose stored function the checker verified. A
`@takes`/`@needs`-only type names its locks but promises nothing about
blocking, so under a spinlock it is an unseen site (fine everywhere else);
the lock order is still checked against the locks an `@irq @takes(…)` /
`@nospin @takes(…)` type names.

A reference with no annotations is just unknown code: callable anywhere
ordinary unknown code is (outside handlers and locks), refused exactly where
unknown code is refused.

**The empty slot** is the null reference, spelled the literal `0`: store it
to leave a slot empty, and compare against it to ask whether a slot is filled
(`slot == 0` / `slot != 0`, and `a == b` between two references, are identity
on addresses):

```nori,excerpt
irq_handlers.push(0)          // nothing registered here — the dispatch skips it
@irq
fn dispatch(v: Int) {
    var i = 0
    while i < irq_handlers.len() {
        let h = irq_handlers[i]
        if !(h == 0) { h(v) }                         // call only a filled slot
        i = i + 1
    }
}
```

`0` is the **only** integer a slot accepts. No other integer, variable or
expression of type `Int` converts to a reference: a slot is a code address,
and an arbitrary value would point a call at code the effects-checkers never
saw; the hole that let any module put any address into an `@irq` slot and
call it, voiding every guarantee the slot's type exists to make. (A reference
can still be converted *to* its address with `to_int`, where an integer is
what you want.) And a slot nothing ever assigns is provably still its initial
`0` at every call, so calling it — a jump to address 0, a page fault — is
refused at compile time; deeper "did this path test it" reasoning is a
follow-up, and the guard above (`if !(h == 0)`) is the runtime shape.

References are checked by the same passes as everything else: a `@takes`
assigned into a slot that names a different lock, or called in the wrong
order, is refused; a function with `inout`/`sink`/`set` parameters cannot be
referenced (an indirect call passes plain values; the checker says so at the
reference site); a generic function cannot be referenced yet; a generic has
no single code body for the reference to point at.

## Auditing unsafe

Because `unsafe`/`extern`/`cstruct`/`asm` are explicit, the whole surface is
auditable. `roll unsafe` (and `noric --unsafe-report`) produce a whole-tree report
of every unsafe construct with its `file:line`, so you can review exactly where a
program leaves the safe subset. See [Tooling](20_tooling.md).

---

Next: [Conditional Compilation](18_conditional_compilation.md).
