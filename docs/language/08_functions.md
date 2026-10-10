# 8. Functions and Closures

## Declaring functions

A function is declared with `fn`, a parameter list, an optional return type, and a
body. Parameters are `name: Type`; the return type follows `->`.

```nori
fn add(a: Int, b: Int) -> Int { a + b }
```

The trailing expression **is** the return value; the same rule `if`/`match`
expressions and `spawn { … }` blocks already follow. An explicit `return a + b`
means exactly the same thing; use it when an early exit makes the flow clearer.

A function that returns no value omits the `->` clause entirely:

```nori
fn log(msg: Str) {
    printl("[log] " + msg)
    // no return needed
}
```

Functions must be declared at the top level (not nested inside other functions).
They may be called before their declaration in the file: order does not matter.

### Unused functions

A top-level function your program never names produces a warning, not an error:

```
▲ [1] unused function `add`
  src/main.nori · 10:1

    10 ▏ fn add(inout a: Int, b: Int) -> Int {
       ▏ ━━ defined but never called

  ▸ remove it, or rename it `_add` to keep it on purpose
```

Prefix the name with `_` to keep a function you are not calling yet; the same
opt-out `_name` gives an unused variable.

The warning stays out of the cases where "nothing calls it" is the point, and so
never fires for:

- a `pub` function: being reached from outside the module is its whole job;
- anything a module brought in under an alias, or a method, since a method can be
  reached through a trait object with no mention of its name anywhere;
- `fn test_*`, which `roll test` finds by name;
- `roll build` on a library, and `noric --runtime` / `--test` / `--fuzz-laws` /
  `--freestanding`.

It is also deliberately quiet when a name is ambiguous: any other mention of the
same spelling (a struct field, a local, a method on some unrelated type)
counts as a use. A function that only ever calls itself counts its own call and
stays quiet too.

An unused **global** is reported the same way, under the same rules; see
[Globals](04_variables.md#globals).

## Return

`return expr` yields a value and exits; `return` alone exits a value-less
function. In a value-returning function, a body (or a trailing `if`/`match`
arm) that ends in a bare expression returns that expression, recursively, so
match arms need no `return` ceremony:

```nori
enum Size { S  M  L }
fn price(s: Size) -> Int {
    match s {
        Size::S => { 3 }
        Size::M => { 4 }
        Size::L => { if true { 5 } else { 6 } }
    }
}
```

A value-returning function must still return on every path, and the compiler
checks it; a function that can reach its end without returning is a build error.

```nori
fn sign(n: Int) -> Int {
    if n < 0 { return 0 - 1 }
    if n > 0 { return 1 }
    return 0
}
```

Drop that last `return` and the build stops:

```text
error: `sign` can finish without returning a Int
  note: a value-returning function must return on every path, but this one can
        reach its end
  fix:  add a `return` at the end, or give every branch (`if` without `else`, a
        loop that can exit) one
```

A path counts as returning if it ends in a `return`, calls `fail` (which never
comes back), or runs a loop it cannot leave (`while true` with no `break`). An
`if` counts only when it has an `else` and both branches return; a `match` counts
only when it is exhaustive and every arm returns. A `foreach`, or a `while` whose
condition can go false, never counts on its own; the loop may run zero times, so
execution can always reach the statement after it.

A function with no `->` clause returns nothing, so the rule does not apply to it.

## Recursion

Functions may call themselves:

```nori
fn fib(n: Int) -> Int {
    if n < 2 { return n }
    return fib(n - 1) + fib(n - 2)
}
```

## Parameter passing

A parameter receives the argument's **handle**, and a bare parameter is a
**view**: the callee may read it, and may not change what the caller passed.
Forgetting to say otherwise is a compile error, not a silent share.

```nori,error
fn total(v: Vec<Int>) -> Int { return v.len() }   // reads: fine
fn addto(v: Vec<Int>) { v.push(1) }               // ERROR: `push` writes to `v`, which is a view
```

Four words say what you mean. `view` is the default and may be written out.

- **`view`** *(default)*: read it; do not change it.

- **`copy`**: the callee gets its **own** value, so its changes never escape:

  ```nori
  struct V2 { x: Int, y: Int }
  fn scale(copy v: V2) -> Int { v.x = v.x * 2  return v.x }

  fn main() -> Int {
      var a = V2 { x: 1, y: 2 }
      printl(scale(a) + " " + a.x)   // 2 1 — the caller's value is untouched
      return 0
  }
  ```

  `copy` is a deep copy; see [Structs](10_structs.md) and [The Memory Model](15_memory_model.md).

- **`inout`**: the callee may change the caller's value. This means two
  different things depending on the type, and the difference is worth knowing:

  For a **scalar**, the callee writes *back* into the caller's variable, so the
  argument must be an assignable place:

  ```nori
  fn add_one(inout x: Int) { x = x + 1 }

  fn main() -> Int {
      var k = 7
      add_one(k)
      printl(to_str(k))     // 8 — the caller's k was updated
      return 0
  }
  ```

  For a **handle** (`Vec`, `Map`, `struct`), the object is already shared, so
  `inout` grants *permission to mutate it*; there is no write-back. Mutating it
  is visible to the caller; **rebinding the parameter is not**:

  ```nori
  fn addto(inout v: Vec<Int>) { v.push(9) }        // caller sees the push
  fn swap(inout v: Vec<Int>)  { v = vec() }        // caller sees nothing: rebinds the name
  ```

  Because a handle needs no write-back, a temporary or a call result is a
  perfectly good `inout` argument.

- **`sink`**: the callee **takes** the value. The caller's name is dead until it
  is given a new one. Use it when ownership genuinely moves.

- **`set`**: a write-only parameter: the callee assigns it and the value is
  written back. Like `inout` without reading first.

## Closures

A closure is an anonymous function written `|params| body`. It captures the
variables it uses from the enclosing scope. A scalar is copied; a heap value
(`Str`, `Vec`, a struct, …) is **moved** into the closure, which then owns it and
frees it when the closure dies. The enclosing name is dead after the capture —
keep using a value by capturing a `copy`, or by passing it as an argument:

```nori,error
var v: Vec<Int> = vec()
let f = |x: Int| v.len() + x     // v moves into f
v.push(1)                        // ERROR: `v` was moved away and can't be used again

var w: Vec<Int> = vec()
let w2 = copy w
let g = |x: Int| w2.len() + x    // g owns the copy; w stays yours
```

```nori
let base = 10
let add_base = |x| x + base      // captures `base` (an Int: copied)
printl(to_str(add_base(5)))      // 15
```

Parameters may be annotated, and `||` is a zero-parameter closure:

```nori,excerpt
let sq  = |x: Int| x * x
let now = || current_time()
```

Because a closure owns what it captures, it is a self-contained value you can
store, return, and pass around; it never dangles. `copy f` deep-copies a closure,
captures included.

### Closures as arguments

A function that takes a closure declares the parameter with a `Fn(Params) -> Ret`
type:

```nori
fn apply(f: Fn(Int) -> Int, v: Int) -> Int { return f(v) }

fn main() -> Int {
    printl(to_str(apply(|x| x * 2, 21)))   // 42
    return 0
}
```

Higher-order helpers in `std/iter` (`vmap`, `vfilter`, `vfold`) take closures
this way. Closures dispatch by identity with no heap indirection for the common
cases; they support up to six parameters.

---

Next: [Collections](09_collections.md).
