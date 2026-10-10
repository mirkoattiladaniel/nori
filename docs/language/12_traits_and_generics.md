# 12. Traits and Generics

## Traits {#traits}

A `trait` is an interface: a set of method signatures a type can implement.

```nori
trait Shape {
    fn area(self) -> Float
    fn name(self) -> Str
}
```

A method signature in a trait has no body (a **default** method may provide one;
see below). A type implements the trait in an `impl Trait for Type` block:

```nori,excerpt
struct Circle { r: Float }

impl Shape for Circle {
    fn area(self) -> Float { return 3.14159 * self.r * self.r }
    fn name(self) -> Str   { return "circle" }
}
```

The compiler checks conformance: an `impl Trait for Type` must define every method
the trait declares.

### Default methods

A trait method may include a body, which becomes the default for implementers
that don't override it:

```nori
trait Greet {
    fn name(self) -> Str
    fn hello(self) -> Str { return "hi, " + self.name() }   // default
}
```

## Generics

Functions, structs, and enums can be parameterized by type with `<T>`. A type
parameter is an ordinary `i64` slot at runtime, so generics cost nothing beyond
the code they expand to.

```nori
struct Box<T> { value: T }

fn identity<T>(sink x: T) -> T { return x }   // takes the value and hands it back
```

A **parameter** of a generic struct type keeps its type argument, exactly like an
annotated local does; so its fields and methods resolve, and an unannotated closure
passed to one of its methods is typed from that argument (this is what lets
`fn adults(t: Table<User>) -> Int { return t.count_where(|u| u.age > 18) }` type `u`
as a `User`):

```nori
struct Crate<T> { value: T }

fn unwrap(c: Crate<Int>) -> Int { return c.value }   // `c.value` is an Int, not an erased slot
```

### Trait bounds

Constrain a type parameter with `<T: Trait>` so the body may call the trait's
methods. This is **monomorphized**: the compiler generates a specialized copy per
concrete type, so the calls are static (no dynamic dispatch).

```nori,excerpt
fn describe<T: Shape>(s: T) -> Float {
    return s.area()          // allowed: T is bounded by Shape
}

fn main() -> Int {
    printl(to_str(describe(Circle { r: 1.0 })))   // 3.14159 — a Circle-specialized copy
    return 0
}
```

## Static vs dynamic dispatch

Nori gives you both, and the choice is in how you write the type:

- **Generic bound `<T: Shape>`**: *static* dispatch. Monomorphized: one
  specialized function per concrete type, calls resolved at compile time. Fastest;
  use it by default.

- **Trait object `Shape`**: *dynamic* dispatch. A value typed as the trait
  itself is a fat value `{tag, data}` that carries its method table, so different
  concrete types can be mixed in one collection and dispatched at runtime.

```nori,excerpt
// dynamic: a heterogeneous collection of Shapes
fn total(shapes: Vec<Shape>) -> Float {
    var sum = 0.0
    foreach s in shapes {
        sum = sum + s.area()     // dispatched dynamically per element
    }
    return sum
}

fn main() -> Int {
    var v: Vec<Shape> = vec()
    v.push(Circle { r: 1.0 })
    v.push(Square { w: 2.0 })   // a different type in the same Vec
    printl(to_str(total(v)))     // 7.14159
    return 0
}
```

Reach for a generic bound when the type is fixed at each call site (the common
case), and a trait object when you genuinely need to store or pass mixed
implementations behind one type.

## Generic methods and cross-module traits

Methods can be generic, and a trait defined in one module can be implemented for a
type in another (implementations are found across `import`s). Modules and
visibility are covered next.

---

Next: [Modules and Namespaces](13_modules.md).
