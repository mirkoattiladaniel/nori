# 14. Error Handling

Nori separates **recoverable** errors (expected conditions a caller should
handle) from **unrecoverable** ones (bugs and impossible states). Recoverable
errors are ordinary values (`Result` / `Option`); unrecoverable ones abort with
`fail` (and the built-in traps).

## `Result<T, E>`

`Result<T, E>` (from `std/result`) is the workhorse: a success `Ok(T)` or a
failure `Err(E)`. It has **two** type parameters: the value type and the error
type. A common shape is `Result<Int, Str>` (a value, or an error message).

```nori,setup

fn parse(s: Str) -> Result<Int, Str> {
    if s.len() == 0 { return Result::Err("empty input") }
    return Result::Ok(parse_int(s))
}
```

Handle it by `match`, binding `Ok`'s value or `Err`'s error:

```nori
fn main() -> Int {
    match parse("") {
        Result::Ok(v)  => { printl("value: " + to_str(v)) }
        Result::Err(e) => { printl("error: " + e) }        // error: empty input
    }
    return 0
}
```

## `Option<T>`

`Option<T>` (also from `std/result`) models a value that may be **absent**,
`Some(v)` or `None`, when there is no meaningful error to carry, just presence or
absence.

```nori
fn head(v: Vec<Int>) -> Option<Int> {
    if v.len() == 0 { return Option::None }
    return Option::Some(v[0])
}
```

Both `Option` and `Result` are prelude enums: after importing `std/result` you
name them unqualified (`Option::Some`, `Result::Ok`).

## Propagating with `try` {#try}

`try expr` unwraps an `Ok`/`Some` to its inner value, or, if it is `Err`/`None`,
**returns that failure from the current function**. It is the concise way to
thread a result through a chain without a `match` at every step. The enclosing
function must itself return a compatible `Result`/`Option`.

```nori
fn plus_one(s: Str) -> Result<Int, Str> {
    let n = try parse(s)        // Ok -> n; Err -> return the Err from plus_one
    return Result::Ok(n + 1)
}

fn main() -> Int {
    match plus_one("41") { Result::Ok(v) => { printl(to_str(v)) }  Result::Err(e) => { printl(e) } }  // 42
    match plus_one("")   { Result::Ok(v) => { printl(to_str(v)) }  Result::Err(e) => { printl(e) } }  // empty input
    return 0
}
```

### Converting the error: `try … with`

`try` requires the propagated error to be the enclosing function's error type —
error types are erased at runtime, so letting an unrelated type through would
corrupt the caller's `match` (this is a compile error that names both types).
To propagate across error types, convert it in place:

```nori
enum ConfigErr { Missing(Str), Malformed(Str) }

fn read_port(s: Str) -> Result<Int, Str> {
    if s.len() == 0 { return Result::Err("empty") }
    return Result::Ok(8080)
}
fn wrap(e: Str) -> ConfigErr { return ConfigErr::Malformed("cfg: " + e) }

fn load(raw: Str) -> Result<Int, ConfigErr> {
    let a = try read_port(raw) with ConfigErr::Missing   // variant ctor: Err(Missing(the Str))
    let b = try read_port(raw) with wrap                 // mapping fn:  Err(wrap(the Str))
    return Result::Ok(a + b)
}
```

The converter is an error-mapping function (`fn(E1) -> E2`) or a variant
constructor of the target error type taking the source error as its payload.
`try` on an `Option` inside a `Result`-returning function always needs a
converter — `None` carries no error value — and there it takes **no argument**:

```nori
enum ConfigErr2 { NoKey }
fn first(v: Vec<Int>) -> Option<Int> {
    if v.len() == 0 { return Option::None }
    return Option::Some(v[0])
}
fn need(v: Vec<Int>) -> Result<Int, ConfigErr2> {
    let k = try first(v) with ConfigErr2::NoKey
    return Result::Ok(k)
}
fn main() -> Int {
    match need(vec()) { Result::Ok(k) => { printl(k) }  Result::Err(e) => { printl("none") } }
    return 0
}
```

## Falling back with `else`

`expr else default` supplies a fallback when `expr` is `None`/`Err`, yielding the
inner value otherwise. It is the "unwrap or use this instead" operator, and sits
at the lowest precedence (see [Operators](05_operators.md)).

```nori
fn get_or(s: Str) -> Int {
    return parse(s) else 0 - 1      // the parsed Int, or -1 on any error
}
```

Use `try` to *propagate* a failure and `else` to *absorb* it with a default.

## Unrecoverable errors: `fail` and traps

`fail(msg)` aborts the program: it writes `msg` to standard error and exits
non-zero. Use it for genuinely impossible states and unrecoverable conditions —
not for errors a caller could reasonably handle.

```nori
fn must_positive(n: Int) -> Int {
    if n <= 0 { fail("expected a positive number, got " + to_str(n)) }
    return n
}
```

The runtime also **traps** on checked violations (an out-of-range index, a
division by zero), printing a located message and exiting rather than corrupting
memory. Traps and `fail` are the same mechanism: a clean, immediate abort.

## Debugging failures

Compiled with `--debug` (via `roll build` in a debug profile, or `noric --debug`),
`fail` and traps print the **source location** and a **call-chain backtrace**, so
you see exactly where and how execution reached the failure, at zero cost when
`--debug` is off. Nori's `why` causal-debugging (see
[Verification and Contracts](19_verification.md)) can additionally explain how a
particular value came to be.

## Which builtins trap, and what to use instead

A builtin that cannot do what it was asked **traps**. That is deliberate: the alternative is a
function that returns something plausible for a failure, which a caller then carries as if it were
real. For each one, `std/result` has a non-trapping counterpart that returns `Result`/`Option`.

| builtin | traps when | non-trapping alternative |
|---|---|---|
| `read_file(path)` | missing, unreadable, a directory, or an I/O error mid-read | `result::try_read_file(path) -> Result<Str>` |
| `write_file(path, s)` | the directory is missing or read-only, the write cannot complete, or the replace fails | `result::try_write_file(path, s) -> Result<Str>` |
| `parse_int(s)` | `s` is not an integer | `result::try_parse_int(s) -> Result<Int>`, or the `parse_int_or(s, d)` builtin |
| `v[i]` | index out of range | `result::vget(v, i) -> Option<T>` |
| `m[k]` | key absent | `result::mget(m, k) -> Option<V>`, or `result::lookup_or(m, k, d)` |
| `a / b` | `b` is zero | `result::try_div(a, b) -> Result<Int>` |

`result::file_exists`, `result::is_regular_file` and `result::is_directory` answer the questions the
wrappers ask, when you want to check first yourself. Existence alone is rarely the right test:
`access` is perfectly happy with a directory, and `read_file` is not.

### The I/O guarantee

**A write that did not happen is never reported as success.** `write_file` stages the content beside
the destination and renames it into place, so a reader sees either the old file or the whole new one.
Every step of that is checked: a short write, a failed flush on close, and a failed replace each abort
with the cause named and the destination left unchanged. This matters more than it sounds; the one
outcome worse than a program that stops is a program that continues believing your data is on disk.

`try_write_file` catches what can be known in advance (a destination that is a directory, a parent
directory missing or unwritable) and returns `Err`. It cannot pre-empt a full disk or an I/O error,
because nothing can be known about those until the bytes are on their way; those still trap, loudly.

## Choosing a style

- **`Result<T, E>`**: an error the caller should handle (parse failure, missing
  file, bad input). Return it; the caller `match`es, `try`s, or `else`s it.
- **`Option<T>`**: mere absence, no error detail needed.
- **`fail` / traps**: a bug or impossible state; abort loudly rather than limp on.

---

Next: [The Memory Model](15_memory_model.md).
