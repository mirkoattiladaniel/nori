# 4. Variables and Bindings

## `let` and `var` {#let_and_var}

A binding introduces a name for a value. `let` is immutable; `var` is mutable.

```nori
let limit = 100        // immutable — cannot be reassigned
var count = 0          // mutable
count = count + 1      // ok
// limit = 50          // ERROR: cannot assign to a `let` binding
```

Assignment (`=`) reassigns an existing `var`; it is a statement, not an
expression, so `a = b = c` is not allowed.

### What a binding means

`let` and `var` say whether the **name** can be reassigned. A second question is
what the name gives you access to, and Nori makes you answer it whenever you bind
one name from another. A bare binding is a **view**: read it, don't write
through it:

```nori
struct P { x: Int, y: Int }

fn main() -> Int {
    var a = P { x: 1, y: 2 }

    var look = a       // view (the default): read it
    printl(look.x)     // 1

    var edit = inout a // the same object, and I may change it
    edit.x = 9
    printl(a.x)        // 9

    var own = copy a   // my own value; changing it never touches `a`
    own.x = 7
    printl(a.x + " " + own.x)   // 9 7
    return 0
}
```

Writing through the view is the part the compiler refuses:

```nori,error
var look = a
look.x = 9         // ERROR: `look` is a view — it can't be written through
```

The fourth word is `sink`, which **takes** the value: the source name is dead
until you give it a new one.

```nori
var v: Vec<Int> = vec()
var w = sink v     // w owns it now
// v.len()         // ERROR: `v` was moved by `sink`
v = vec()          // rebinding revives the name
```

Passing a name to a `sink` **parameter** moves it the same way; the keyword at
the call boundary means exactly what it means at a binding:

```nori
fn eat(sink s: Str) -> Str { return s }

var a = "hello" + ""
let b = eat(a)
// printl(a)       // ERROR: `a` was moved by `sink`

var k = "keep" + ""
let c = eat(copy k)    // `copy` at the call site keeps the caller's binding alive
printl(k + c)          // ...so `k` is still usable here
```

This applies only where a name could alias something: `Vec`, `Map`, `Str` and
structs. A scalar has nothing to write through, so `var b = a` on one is just a
second name for the same value. And a binding of a
*fresh* value (`var v: Vec<Int> = vec()`) owns what it made, so it stays writable
without any keyword. See [The Memory Model](15_memory_model.md).

## Type annotations

The type of a binding is inferred from its initializer. You may annotate it
explicitly with `: Type`, which is required when the initializer alone doesn't
pin the type (for example, an empty container):

```nori
let n = 42                 // inferred Int
let pi: Float = 3.14159    // annotated
var xs: Vec<Int> = vec()   // annotation needed — vec() is empty
var m: Map<Str, Str> = map()    // keys Str, values Str
```

Annotations also select a **storage width** for the sized integer types
(`I8`…`U64`), which matters in `fill` arrays and `cstruct` fields:

```nori
var bytes: [U8] = fill(16, 0)   // 16 one-byte slots
```

### Stores are type-checked

An annotated declaration and an assignment must both agree with the name's
type, in the two directions where the value could not possibly be reinterpreted
safely: a `Str` is a heap string handle, and a container is an arena handle, so
neither can be given a plain number.

```nori,error
var c: Str = 5             // ERROR: cannot initialise `c`, which is Str, with Int
var c = " "
c = 5                      // ERROR: cannot assign Int to `c`, which is Str
c = s.at(0)                // ERROR: cannot assign Byte to `c`, which is Str

var v: Vec<Int> = vec()
v = 7                      // ERROR: cannot assign Int to `v`, which is Vec<Int>
```

The reverse stays legal: `Int` and `Char` mix by design, `Int` widens to
`Float`, and a handle stored in an `Int`-typed name is how a ported C library
carries a table.

## Scope and the no-shadowing rule {#no_shadowing}

Bindings are visible from their declaration to the end of the enclosing function.
Nori does **not** have block-shadowing: every name maps to one function-level slot,
so a `var`/`let` in a nested block would reuse the slot of an enclosing binding of
the same name rather than introducing a separate variable.

Because that would make the code mean something other than what it reads as,
**redeclaring a name that is already live is a build error**:

```nori,error
fn main() -> Int {
    var x = 1
    if true {
        var x = 99       // ERROR: `x` is already declared in an enclosing scope
        x = x + 1
    }
    printl(to_str(x))
    return 0
}
```

```text
error: `x` is already declared in an enclosing scope
  note: Nori has no block shadowing — this reuses the enclosing `x`, so assigning
        to it changes that one
  fix:  rename this one (e.g. `x2`), or assign to the existing `x` if that is what
        you meant
```

What is still allowed:

- **Assigning** to the enclosing binding: `if true { x = 99 }` is fine, and is
  what you want when you mean to change it.
- **Sibling blocks** reusing a name **at the same type**. Two `if` blocks at the
  same depth may each declare `t`, because neither is inside the other and the
  shared slot then means exactly what it looks like. Redeclaring one at a type
  the first cannot be read as is an error wherever it stands, even when the first
  block has already closed:

  ```nori
  while i < n { var shown = i + 1 }   // an Int
  var shown = label                   // ERROR: `shown` is already declared in
                                      // this function, as Int
  ```

  There is no enclosing scope to blame here — the loop body ended — but the slot
  is still shared, so the `Str` would be read as that `Int`.
- **Different functions** reusing a name: slots are per-function.
- **Loop variables.** `foreach i in …` binds `i` for the loop, so two sibling loops
  may both use `i`, and an inner loop may reuse an outer one's name. What a loop
  variable may not do is land on a slot the function reads at another type; a
  binding shares the one slot per name exactly as a `var` does:

  ```nori,error
  var nb = "abc"
  foreach nb in 0..3 {        // ERROR: `nb` is already declared in this function,
      printl(to_str(nb))      //        as Str
  }
  ```

  The same holds for a **parameter** (`fn f(nb: Int)` then `var nb = "text"`), for
  `lock m as nb`, and for an enum payload bound by `match`.
- **Shadowing a module-level `global`.** A `let`/`var` may reuse the name of a
  global; inside that function the name means the local, and the global keeps its
  value. (Globals live in their own storage, so there is no shared slot, unlike a
  parameter, which a local may still not shadow.) This is what keeps a program's
  own `global n` from being an error inside a library that happens to use `n` as a
  local.

> **Note.** This is the one place Nori's scoping differs from most languages: a
> nested block does not open a fresh namespace, so an inner name must be a *new*
> name. Pick a distinct one (`x2`, `inner`, something meaningful) when you mean a
> distinct value.

## Destructuring

A tuple binding unpacks several values at once. It works with `let` and `var`:

```nori
fn divmod(a: Int, b: Int) -> (Int, Int) { return (a / b, a % b) }

fn main() -> Int {
    let (q, r) = divmod(17, 5)              // q = 3, r = 2
    printl(to_str(q) + " " + to_str(r))     // 3 2
    return 0
}
```

The number of names must match the tuple's arity.

## Globals

Top-level constants are declared with `global`, visible to the whole module (and,
if `pub`, to importers). The **scalar** types take a constant initializer
directly: `Int`, `Bool`, `Float` and `F32`:

```nori
global MAX_RETRIES: Int = 5
pub global DEFAULT_PORT: Int = 8080   // exported to modules that import this one
global VERBOSE: Bool = false
global EPSILON: Float = 1e-9
global SCALE: F32 = 0.5
```

An `Int` global **must** have one; the others may. The value goes into the data
segment, so nothing runs at startup to produce it.

An `Int` initializer is a **constant integer expression**, computed by the
compiler: integer literals (decimal, `0x` hex, `0b` binary), unary `-` and `~`,
parentheses, and `* / % + - << >> & ^ |` with the precedence and meaning they
have in a function body (`/` and `%` truncate, `>>` is logical). A hex or binary
literal is a 64-bit pattern, so `0xFFFFFFFFFFFFFFFF` is `-1`.

```nori
global PAGE: Int = 1 << 12
global FLAGS: Int = 0x8000 | 3
global NONE: Int = 0 - 1
```

What the program could not compute is refused rather than wrapped: an overflow,
a division or remainder by zero, a shift count outside `0..63`, and anything that
is not a constant (a name, a call, a comparison):

```
◆ [1] `9223372036854775807 + 1` overflows a 64-bit `Int` in a global's initializer
  ▸ fix · the value is computed by the compiler, so it must fit in
          -9223372036854775808..9223372036854775807
```

A float initializer is a number literal, optionally negative, and an integer
literal is accepted where a float is expected (`= 3` means `3.0`). A `Bool` one
is `true` or `false`.

A global of any other type is declared **without** an initializer; it starts
zeroed/empty and is populated at runtime (this is how a module holds a shared
lookup table, e.g. `global cache: Map<Str, Int>`), again with the usual value
semantics:

```nori
global cache: Map<Str, Int>  // empty at startup; filled by the code that uses it
```

That is not a restriction about globals but about **constants**: a `Vec`, a
`Str` or a `Map` is produced by a call at runtime, so there is no constant form
of one to put in a data segment, and zero-as-null is the correct initial value.
Writing one is refused by name:

```
◆ [1] a `Vec<Int>` global takes no initializer, and `v` is one
  ▸ fix · write `global v: Vec<Int>` and assign it before use — a Vec<Int> is made
          at runtime, so there is nothing to put in the data segment
```

A `[T]` **array** global is a fixed-size block. `= N` gives its element count,
zero-filled; `= [a, b, c]` gives its contents, and the size follows from the
list:

```nori
global scratch: [I32] = 4096                          // 4096 zeroed elements
global K: [Int] = [0x428a2f98, 0x71374491, 0xb5c0fbcf]   // a constant table
global T: [Int] = [1 << 12, 0x8000 | 3, 0 - 1]          // 4096, 32771, -1
```

A `[T]` **can** be returned from a function, under one rule; see
[Fixed arrays](09_collections.md#returning-an-array).

The list form is a **data-segment initializer**, not a program: the count and
each element are constant integer expressions, the same forms as an `Int`
global's initializer, and anything else is refused. Elements are separated by
commas (a trailing one is allowed), and an element ends where its expression
does: `[5, 3 - 1, 7]` is three elements, `5 2 7`, however it is spaced. The array itself stays mutable; what is fixed is
where the initial bytes come from. Nothing runs at startup to build it, which is
the point: without this a lookup table has to be rebuilt on every call, or
lazily filled behind a check that also races between tasks. std/crypto's
SHA-256 round constants were 64 `Vec` pushes per hash before this existed.

For a **`Str` constant** (the one scalar-looking type that is really a handle),
use a nullary function, which reads like a constant at the call site:

```nori
fn version() -> Str { return "1.0" }
```

`global` is the only plain top-level binding form; there is no top-level
`let`/`var`, and executable statements live only inside functions.

### `threadlocal global`, one copy per thread {#threadlocal}

A `threadlocal global` is a global with **one instance per thread**, rather than
one for the whole program:

```nori
threadlocal global g_depth: Int = 0

fn enter() { g_depth = g_depth + 1 }
```

It is a **scalar `Int` only**, with a constant integer initializer: per-thread
state is one i64 cell, and a `Str`, `Vec` or `Map` is refused by name:

```
◆ [1] `threadlocal global` supports only a scalar `Int`
  ▸ fix · per-thread state is an i64 cell — e.g. `threadlocal global g_cur: Int = 0`
```

That restriction is what makes it useful where an ordinary global is refused.
`spawn` is one thread per task, so two tasks never see the same instance and
there is nothing to race on; a `threadlocal global` may be read and written
from inside a task, while an ordinary mutable global may not (see
[Concurrency](16_concurrency.md#race_safety)).

**In a freestanding image the thread is a CPU.** The native back end reaches a
thread-local through `%gs`, where a kernel keeps each CPU's block, and the linker
emits one TLS template that the kernel copies below each CPU's thread pointer —
so `threadlocal global` is how a kernel declares per-CPU data without computing
offsets by hand. Only the native back end can do this: LLVM lowers every x86-64
thread-local through `%fs`, which a kernel leaves to the programs it runs, so a
freestanding program's own `threadlocal` under `--llvm` is refused with a message
saying to drop the flag.

### Unused globals

A global nothing in the program ever names produces a warning, not an error:

```
▲ [1] unused global `scratch`
  src/main.nori · 3:1

    3 ▏ global scratch: Vec<Int>
      ▏ ━━━━━━ declared but never read or written

  ▸ remove it, or rename it `_scratch` to keep it on purpose
```

Prefix the name with `_` to keep one you are not using yet. The rules match
[unused functions](08_functions.md#unused-functions): `pub` globals, globals a
module brought in under an alias, and library/`--runtime` builds are all exempt,
with two additions of its own:

- a `durable var` is never reported. The image machinery saves and reloads every
  durable whether or not your code mentions it, so "nothing names it" does not
  mean nothing reads it;
- a program containing an inline `asm` block gets no global warnings at all. Asm
  names symbols inside a string, and a string is not an identifier the compiler
  can count, so a global an asm block reads would otherwise look untouched.

`frozen global` and `threadlocal global` are reported like any other.

## Persistent globals: `durable var`

A `durable var` is a global whose value **survives across program runs**. It is
declared like a global, but the runtime automatically loads it from disk when the
program starts and saves it again when the program exits; no serialization code,
no database, no file handling:

```nori
struct User { name: Str  age: Int }

durable var launch_count: Int = 0        // a scalar takes a constant initializer
durable var users: Vec<User>             // a container starts empty on first run

fn main() {
  launch_count = launch_count + 1        // just mutate it like any global…
  users.push(User { name: "ada", age: 36 })
  printl(launch_count)                   // 1, then 2, then 3… on successive runs
}
```

Each `durable var NAME` is backed by a file `NAME.ndb` in the working directory.
On entry the value is loaded (containers initialize to empty when the file does
not yet exist); on every exit path it is written back. The write is **atomic and
crash-safe**; the image is staged to a temporary file, flushed, and renamed over
the target, and it carries a CRC, so a crash mid-write leaves the previous image
fully intact rather than a torn file.

The value's type must be serializable: `Int`, `Float`, `Bool`, `Char`, `Str`, and
any `Vec`, `Map`, `struct`, or `enum` built from those (nested and recursively
shared/cyclic graphs are handled). A type that cannot persist (a closure or an
FFI handle) is rejected at build time.

To checkpoint mid-run (rather than only at exit), or to persist a plain `global`,
call `commit` and `restore` explicitly:

```nori
durable var scores: Map<Str, Int>

fn main() {
  restore(scores)          // load (1-arg: path from the `durable var` registry)
  scores.put("alice", 10)
  commit(scores)           // write now, without waiting for exit
}
```

```nori
global g: Int = 0

fn main() {
  restore(g, "g.ndb")      // 2-arg: an explicit path works on a plain global too
  g = g + 1
  commit(g, "g.ndb")
}
```

### Sharing a durable across tasks

A `durable var` is a global, so the usual concurrency rule applies: a task that touches a global is a
build error, because that would be a data race. A durable is *shared state you actually want to share*,
so it gets the same escape hatch a `mutex` has: `lock` it:

```nori
durable var hits: Int = 0

fn handle() -> Int {
    lock hits as h { h = h + 1 }        // exclusive; other tasks wait
    return 0
}
```

Inside the block, `x` is the durable's current value and carries its type; so a
`durable var users: Table<User>` gives you a `Table<User>` to call methods on. The value is written
back and the lock released when the block ends, including on an early `return`. Reaching the durable
from a task *without* the lock is still rejected.

The auto-commit at exit sees whatever the last lock wrote, so a concurrent program persists the same
value a sequential one would.

### Sharing a durable across processes

A durable is shared by everything that opens the same file, other *processes* included, not just the
tasks inside one program. `lock` covers that too:

- entering `lock <durable> as x { … }` takes an **exclusive OS lock** on the image (a `<name>.ndb.lock`
  file) and **reloads the value from disk**, so the block starts from whatever the last committer wrote
  rather than from this process's copy;
- leaving the block commits and releases the lock.

So a read-modify-write inside `lock` is atomic across processes: three programs each adding 100 to a
counter end at exactly 300. The lock is held by the OS, which releases it if the process dies; a crash
never strands a durable behind a stale lock.

Outside a lock, a durable is still loaded once at start and written once at exit. If another process
committed in between, writing would silently drop their update; so it is **refused** instead, naming
the variable and pointing at `lock`:

```
durable var `n`: another process committed to this image while this one was running, so writing
now would drop that update. Do the update inside `lock n as x { … }`, which holds the image
across processes for the block.
```

A value that did not change is never written at all, so read-only processes never conflict with a
writer, and a program that only reads a durable can run alongside one that updates it.

Two durables locked in two different orders can deadlock, so that is a build error; see
[lock order](16_concurrency.md#lock-order-is-checked).

### Evolving the shape: `migrate from`

A persisted value has a **schema**; the structure the bytes were written with. On load, Nori compares
the image's schema against the type you declared now:

- they match → the value loads directly;
- they differ and you declared a migration for that shape → the image is read *as the old type* and
  handed to your converter, whose result becomes the value (and the next commit writes the new shape);
- they differ and nothing matches → the load **fails with an error**, naming the variable.

That last case matters: the encoding is positional, so reinterpreting an old image with a new layout
does not fail on its own; it silently returns nonsense. Refusing is the only safe default.

Declare a migration by keeping the old shape around under a new name:

```nori
struct UserV1 { name: Str }                     // the shape already on disk
struct User { name: Str, age: Int }             // the shape you want now

fn upgrade(old: Vec<UserV1>) -> Vec<User> {
    var out: Vec<User> = vec()
    var i = 0
    while i < old.len() { out.push(User { name: copy old[i].name, age: 0 })  i = i + 1 }
    return out
}

durable var users: Vec<User> migrate from Vec<UserV1> with upgrade
```

Write one `migrate from … with …` clause per shape you still need to load; several old versions can
coexist. Clauses are matched by *shape*, not by the order you list them in.

#### Converters compose

A converter does not have to jump straight to the current type. If it returns a shape another clause
names, the two run in sequence; so a new version costs **one** new converter, not a rewrite of every
old one:

```nori,excerpt
fn v1_to_v2(old: Vec<UserV1>) -> Vec<UserV2> { … }   // one hop
fn v2_to_now(old: Vec<UserV2>) -> Vec<User>   { … }   // the next

durable var users: Vec<User> migrate from Vec<UserV1> with v1_to_v2
                             migrate from Vec<UserV2> with v2_to_now
```

A `Vec<UserV1>` image runs both converters; a `Vec<UserV2>` image runs only the second; a current image
runs neither. Either way the next commit writes the current shape, so the migration happens once.

The compiler builds those paths at build time and rejects the ways they can go wrong; each of these is
an error before your program runs, not a surprise in production:

- a converter whose **parameter** isn't the shape its clause names (it would be handed the wrong bytes
  and decode them positionally: wrong data, no trap);
- a chain that **never reaches** the current type (`nothing converts Vec<UserV2> to Vec<User>`);
- two clauses naming the **same** on-disk shape (the second could never run);
- a clause whose shape is the **current** one (it could never match a stored image);
- a converter that doesn't exist, or a set of clauses that loops.

#### The image it converted is kept

A migration replaces the stored value: the next commit writes the new shape over the file. So before a
converter runs, the image it is about to convert is copied to `<name>.ndb.bak`, byte for byte. If the
converter turns out to be wrong, the data it read is still there.

#### Inspecting what is on disk

`noric --db-info prog.nori` reads the image headers without running the program and reports, for each
`durable var`, whether the file is intact, the shape it holds against the shape declared now, and the
verdict: loads directly, migrates (naming the converters that would run), or refused. See
[Tooling](20_tooling.md).

#### Images migrate too

A `snapshot()` image records the shape of *each* global it holds, so `resume()` migrates the globals
whose shape changed and reads the rest directly. A plain `global` may therefore carry `migrate from`
clauses as well; it has no `.ndb` of its own, but it can be inside an image:

```nori
global users: Vec<User> migrate from Vec<UserV1> with upgrade
```

An image whose *layout* differs — different globals, or a different order — is still refused outright:
that is a different program, not an older shape.

The schema is **structural, not nominal**. It covers field names, their types, and their order,
recursively; so adding, removing, retyping or reordering a field is a new schema. Renaming the
*type* is not: the encoding never stores type names, so `struct Person { name: Str }` loads an image
written by `struct User { name: Str }`. That is what lets you copy a struct to `UserV1` and migrate
from it. Reordering two fields of the *same* type is caught, because the values would otherwise land
in each other's slots.

### Tables: querying a durable collection

For a collection of records, `std/table` gives `Table<T>`, rows plus typed queries. A `durable var`
of that type is a database with no glue code and no SQL:

```nori
import "std/table" as table
struct User { name: Str, age: Int }

durable var users: table::Table<User>

fn main() -> Int {
    let _id = users.insert(User { name: "ann", age: 34 })  // monotonic id, never reused
    let adults = users.where(|u| u.age > 18)               // predicate is an ordinary closure
    printl(adults.len())
    return 0
}
```

The predicate is a closure, so it is typed and checked at compile time: `u` is a `User`, and `u.aeg`
is a build error, not a runtime surprise. Row order is stable and ids are dense, which is what an index
needs. See [std/table](../std/table.md) for the full API (`count`, `insert`, `all`, `at`, `where`,
`count_where`, `any`, `find`, `index_of_id`, `delete_where`, `clear`).

### Equality queries are indexed for you

An equality query on a field is not a scan. `where`, `count_where`, `any` and `find` with a predicate
of the shape `|row| row.field == key` are compiled into a keyed-index lookup on that field:

```nori,excerpt
let anns = users.where(|u| u.name == who)     // compiled as an index lookup on `name`
```

The first such query indexes the rows (one pass); later ones answer in O(1), and a query after new
inserts indexes only what was added. The answers are exactly the scan's: same rows, same order, same
`find` position. So this is a compilation choice, not a different API. Repeated lookups stay fast
as the table grows, because the indexed path stops growing.

The scan stays when the two might not agree, and the fallback is silent because the result is the same
either way: a predicate that is not a field equality (`u.age > 18`), a key that is a call (a scan
evaluates it once per row, so hoisting it could drop a side effect), a receiver whose type is not
written down, a field that is neither `Str` nor `Int`, or a program that assigns that field name
anywhere; a row mutated in place through its handle would leave a keyed index stale.

For the cases the rewrite declines — a computed key, a key that is not a plain field — the index is
also a normal API. `index_get(name, key, keyfn)` keeps a keyed index and answers in O(1):

```nori,excerpt
let anns = users.index_get("byname", "ann", |u| copy u.name)
```

The key function is passed at the call site rather than stored, because a closure cannot be serialized
and a `Table` has to persist; so the index holds only data, and catches up lazily: a lookup indexes
whatever rows were appended since the last one. `delete_where` and `clear` reset it, since positions
shift. `index_count`, `index_has` and `index_find` answer the other three query shapes.

### `transient`, derived fields that never persist

An index is *derived* state: it can always be rebuilt from the rows, and writing it to disk would bloat
the image and slow every load. Mark such a field `transient`:

```nori
struct Table<T> { rows: Vec<T>, transient idx: Map<Str, Vec<Int>> }
```

A `transient` field is skipped when the value is written, rebuilt empty when it is read, and, because
it is not part of the shape on disk, **excluded from the schema fingerprint**. So adding a cache to a
persisted struct does not invalidate images already written, and needs no `migrate from`.

A `Table<T>` is an ordinary struct, so nothing about it is special-cased: any generic struct whose
fields are serializable can be a `durable var`.

## Program images: `snapshot` / `resume`

Where a `durable var` persists one value, `snapshot(path)` persists the **whole
program's global state at once**; every serializable global (and the heap it
reaches) written atomically into a single image file. `resume(path)` loads that
image back into the globals and returns `1`, or `0` if no image exists yet. This
makes a long computation interruptible: keep its progress in globals, `resume`
at start, and `snapshot` at a safe point (a loop boundary):

```nori
global step: Int = 0
global acc: Int = 0

fn main() {
  resume("job.img")                 // continue where a previous run left off
  while step < 1000000 {
    acc = acc + step * step
    step = step + 1
    if step % 100000 == 0 { snapshot("job.img") }   // checkpoint periodically
  }
  printl(acc)
}
```

Because the image is one atomic write (staged to a temp file, flushed, renamed
over the target, and CRC-checked), every global is captured at the same instant —
a crash mid-write leaves the previous consistent image intact. Only globals are
captured, not local variables or the call stack, so resumable state must live in
globals. The value types follow the same serializability rules as `durable var`.

Because an image captures only globals and heap, a raw `Int` fd cannot survive it —
after a restart it would name nothing. So `snapshot()` **refuses** (with an error)
if any *bare* fd (from `open_file`, or a raw socket) is open when it is called.

An image **also captures tasks that are still in flight**. A suspended task is a
chain of heap frames (Nori's coroutines are stackless), so it serializes like any
other data: a task sleeping on a timer resumes with its *remaining* time, and one
parked in `await`, on a channel, or on a mutex is restored together with the task
or object it waits on. Frame state travels by value (an `Int`, a `Str`, a `Vec`, a
struct), and handles are re-pointed at the restored objects.

A channel's **buffered values** travel too. The channel object itself doesn't know
its element type, so the type comes from the frame slot that names it; a
`Channel<Str>` parked mid-drain restores with its queued strings intact.

`snapshot()` still **refuses** the cases an image cannot faithfully rebuild, naming
which:

- a task blocked on a **socket**; a live TCP peer cannot be recreated;
- a task inside a **`within … else` block**, which carries a best-so-far checkpoint
  cell that outlives the block;
- a task parked on a **mutex whose holder is not itself captured**; nothing would
  release the lock after a restart, e.g. `main` holding it across the call;
- a task parked on a **channel whose buffered element type cannot be named**;
- a frame slot holding a value with **no serializer**.

Draining to a quiet point before snapshotting remains the simplest pattern, but it
is not required.

Typed handles are the exception to the open-resource rule: a `std/os` `File` or
`std/net` `Socket` held in a global **re-establishes itself** on resume. Snapshot serializes its reopen metadata
(a file's path/flags/mode/offset, a socket's bind address) rather than the fd, and
`resume` reopens the resource, re-`open`ing the file and seeking back to its offset,
or re-`listen`ing on the same port. So a `File`/`Socket` in a global survives an
image; only opaque bare fds must be closed first. (Accepted or connected client
sockets cannot be recreated and are not re-establishable.) Re-establishment works
on Linux and Windows; macOS is unported and aborts with a clear message.

Note that a `File`'s flags travel with it, so a handle opened with `O_TRUNC` is
reopened with `O_TRUNC`; the file is truncated again on every resume. For a
handle meant to accumulate across images, open it without it.

An image is a self-contained, address-free file, so it is portable: copy it to
another machine of the same target triple running the same binary and `resume`
picks up there, and any number of processes can `resume` the same image to fan out
identical clones of a frozen state. To keep that safe, `resume` verifies a layout
fingerprint stored in the image against the running program and errors on a
mismatch rather than misreading an image written by a different program or version.

## Related binding forms

Two specialized binding forms are covered in their own chapters:

- **`versioned var`** records a version on every assignment, enabling
  `x@undo` / `x@redo` / `x@mark` / `x@goto` time-travel over a value's history.
  See [Verification and Contracts](19_verification.md).
- Function parameters take the same four words (`view`, `copy`, `inout`, `sink`,
  plus write-only `set`), and mean the same things there, including `sink`,
  which moves the argument, so the caller's name is dead after the call. See
  [Functions](08_functions.md) and [The Memory Model](15_memory_model.md).

---

Next: [Operators](05_operators.md).
