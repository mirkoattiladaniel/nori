# 11. Enums and Pattern Matching

## Enums

An `enum` is a sum type: a value that is exactly one of several named variants.
Variants may be empty, carry a single payload, or carry several.

```nori
enum Shape {
    Circle(Float)          // one payload
    Rect(Float, Float)     // two payloads
    Nothing                // no payload
}
```

Construct a value with `Type::Variant`, supplying payloads if the variant has
them:

```nori,excerpt
let a = Shape::Circle(2.0)
let b = Shape::Rect(3.0, 4.0)
let c = Shape::Nothing
```

## Matching

`match` is how you inspect an enum. Each arm is `Pattern => { body }`; a variant
pattern binds its payloads to names.

```nori,excerpt
fn area(s: Shape) -> Float {
    match s {
        Shape::Circle(r)    => { return 3.14159 * r * r }
        Shape::Rect(w, h)   => { return w * h }
        Shape::Nothing      => { return 0.0 }
    }
}
```

`match` also matches literal values, with `_` as the catch-all wildcard:

```nori
fn name(n: Int) -> Str {
    match n {
        0 => { return "zero" }
        1 => { return "one" }
        _ => { return "many" }
    }
}
```

A `match` may be used as an expression when every arm produces a value of a common
type.

### `match sink`: taking the payloads {#match_sink}

The bindings of a plain `match` are **views** of the payloads: the value being
matched still owns them, so an arm may read them but not store or return them.
`match sink x { … }` tears the value down instead; each arm **owns** what it
binds and the shell is freed:

```nori
fn unwrap_or(sink o: Option<Str>, sink dflt: Str) -> Str {
    match sink o {
        Option::Some(s) => { return s }      // s is ours to return
        Option::None    => { return dflt }
    }
}
```

`x` is dead after a `match sink`. Use it when a `match` is the last thing done
with a value, which is what a `match` on a returned `Result` or `Option` usually is.

### Exhaustiveness {#exhaustiveness}

A `match` on an enum **must** cover every variant, or end with `_`. Leaving one out
is a build error that names the variants you missed:

```nori,error
enum Shape { Circle(Float)  Rect(Float, Float)  Nothing }

fn area(s: Shape) -> Float {
    match s {                                    // ERROR: `Nothing` is never matched
        Shape::Circle(r)  => { return 3.14159 * r * r }
        Shape::Rect(w, h) => { return w * h }
    }
}
```

```text
error: this `match` does not cover variant `Nothing`
  note: a match on an enum must handle every variant — an unmatched value would
        fall through the whole chain
  fix:  add an arm for `Nothing`, or a `_ => { … }` arm for the rest
```

Covering all cases is how you make illegal states unrepresentable, and the rule
earns its keep when a type grows: add a variant and every `match` that doesn't
mention it becomes a build error, so the compiler walks you through the places
that need a new arm instead of letting them silently fall through.

Use `_` when you genuinely mean "everything else"; it is the explicit opt-out,
and it keeps a `match` compiling as the enum grows.

### Guards, or-patterns, ranges, struct destructuring

Any arm may add an **`if` guard**; the arm is taken only when the pattern matches
*and* the condition (which sees the arm's binds) is true; otherwise the match keeps
trying the arms below. A guarded arm therefore never counts toward exhaustiveness.

**Or-patterns** join alternatives with `|` (variants, string/Int/Char literals, or
ranges); only the first pattern of the chain may bind a payload. `LO..HI` matches a
half-open Int or Char range. A `Name { field, … }` pattern destructures a struct
scrutinee, binding any subset of its fields by name; without a guard it is
irrefutable and covers like `_`.

```nori
enum Ev { Key(Int), Click(Int, Int), Quit, Refresh }

fn react(e: Ev) -> Str {
    match e {
        Ev::Key(k) if k == 27 => { return "escape" }
        Ev::Key(k) => {
            match k {
                48..58 | 65..91 => { return "alnum" }
                _ => { return "key " + k }
            }
        }
        Ev::Click(x, y) if x < 0 || y < 0 => { return "offscreen" }
        Ev::Click(x, y) => { return "click " + x + "," + y }
        Ev::Quit | Ev::Refresh => { return "control" }
    }
}
fn main() -> Int {
    printl(react(Ev::Key(66)) + " " + react(Ev::Click(3, 9)) + " " + react(Ev::Quit))
    return 0
}
```

Guards replace the nested-`if`-inside-arm pattern: the conditions read in the arm
head, and fall-through order stays visible top to bottom.

### `if let` and `while let`

When only one variant matters, `if let` / `while let` are concise sugar for a
two-arm match:

```nori,excerpt
if let Shape::Circle(r) = s {
    printl("radius " + to_str(r))
} else {
    printl("not a circle")
}
```

## Generic enums

Enums can be parameterized by type:

```nori
enum Pair<T> { One(T)  Two(T, T)  Empty }
```

The type parameter is used in the variant payloads and resolved at each use site.

## Prelude enums (`Option`, `Result`, `Json`)

Three widely used enums live in the standard library. Import the module, and then,
because they are *prelude* types, you name them **unqualified**, even when the
module is aliased:

- **`Option<T>`**: `Option::Some(v)` or `Option::None`; a value that may be
  absent. From `std/result`.
- **`Result<T, E>`**: `Result::Ok(v)` or `Result::Err(e)`; a value or an error. `Result<T>` = `Result<T, Str>`.
  From `std/result`. Central to [Error Handling](14_error_handling.md).
- **`Json`**: a recursive enum modeling JSON. From `std/json`.

```nori
// Option and Result are the prelude — no import needed. Import std/result only for its
// helper functions: import "std/result" as result

fn first(v: Vec<Int>) -> Option<Int> {
    if v.len() == 0 { return Option::None }
    return Option::Some(v[0])
}

fn main() -> Int {
    var xs: Vec<Int> = vec()
    match first(xs) {
        Option::Some(x) => { printl("head " + to_str(x)) }
        Option::None    => { printl("empty") }
    }
    return 0
}
```

`Option` and `Result` are ordinary enums; you `match` them, bind their payloads,
and construct them with `::`; the standard library just defines them for you. The
`expr else default` operator
(see [Operators](05_operators.md)) and the `try` form
(see [Error Handling](14_error_handling.md)) provide shortcuts for the common
"unwrap or fall back / propagate" patterns.

---

Next: [Traits and Generics](12_traits_and_generics.md).
