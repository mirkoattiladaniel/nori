# 7. Control Flow

## `if` / `else`

`if` runs a block when a `Bool` condition holds; `else` and `else if` chain
alternatives. The condition must be a `Bool`; there is no truthiness.

```nori,excerpt
if score >= 90 {
    printl("A")
} else if score >= 80 {
    printl("B")
} else {
    printl("C")
}
```

### `if` as an expression

`if`/`else` can also be an **expression** that produces a value: each branch's
final expression is its value, and the branches must share a type.

```nori,excerpt
let grade = if score >= 90 { "A" } else if score >= 80 { "B" } else { "C" }
let sign  = if n < 0 { 0 - 1 } else { 1 }
```

An `if` used as an expression must have an `else` (every path must produce a
value).

## `while`

`while` repeats a block while a `Bool` condition holds.

```nori
var i = 0
while i < 10 {
    printl(to_str(i))
    i = i + 1
}
```

## `foreach`

`foreach` iterates over a range or a collection.

Over a **range** `lo..hi` (from `lo` up to but not including `hi`):

```nori
var sum = 0
foreach i in 0..5 { sum = sum + i }    // 0+1+2+3+4 = 10
```

Over a **collection** (a `Vec` or a `fill` array), binding each element:

```nori
var xs: Vec<Int> = vec()
xs.push(3)  xs.push(4)  xs.push(5)
var total = 0
foreach x in xs { total = total + x }  // 12
```

When the compiler can prove the iterations are independent, `foreach` may be run
in parallel automatically (see [Concurrency](16_concurrency.md)); the source is
unchanged and the result is identical.

## `break` and `continue`

`break` exits the innermost loop; `continue` skips to its next iteration.

```nori
var i = 0
while true {
    i = i + 1
    if i % 2 == 0 { continue }   // skip evens
    if i > 9 { break }           // stop past 9
    printl(to_str(i))            // 1 3 5 7 9
}
```

### Loop labels

Label a loop with `name:` on the same line as the loop, then `break name` /
`continue name` to target it from an inner loop.

```nori
outer: foreach i in 0..3 {
    foreach j in 0..3 {
        if i + j == 3 { break outer }   // leaves both loops
    }
}
```

The label must be on the **same line** as `break`/`continue`. On the next line it
would read as a bare `break` followed by a separate expression statement, leaving
only the innermost loop; so the compiler refuses it:

```text
error: the loop label must be on the same line as `break`
  fix:  write `break outer`; on the next line it reads as a separate statement and
        this would leave only the innermost loop
```

A `(` or `[` that continues an expression follows the same rule, for the same
reason; see [Statements and line breaks](02_lexical_structure.md#statements-and-line-breaks).

## `defer`

`defer S` registers statement `S` to run when the **enclosing block** ends, by
whatever path: falling off the end, an early `return`, a `break`/`continue`, or an
exception-free unwinding like a `region`'s exit. It is the way to say "when this
scope ends, put that back", which auto-free covers for memory the language made and
for nothing else (locks, saved interrupt flags, reference counts, raw blocks
allocated by `free`-style FFI).

```nori,excerpt
fn sys_readlinkat(dirfd: Int, upath: Int, ubuf: Int, size: Int) -> Int {
    if size <= 0 { return EINVAL() }
    let n = at_lookup(dirfd, upath, AT_SYMLINK_NOFOLLOW())
    if n < 0 { return n }
    if !inode_pin(n) { return ENOENT() }
    defer inode_unpin(n)                // on every path below: return, error, success
    if !is_lnk(n) { return EINVAL() }
    let t = link_target(n)
    ...
}
```

Rules and semantics:

- **Scope, not function.** A defer runs when the block that contains it ends, like
  Zig/Swift. A defer inside a loop body therefore runs at the end of **each**
  iteration — the natural shape for a per-iteration lock or buffer — and a defer
  inside an `if` body runs at that `if`'s end.
- **LIFO within a scope.** Deferred statements in one block run in reverse order of
  registration, and nested blocks unwind before their parents; so a resource taken
  inside another is released first, no matter which exit path runs.
- **Against auto-free.** The deferred statements run **before** the block's owned
  values are reclaimed, so a defer may name a value the scope owns
  (`defer log(msg)`, where `msg` is a `Str`). A trap (an out-of-bounds access or
  division by zero) does **not** run defers: the runtime aborts, there is no path
  back to this block's cleanup.
- **The return value first.** `return f(x)` computes `f(x)` *before* the defers
  run, then runs them, then returns; the value is never rewritten by the cleanup:
  `x = 1; defer x = x * 10; return x` returns `1`.
- **The statement runs later, and so do the expressions in it.** `defer S` does not
  evaluate `S`'s parts at the `defer` and save them for later; the whole statement
  runs at the block's end, reading whatever the names hold *then*. This is Zig's
  rule, and it is **not Go's**, where a deferred call's arguments are evaluated and
  saved where the `defer` is written:

  ```nori,excerpt
  var s = "first"
  defer printl(s)        // prints "second": `s` is read when the block ends
  s = "second"
  ```

  It follows that `defer free(b)` releases whatever `b` holds at the end, so
  reassigning `b` after the `defer` leaks the first block and frees the second.
  Where a name will change, defer in a scope where it will not, or bind the value
  you mean to a `let` first. A defer naming a value moved out with `sink` sees the
  emptied place — a `Str` of length 0 — which is harmless for a `free` and
  surprising for a `log`.
- **What a defer may name.** Values declared before it, only; the statement is
  checked at its `defer` position, so a name declared later is a compile error at
  the `defer`, not a mystery at the block's end. A defer may not declare a name,
  may not `return`/`break`/`continue`, and may not nest another `defer` (two
  cleanups go in two `defer`s, which already run in reverse order). A defer inside
  a `parallel foreach` is refused (there is no shared block end).
- **Cost.** A block with no `defer` emits exactly the code it always did; the
  deferred statements are emitted only where a scope actually ends.

## `match`

`match` compares a value against patterns, running the first arm that matches.
Arms are `pattern => { body }`. `_` is the wildcard that matches anything.

Matching literal values (with an exhaustive wildcard):

```nori
fn classify(n: Int) -> Str {
    match n {
        0 => { return "zero" }
        1 => { return "one" }
        _ => { return "many" }
    }
}
```

Matching an **enum** and binding its payload:

```nori
enum Shape { Circle(Float)  Square(Float)  Nothing }

fn area(s: Shape) -> Float {
    match s {
        Shape::Circle(r) => { return 3.14 * r * r }
        Shape::Square(w) => { return w * w }
        Shape::Nothing   => { return 0.0 }
    }
}
```

`match` can also be used as an expression (each arm supplies a value of a common
type). Patterns and exhaustiveness are covered fully in
[Enums and Pattern Matching](11_enums_and_matching.md).

### `if let` and `while let`

When you care about a single variant, `if let` and `while let` are concise
sugar for a two-arm `match`:

```nori,excerpt
if let Shape::Circle(r) = s {
    printl("circle of radius " + to_str(r))
} else {
    printl("something else")
}

// drain a channel until it closes, say
while let Option::Some(v) = next() {
    handle(v)
}
```

---

Next: [Functions and Closures](08_functions.md).
