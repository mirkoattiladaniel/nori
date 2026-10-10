# 10. Structs

A `struct` is a named product type: a fixed set of typed fields.

## Declaring and constructing

```nori
struct Point { x: Int, y: Int }

fn main() -> Int {
    let p = Point { x: 3, y: 4 }     // struct literal — every field named
    printl(to_str(p.x))              // 3
    return 0
}
```

A field is a store, and its value must be the type the field was declared, the
same rule as a `let` with an annotation, and `Int` and `Float` do not cross here
either:

```nori,excerpt
struct Size { w: Float }

// let s = Size { w: 5 }       // ERROR: cannot initialise `Size.w`, which is Float, with Int
let s = Size { w: 5.0 }
```

Fields are accessed with `.`. To mutate a field, the binding must be a `var`:

```nori,excerpt
var p = Point { x: 0, y: 0 }
p.x = 10
p.y = p.x * 2
```

## Default field values

A field may declare a default with `= expr`; omit it in a literal to take the
default.

```nori
struct Config { verbose: Bool = false, retries: Int = 3, name: Str = "app" }

fn main() -> Int {
    let c = Config { retries: 5 }        // verbose=false, retries=5, name="app"
    printl(to_str(c.retries))            // 5
    return 0
}
```

## Which fields a literal must name

A literal that names *some* fields must name every field that has **no default**.
Naming a field the struct doesn't declare is a build error too, which catches a
misspelling.

```nori
struct Point { x: Int, y: Int }

fn main() -> Int {
    let a = Point { x: 1, y: 2 }   // every field named
    let b = Point {}               // x = 0, y = 0 — every field takes its default
    let c = Point { ..a, y: 9 }    // copy `a`, override y
    printl(to_str(a.x + (b.y + c.y)))
    return 0
}
```

These are rejected:

```text
Point { x: 1 }        error: struct `Point` literal is missing field `y`
Point { x: 1, z: 2 }  error: struct `Point` has no field `z`
```

The two opt-outs above — `Point {}` and `Point { ..base }` — are legal precisely
because each supplies the missing slots explicitly.

The point of the rule is what happens when a struct grows. Add a field without a
default and every construction site becomes a build error until you name it there
or give it a default; so a new field is never silently left at zero.

### Unused fields

A field the program fills in and never consults produces a warning, not an error:

```
▲ [1] unused field `Pt.dead`
  src/main.nori · 1:26

    1 ▏ struct Pt { x: Int, dead: Int }
      ▏                    ━━━━ never read — only written

  ▸ remove it, or rename it `_dead` to keep it on purpose
```

Writing a field is not reading it: `s.f = v` on its own leaves `f` unread, which
is the case this finds. Prefix the name with `_` to keep one deliberately.

Because deleting a field that is quietly load-bearing is worse than missing one,
several operations count as reading the **whole** record, and silence the warning
for every field in it:

- `V { ..base }`: the spread copies every field out of `base`;
- `a == b` on structs, which compares them field by field;
- a `durable var` and everything reachable from its type. The stored schema is
  structural, so dropping an unread field invalidates images already on disk;
- `snapshot` / `resume` anywhere in the program, which image every global.

A `cstruct` is never reported: a C binding must be namespaced, so its fields are
`NS__Pt.x`, and only your own module's structs are considered. `transient` fields
are skipped too; they are rebuilt rather than read.

## Spread: `{ ..base, field: value }`

Copy every field from an existing value, then override the ones you name:

```nori
struct Config { verbose: Bool = false, retries: Int = 3, name: Str = "app" }

fn main() -> Int {
    let base = Config { name: "svc" }
    let loud = Config { ..base, verbose: true }   // name="svc", retries=3, verbose=true
    printl(loud.name)
    return 0
}
```

The base is read at the point of the spread, so later changes to it don't affect
the copy.

## Methods

Methods are functions defined in an `impl` block for the type. The first
parameter, `self`, is the receiver; call a method with `receiver.method(args)`.

```nori
struct Counter { n: Int }

impl Counter {
    fn get(self) -> Int { return self.n }
    fn plus(self, k: Int) -> Int { return self.n + k }
}

fn main() -> Int {
    let c = Counter { n: 100 }
    printl(to_str(c.get()))          // 100
    printl(to_str(c.plus(5)))        // 105
    return 0
}
```

A type may have several `impl` blocks. Methods that implement a `trait` are
written in an `impl Trait for Type` block; see
[Traits and Generics](12_traits_and_generics.md).

## Handles, views, and `copy`

A struct is a **handle**: assigning or passing one gives a second name for the
*same* object. What Nori adds is that the second name is a **view** unless you say
otherwise; it reads, and writing through it is a compile error. So sharing is
never silent (see [The Memory Model](15_memory_model.md)).

```nori
struct P { x: Int, y: Int }

fn main() -> Int {
    var a = P { x: 1, y: 2 }
    var look = a       // a view: reading is fine
    printl(look.x)     // 1

    var b = inout a    // b names the same struct — and says so
    b.x = 99
    printl(a.x)        // 99

    var own = copy a   // my own value
    own.x = 7
    printl(a.x + " " + own.x)   // 99 7
    return 0
}
```

Writing through the view is refused:

```nori,error
var look = a
look.x = 99        // ERROR: `look` is a view — it can't be written through
```

`copy` works the same way in parameter position, which is how a function takes an
argument it may freely modify without the caller seeing it:

```nori
struct Vec2 { x: Int, y: Int }

fn scale(copy v: Vec2) -> Int { v.x = v.x * 2  return v.x }

fn main() -> Int {
    var a = Vec2 { x: 1, y: 2 }
    printl(scale(a) + " " + a.x)   // 2 1 — the caller's value is untouched
    return 0
}
```

**`copy` is deep.** A struct whose field is another struct, a vector or a string
gets its own copy of each, so the two values are fully independent:

```nori
struct Vec2 { x: Int, y: Int }
struct Rect { min: Vec2, max: Vec2 }

fn main() -> Int {
    var r1 = Rect { min: Vec2 { x: 1, y: 1 }, max: Vec2 { x: 9, y: 9 } }
    var r2 = copy r1
    r2.min.x = 99
    printl(r1.min.x + " " + r2.min.x)   // 1 99
    return 0
}
```

There is no type-level form. `copy struct` existed once and was removed: with a
bare name already being a view, forgetting `copy` is a compile error rather than a
silent share, so a second mechanism bought only ergonomics.

## Owning fields, `sink`, and `view` fields

A record **owns** its fields: when the record is freed its fields are freed with
it. So a field can only be given a value nobody else owns, namely a fresh one, a `sink`
(moved) one, or a `copy`:

```nori,error
struct Doc { title: Str, lines: Vec<Str> }

fn make(t: Str) -> Doc {
    var ls: Vec<Str> = vec()
    ls.push("first")
    return Doc { title: t, lines: ls }          // ERROR: `t` is a view (a bare parameter)
}
fn make2(sink t: Str) -> Doc {
    var ls: Vec<Str> = vec()
    ls.push("first")
    return Doc { title: t, lines: sink ls }     // t is owned here; ls moves in
}
```

`sink d.lines` moves a field *out*, leaving an empty vector behind, and an
accessor that hands back a field without giving it away is declared `-> view`:

```nori
struct Doc { title: Str, lines: Vec<Str> }
impl Doc {
    fn lines(self) -> view Vec<Str> { return self.lines }   // a borrow: the caller may read, not keep
    fn take_lines(inout self) -> Vec<Str> { return sink self.lines }   // a move: self.lines is empty now
}
```

A record that exists to *read* another value (a parser over its input, a cursor
over a buffer) declares the field it borrows as `view`:

```nori
struct Scanner { src: view Str, pos: Int }

fn scan(s: Str) -> Int {
    var sc = Scanner { src: s, pos: 0 }      // allowed: the field is a view
    sc.pos = sc.src.len()
    return sc.pos                            // sc is freed shallow; s is untouched
}
```

A `view` field is never freed by its record, and the record may not outlive the
value it views; the compiler checks both. `view struct S { … }` makes every field
of `S` a view.

What such a field takes is a borrow, so a name that is already one (a bare
parameter, or anything bound `view`) goes straight in; it is only elsewhere
that storing a view is refused.

## Generic structs

A struct can be parameterized by a type:

```nori
struct Box<T> { value: T }
```

Generics (including generic methods and trait bounds) are covered in
[Traits and Generics](12_traits_and_generics.md).

---

Next: [Enums and Pattern Matching](11_enums_and_matching.md).
