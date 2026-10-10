# 15. The Memory Model

Nori has **no garbage collector**, **no reference counting**, and **no lifetimes**.
Every heap value has exactly one **owner** — a place the compiler can name — and
the compiler frees it when that owner dies. The whole model is decided at compile
time; the running program only ever executes the frees the compiler wrote. A
program that compiles neither leaks nor frees twice. This chapter describes the
model you program against.

## Handles, owners and views {#handles}

A `Str`, `Vec`, `Map`, `Pool`, fixed array `[T]`, plain `struct`, enum value or
closure is a **handle**: an `i64` naming a heap record. Handles are cheap to pass
around, so Nori distinguishes two ways of holding one:

- An **owner** is the place the value belongs to. A `let`/`var` bound from an
  allocation (`vec()`, a literal, a call that builds something), a field of an
  owned record, an element of an owned container. When an owner goes out of scope
  its value is freed, **deep**: a `Vec<Str>` frees its strings, a struct frees its
  fields, a map frees its values.
- A **view** is a second name for a value somebody else owns. A bare parameter,
  `let b = a`, `a.f`, `v[i]`, a global, the result of a `-> view` function. A view
  reads and passes the value along; it never frees it and it can never be
  **stored** anywhere that outlives the statement.

```nori
fn total(xs: Vec<Int>) -> Int {      // xs is a view: total reads the caller's vector
    var s = 0
    foreach x in xs { s = s + x }
    return s
}
fn main() -> Int {
    var v: Vec<Int> = vec()           // v owns the vector
    v.push(1)  v.push(2)
    let w = v                         // w is a view of v's vector
    printl(to_str(total(w)))
    return 0                          // v dies here and the vector is freed — once
}
```

A view cannot be pushed, put, placed in a field, assigned to a longer-lived
variable, captured by a closure, or returned as an owned value: what a container
or record holds must be owned by it. Where you want that, say how:

- **`sink x`** hands x's value over. The container now owns it and `x` is dead
  after the statement. `sink r.f` / `sink v[i]` / `sink g` (a global) move a
  value *out* of a place and leave a fresh empty behind (`""`, `vec()`, `map()`,
  `0`). A move costs nothing.
- **`copy x`** stores a **deep copy**. Both x and the copy stay valid, each with
  its own owner.
- **`-> view T`** on a function declares that its result is borrowed from a
  parameter (an accessor returning `self.items`); the caller gets a view.
- **`f: view T`** on a struct field (or `view struct` on the whole record) lets a
  record hold a borrow. Such a record is freed shallow and may not outlive what it
  views; the compiler checks that. A name already bound as a view (a bare
  parameter) may be stored there: the field is declared to take a borrow, and it
  is the record's own lifetime that is checked instead.

```nori,error
var names: Vec<Str> = vec()
let s = "abc" + suffix
names.push(s)              // ERROR: `s` is an owner; the push would take it. Say which:
names.push(sink s)         // move: s is dead from here
names.push(copy s)         // copy: s stays yours
```

The rule to remember: **a bare name is a view; stores say `sink` or `copy`.** In
practice most stores are of values built right there (`v.push("x" + y)`,
`m.put(k, parse(t))`), which are owned by nobody yet and simply move in.

## Mutation and sharing

A view reads but cannot write, so a mutation through it is a compile error rather
than a surprise. Say `inout` and the sharing is explicit:

```nori,error
var a: Vec<Int> = vec()
var b = inout a    // b names the same vector, and says so
b.push(2)
var c = a          // a bare name is a view
c.push(3)          // ERROR: `push` writes to `c`, which is a view
```

The same applies to parameters: a bare parameter is a view, and a callee that
means to change the caller's object declares `inout`. `Str` values are immutable,
so `s2 = s2 + "!"` rebinds `s2` to a new string and frees the old one.

Shared mutable state is prevented by checking, not by the absence of sharing. A
value that is mutable and passed by reference cannot cross into a concurrent task
at all — the compiler rejects it — so concurrent code communicates by moving values
(`spawn` arguments the checker proves share-safe, channels, `Shared<T>`), and data
races are ruled out at compile time (see [Concurrency](16_concurrency.md)).

## Where values are freed {#auto_free}

You never call `free`. Every owner is freed at the end of its scope, and a
variable that is rebound frees the value it held before. A value moved out with
`sink` is not freed by its old owner; the flag that says so is decided at compile
time where possible and carried at run time only when a move happens on one path
of an `if` and not the other. Temporaries (`f(g(x))`) die at the end of their
statement.

A `match sink r { … }` tears a value down: each arm owns its payload and the shell
is freed. A plain `match r { … }` gives the arms views of the payloads.

A **block expression** (`{ var s = build()  s }`, or the body of `within`, `race`
and `select` arms) runs its statements and hands its tail value out; the block's
own locals die with it, so the tail moves a local out rather than viewing it. A
`[a, b, c]` literal takes its elements the way `push` would. An `inout` binding
names a place that outlives the statement: `var f = inout load(path).font` is
refused, because the record `load` built dies at the end of that line; bind it
to a name first.

Keys are **copied into** a map, so `m.put(k, v)` and `s.add(k)` take nothing from
`k`; only the value moves.

A **closure** captures by taking ownership: a name its body mentions is moved into
the closure, and freed with it. To keep using the value in the enclosing scope,
capture a `copy`, or pass it as an argument instead. The exception is a closure
written straight into a call whose parameter does not keep it (`v.count_where(|u|
u.name == who)`): that closure only *borrows* its captures and dies with the
statement, so `who` stays yours. A callee that stores a closure declares the
parameter `sink`, and then the closure it is handed owns what it captured.

`m.get(k) else ""` (or any literal default) is a view of the map's value, the same
as `m.get(k)` itself: it dies when the entry is replaced and cannot be returned
or stored as owned. A default that is built on the spot (`else "a" + b`) makes the
whole expression owned, because on a miss there is nothing else to own it.

`if c { a } else { b }` where both arms name a **place** (a name, a field, an
element) builds nothing: it picks one of two places that were already there. In a
borrowed position (a `view` field) it is therefore a borrow of both, exactly as a
bare name is. In an owning position both arms hand their values over, since which
one arrived is a run-time fact and whoever receives it has to own it either way.

A **view taken before a call** stays valid only if the call leaves what it views in
place. The compiler summarizes every function by the parts of its `inout`
parameters it replaces or frees, so `let t = self.tok  self.bump()  t.text` is
refused when `bump` reassigns `self.tok` and accepted when it only counts, or
only touches other fields of `self`. A view of a container element (`let s =
m.get(k)`) dies with a store into that container; a view handed out by a
`-> view` function dies with any write under what that function was given, since
which part it borrowed is the callee's business. A name that is a view on one
path of an `if` and an owner on the other is held to the view's rule.

**Runtime handles** (`Task<T>`, `Channel<T>`, `Mutex`, `Shared<T>`) are the
scheduler's, not values one task owns: any task may keep naming them, a spawned
block may capture them, and nothing frees them. A value sent on a channel becomes
the channel's, and a value received (`foreach v in ch`) is the receiver's own.

### Small structs stay in registers

A struct whose fields are all scalars (`V { x, y, z }`) is passed and returned as
its fields wherever a call can supply them directly, so vector math costs no
allocation at all. The frees above still cover the record wherever one is built.

### Field and element reads are inline

A struct field read and a vector element read compile to an address computation
and a load against the runtime's arena, not a call. The arena base is a global the
runtime exports, so this holds across separately compiled units too.

### Frame promotion

If the compiler can prove a fixed array dies at the end of its scope, it puts the
array in the **stack frame** instead of the heap, and the allocation disappears
entirely. A `fill(N, v)` is promoted when `N` is a literal, the array is at most 16
KB, no loop encloses the allocation, and the function cannot return an array.

### A table is paged small until it is big

Each table is reserved whole and touched sparsely, which is the shape transparent
huge pages are worst for: the first record of a table would fault in two megabytes,
so a program that allocates one value of each kind pays four of them before doing
any work. Reservations therefore ask the kernel for ordinary pages, and a table
asks for huge ones again once it passes 262,144 records, dense enough that the
translation buffer wants them and the rounding is noise. A small program therefore
starts with a small resident size.

## Regions

A `region { … }` block is a scoped arena: everything allocated inside it is freed
together when the block ends.

```nori
region {
    var scratch: Vec<Int> = vec()
    foreach i in 0..1000 { scratch.push(i * i) }
    printl(to_str(scratch.len()))
}   // scratch (and everything allocated in here) is freed at this brace
```

## Ownership on parameters

Parameters are views by default. Three annotations change how a value crosses a
call boundary:

- **`inout`**: pass by reference; assignments in the callee are written back to
  the caller (see [Functions](08_functions.md)).
- **`sink`**: the callee takes ownership of the argument. The caller's name is
  dead after the call; pass `copy x` to keep it. A function that stores what it is
  given (`fn add(inout self, sink item: T)`) declares it this way.
- **`copy`**: the callee gets its own deep copy.

## Generics

A generic function is compiled once per concrete type it is called with, so a
`Vec<T>` inside it frees its elements exactly as a `Vec<Str>` would.

## Safety guarantees {#safety}

- **No leaks**: every heap value has one owner and is freed when that owner dies.
- **No dangling, no double free**: a view can never be stored or outlive what it
  views, and a moved value has one owner at a time.
- **Bounds checking**: indexing a `Vec`, array, or `Str` out of range traps.
  Provably-in-range accesses have the check erased.
- **Exclusivity**: a mutation is never observed through an unexpected second
  reference; this is checked and has been proven sound.

To step outside the model deliberately (raw memory, FFI), you use an `unsafe`
block; see [Unsafe and FFI](17_unsafe_and_ffi.md). A raw address taken there does
not extend anything's life: the frame still frees its owners at its end, so a
value whose address has to outlive the frame must be handed to something that
lives longer. `noric --xray` shows each value's lifetime for exactly this reason.

---

Next: [Concurrency](16_concurrency.md).
