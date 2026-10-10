# 9. Collections

Nori provides three built-in collections: the growable `Vec<T>`, the string-keyed
`Map<K, V>`, and fixed-size arrays via `fill`, plus a `Pool<T>` arena. They are
operated on by **methods** — `v.push(x)`, `m.get(k)`, `s.len()` — and `.len()`
is one name across every sized value (Vec, Map, array, Str). There is exactly
one way to spell each operation.

The surface at a glance:

| Receiver | Operations |
|---|---|
| `Vec<T>` | `v.len()` · `v.push(x)` · `v.pop()` · `v.take(i)` · `v[i]` · `v[i] = x` |
| `Map<K, V>` | `m.len()` · `m.get(k)` · `m.put(k, x)` · `m.has(k)` · `m.delete(k)` · `m.keys()` · `m.values()` · `m.bump(k, d)` · `m.get_or_put(k, d)` · `m.upsert(k, d, f)` |
| array `[T]` | `a.len()` · `a[i]` · `a[i] = x` |
| `Str` | `s.len()` · `s.at(i)` · `s.slice(lo, hi)` |
| `Pool<T>` | `p.insert(node)` · `p.get(id)` |

`m.get(k)` traps on a missing key (with a message naming the key); the
non-trapping read is `m.get(k) else dflt`, which compiles to a single probe when the default is a
name or a literal, and otherwise to a presence check
plus branch; no allocation, and no separate `*_or` function to learn.

Under the hood a method call desugars to the corresponding builtin when the
receiver's type is statically known (an annotation, a parameter/global type, an
inferable initializer, a struct field, a container element, a call's declared
return type). A user `impl` may define methods with these names on its own types
— user methods always win there; the sugar applies only to the built-in
containers. If the compiler cannot tell a receiver's type (rare), the call stays
ordinary method dispatch and the fix is a type annotation.

```nori
fn word_lengths(text: Str) -> Vec<Int> {
    var out: Vec<Int> = vec()
    var word = 0
    var i = 0
    while i < text.len() {
        if text.at(i) == ' ' { out.push(word)  word = 0 } else { word = word + 1 }
        i = i + 1
    }
    out.push(word)
    return out
}
fn main() {
    let ls = word_lengths("the quick fox")
    printl(to_str(ls.len()) + " words, first has " + to_str(ls[0]) + " chars")
}
```

## `Vec<T>`, growable list

An ordered, growable sequence. Create it with `vec()` (an annotation is needed
since the literal is empty), then use:

| Operation | Meaning |
|---|---|
| `v.push(x)` | append `x` |
| `v.pop()` | remove and return the last element |
| `v.take(i)` | move element `i` out and return it, leaving an empty value in the slot |
| `v[i]` | read element `i` (bounds-checked) |
| `v[i] = x` | write element `i` |
| `v.len()` | number of elements |

```nori
var v: Vec<Int> = vec()
v.push(1)
v.push(2)
v.push(3)
v[0] = 10
let last = v.pop()          // 3
let n = v.len()             // 2
foreach x in v { printl(to_str(x)) }
```

Elements can be any type: `Vec<Str>`, `Vec<Point>`, `Vec<Vec<Int>>`, etc. When a
`Vec` owns its elements, the compiler reclaims them with the `Vec`.

Because that reclamation follows the **declared** element type, mixing `Str` with
anything else is an error rather than a silent reinterpretation: pushing a number
into a `Vec<Str>`, or passing a `Vec<Int>` to a `Vec<Str>` parameter, is refused at
the call. Numeric element types still widen (a `Vec<Int>` reaches a `Vec<Float>`
parameter), since neither is a handle to anything.

`v[i] = x` overwrites the slot; whether the value it displaces can be reclaimed
depends on what the compiler could prove about the vector. `v.take(i)` states the
same intent as a move: the element becomes the caller's value, reclaimed like any
other local, and the slot is left holding `""` (a `Vec<Str>`) or `0` (a scalar
vector), which is why `take` is limited to `Str`, `Int`, `Bool`, `Char` and
`Byte` elements. Use it where clearing a slot is how you release what was in it.

```nori
var names: Vec<Str> = vec()
names.push("ada")
let owned = names.take(0)   // "ada"; names[0] is now ""
```

## `Map<K, V>`, hash map

Write the type as `Map<K, V>`. The key type `K` may be `Str` (content-hashed), a
scalar (`Int`, `Char`, or `Bool`, identity-hashed: no string conversion, no
allocation per lookup), or a **struct whose fields are all scalars**, which
hashes and compares **structurally** (two keys with equal fields are the same
key). The key type is always written: `Map<V>` without it is a build error
that shows the fix, as is any unsupported key type (custom `Hash`/`Eq`
implementations are future work).

| Operation | Meaning |
|---|---|
| `m.put(k, v)` | insert/overwrite key `k` |
| `m.get(k)` | value at `k`, **traps** if `k` is absent |
| `m.has(k)` | is `k` present? (`Bool`) |
| `m.delete(k)` | remove `k` |
| `m.keys()` | a `Vec<K>` of the keys |
| `m.values()` | a `Vec<V>` of the values |
| `m.bump(k, d)` | add `d` to the value at `k` (inserting `d` when absent); evaluates to the **new** value. `Int` values only |
| `m.get_or_put(k, dflt)` | the value at `k`, inserting `dflt` first when absent |
| `m.upsert(k, dflt, f)` | replace the value at `k` with `f(current)`, starting from `dflt` when absent |

The last three read and write in **one hash probe**. So does `m.put(k, (m[k] else 0) + 1)`,
which the compiler recognizes; see [Updating a value in place](#updating-a-value-in-place).

```nori
fn main() -> Int {
    var byid: Map<Int, Str> = map()
    byid.put(42, "answer")
    byid[7] = "seven"
    printl(byid.get(42) + " " + (byid[9] else "none") + " n=" + byid.len())
    var ks = byid.keys()               // Vec<Int>
    var sum = 0
    foreach k in ks { sum = sum + k }
    printl(sum)
    return 0
}
```

```nori
var ages: Map<Str, Int> = map()
ages.put("ada", 36)
ages.put("bob", 41)
if ages.has("ada") { printl(to_str(ages.get("ada"))) }   // 36
var total = 0
foreach k in ages.keys() { total = total + ages.get(k) }
ages.delete("bob")
```

Values can be any type (`Map<Str, Vec<Int>>`, `Map<Str, Point>`, …); keys are `Str`, `Int`,
`Char`, `Bool`, or an all-scalar-field struct.

### Updating a value in place

Counting is the most common thing anyone does with a map, and the obvious spelling reads a key and
then writes it:

```nori,excerpt
m.put(w, (m[w] else 0) + 1)
```

That is **one hash probe**, not two: the compiler recognizes this exact shape (the same map and the
same key on both sides, a side-effect-free default and operand) and lowers it to a single
find-or-insert. Write it the natural way.

Three methods say it explicitly, for the cases the fusion does not cover:

| method | meaning |
|---|---|
| `m.bump(k, delta)` | add `delta` to the value at `k` (inserting `delta` when absent); evaluates to the **new** value. `Int` values only. |
| `m.get_or_put(k, dflt)` | the value at `k`, inserting `dflt` first when absent. Any value type. |
| `m.upsert(k, dflt, f)` | replace the value at `k` with `f(current)`, starting from `dflt` when absent. Any value type, any update. |

```nori
fn main() -> Int {
    var counts: Map<Str, Int> = map()
    counts.bump("word", 1)                          // count
    let total = counts.bump("word", 2)              // ... and read the running total
    printl(to_str(total))                           // 3

    var ids: Map<Str, Int> = map()
    let id = ids.get_or_put("word", 0)              // intern: the existing id, or the new one
    printl(to_str(id))                              // 0

    var lines: Map<Str, Str> = map()
    lines.upsert("k", "", |v| v + "line")           // accumulate — `v` types from the map
    printl(lines.get("k"))                          // line
    return 0
}
```

`upsert`'s closure parameter takes the map's value type, so it needs no annotation. The closure
**owns** the value it is handed: it may change it in place and hand it back (`|v| { v.push(x)  v }`),
or build a new one and let the old die with the call (`|v| v + "line"`). Nothing is copied unless
you write `copy`.

Two things to know about the default. It is an ordinary argument, so it is **evaluated even when the
key is present**: `index.get_or_put(w, vec())` builds a vector on every call and the runtime frees
the one it did not store. That is fine for `vec()` / `""` / a constant, but when the default is
expensive, keep the explicit form and only build it in the absent branch. On a hit the default
is freed whole, elements included.

`bump` is `Int`-only because a value slot holds raw bits; use `upsert` for `Float` counters.

## Iterators

`foreach` also loops over **any type with `fn next(self) -> Option<T>`**. This is the
structural iterator contract (no trait declaration needed; the method's shape is
the contract). The loop desugars to repeated `next()` calls: `Some(x)` runs the
body with `x` bound, `None` ends the loop. `break`/`continue` work as in any
loop, and each `next()`'s `Option` is freed as it is consumed, so iterator
loops are allocation-flat.

```nori
import "std/iter" as it

struct Fib { a: Int, b: Int, left: Int }
impl Fib {
    fn next(self) -> Option<Int> {
        if self.left == 0 { return Option::None }
        self.left = self.left - 1
        let cur = self.a
        let nx = self.a + self.b
        self.a = self.b
        self.b = nx
        return Option::Some(cur)
    }
}
fn main() -> Int {
    var s = 0
    var fb = Fib { a: 0, b: 1, left: 10 }
    foreach f in fb { s = s + f }
    printl(s)                       // 88 — no Vec was ever built
    var vowels = 0
    var cs = it::bytes("iterator")  // std/iter lazy blocks: lazy(lo, hi), bytes(s); characters: std/unicode chars(s)
    foreach c in cs { if c == 'i' || c == 'a' || c == 'e' || c == 'o' { vowels = vowels + 1 } }
    printl(vowels)
    return 0
}
```

A `next` returning anything other than `Option<T>` is not an iterator; the
loop rejects the type rather than guessing. (Bind the iterator to a variable
first; struct literals cannot appear in a loop header.)

## Set

`Set<T>` is the built-in set, with the same element types a `Map` accepts as keys and
the same performance. (Internally a `Set<T>` *is* a `Map<T, Int>`; diagnostics
may show the map spelling.) Construct with `set()`.

| Operation | Meaning |
|---|---|
| `s.add(x)` | insert, `true` if newly added, `false` if already present |
| `s.contains(x)` | membership (`Bool`) |
| `s.remove(x)` | remove, `true` if it was present |
| `s.len()` | element count |
| `s.items()` | a `Vec<T>` of the elements, insertion-ordered |

```nori
struct Cell { row: Int, col: Int }
fn main() -> Int {
    var seen: Set<Cell> = set()
    var dups = 0
    var i = 0
    while i < 20 {
        if !seen.add(Cell { row: i % 3, col: i % 2 }) { dups = dups + 1 }
        i = i + 1
    }
    printl("distinct=" + seen.len() + " dups=" + dups)
    return 0
}
```

(`std/set` remains for the struct-wrapped `Set`/`ISet` API; new code should
prefer the built-in. `std/hashmap`'s Int-keyed workaround is obsolete.)

### Reading a key that might be absent

`m.get(k)` traps when `k` is not present; the trap names the missing key. It is
the right call when the key is known to be there (you just `put` it, or `has`
already said so). When absence is a normal outcome, say so with `else`:

```nori
fn main() -> Int {
    var ages: Map<Str, Int> = map()
    ages.put("ada", 36)
    printl(to_str(ages["carol"] else 0))         // 0 — absent
    printl(to_str(ages.get("carol") else 0))     // the same read, method spelling
    return 0
}
```

**`m[k] else d` is the one to reach for.** Where `d` is a side-effect-free value it
compiles to a single hash probe and allocates nothing; `has` followed by `get` is
two probes over the same key, and that difference is measurable in a counting loop.

`std/result` adds two library forms for when a *value* is wanted rather than a
branch: `lookup_or(m, k, d)` is the same non-trapping read as a function, and
`mget(m, k)` answers an `Option`. Reach for `mget` when "absent" and "present" need
genuinely different handling; but note an `Option` is a heap object (both variants
allocate), so it costs an allocation per read that `else` does not.

## Fixed arrays, `fill`

`fill(n, init)` allocates a fixed-size, contiguous buffer of `n` elements, each
initialized to `init`. Its length is fixed (no `push`), and its flat storage makes
it the right choice for numeric and data-parallel work. `fill2(rows, cols, init)`
makes a 2-D array indexed `a[row, col]`.

```nori
var a = fill(4, 0)          // [0, 0, 0, 0]
a[2] = 9
let x = a[2]                // 9

var grid = fill2(3, 3, 0)   // 3x3
grid[1, 2] = 7              // 2-D index uses a comma
```

An array is a **handle**, like a `Vec` or a `struct`, so binding one to a second
name follows the same rule: a bare name is a `view` that reads, and sharing that
writes must be spelled `inout` (see [The Memory Model](15_memory_model.md)).

```nori,error
var a = fill(3, 0)
var r = a                   // a view — reads a's elements
let x = r[0]                // 0
r[1] = 9                    // ERROR: `r` is a view — it can't be written through

var w = inout a             // the same array, and says so
w[1] = 9
a[1]                        // 9 — the write through `w` is visible through `a`
```

Parameters follow the same rule, so a function that fills its caller's array says
so:

```nori
fn fill_squares(inout a: [Int], n: Int) {
    var i = 0
    while i < n { a[i] = i * i  i = i + 1 }
}

fn total(a: [Int], n: Int) -> Int {      // a bare parameter reads
    var s = 0
    var i = 0
    while i < n { s = s + a[i]  i = i + 1 }
    return s
}
```

### Returning an array

A function may return a `[T]`, and doing so hands the buffer to the caller: the
callee skips the free it would otherwise do at scope end, and the caller frees it
at its own last use. One owner, start to finish.

```nori
fn squares(n: Int) -> [Int] {
    var a: [Int] = fill(n, 0)
    var i = 0
    while i < n { a[i] = i * i  i = i + 1 }
    return a                       // the buffer moves to the caller
}

fn report() {
    let s = squares(8)             // `s` owns it now; freed after its last use
    printl(to_str(s[7]))           // 49
}
```

That transfer is only sound when the array is **freshly allocated here**, so the
compiler accepts exactly that shape: a local filled by `fill(…)` in this function
(or handed up from another array-returning function), and never reassigned.
Returning a parameter, a global, or an alias of either would have the caller free
storage the function never owned, and each is refused by name, with the reason
and the alternatives: return a `Vec<T>`, or take an `inout [T]` parameter and
fill it.

Different arrays may be returned on different paths. Each `return` hands back one
of them and frees the others that are live at that point, so exactly one buffer
survives the call whichever way control went:

```nori
fn pick(n: Int) -> [Int] {
    var a: [Int] = fill(n, 1)
    var b: [Int] = fill(n, 2)
    if n > 3 { return a }      // frees b
    return b                   // frees a
}
```

The result has exactly one legal position: **bound to a name**. That binding is
what owns the buffer, so a bare `mk(4)` statement, `sum(mk(4), 4)`, and
`spawn mk(4)` are all refused; bind it on its own line and pass the name.
Once bound it is an ordinary `[T]` again.

A method may return one too, on the same terms. The exception is a **trait**
method: the call is dispatched dynamically, so there is no static callee to read
the return type from, and the caller could not take ownership; that is refused
at the definition.

The last limit is the element type, which must have a fixed machine width (`Int`,
`I32`, `F64`, …); `[Str]` and `[Byte]` are stored as untyped slots and cannot be
returned.

Element storage width can be chosen with a sized-integer annotation
(`var bytes: [U8] = fill(16, 0)`), which controls memory layout, useful for
buffers and FFI. Independent `foreach` loops over a `fill` array are prime
candidates for automatic parallelization (see [Concurrency](16_concurrency.md)).

## `Pool<T>`, handle arena

A `Pool<T>` allocates many `T` records with stable integer handles and cheap
allocation, ideal for graph/tree nodes and other long-lived object soups. You
insert values and refer to them by handle. See the
[standard library reference](../std/) for the pool API in practice.

## Choosing a collection

- **`Vec`**: an ordered list that grows; the default sequence.
- **`Map`**: keyed lookup by string.
- **`fill`**: a fixed-size numeric/byte buffer; fastest and parallel-friendly.
- **`Pool`**: many small records with stable handles.

---

Next: [Structs](10_structs.md).
