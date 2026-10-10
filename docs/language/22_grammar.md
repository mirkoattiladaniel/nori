# 22. Grammar Summary

A consolidated overview of Nori's syntax, gathering the forms defined throughout
the reference. It is a summary for orientation, not a formal grammar; `NAME` is a
placeholder, `?` marks optional, `…` marks repetition.

## Program structure

A source file is a sequence of top-level items:

```
import "PATH" ?as NAME          # module import (aliased or flat)
use NAME::NAME                  # bring a namespaced name into scope
?pub global NAME: TYPE ?= CONST # top-level constant; CONST for a scalar (required for Int), none otherwise
                                # an Int's CONST, a [T]'s size and each `[a, b, …]` element: a constant integer
                                # expression (literals, unary - ~, ( ), * / % + - << >> & ^ |), folded at build time
?pub threadlocal global NAME: Int = CONST   # one instance per thread (per CPU in a freestanding image)
?pub frozen global NAME: TYPE ?= CONST      # written only before the first spawn, so a task may read it
?pub durable var NAME: TYPE = EXPR ?migrate from OLD with FN   # persists across runs
?pub fn NAME(PARAMS) ?-> ?view TYPE ?deprecated ?"MSG" { BODY }   # PARAMS: ?view|copy|inout|sink|set NAME: TYPE
@irq ?pub fn NAME(…) { BODY }   # interrupt handler: it and everything it calls may not allocate
?@section(".NAME") ?@align(N) ?@code32 naked fn NAME() { unsafe { asm(…) } }   # placed bytes, no prologue
?pub struct NAME?<PARAMS> { FIELD: TYPE ?= EXPR, … }
?pub enum NAME?<PARAMS> { VARIANT ?(TYPE, …)  … }
?pub trait NAME { fn NAME(?inout self, …) ?-> TYPE  … }
TYPE:      # a type is also a function reference — a signature with the declaration's annotations
           # (see Unsafe and FFI, Function references):
           #   [@irq|@nospin|@needs(L)|@takes(L)|@sleeps]* fn(TYPE, …) ?-> TYPE    the value is the code's address
           #   (@nospin and @sleeps are opposites and never appear together)
           #   conventional types:  Int Float Bool Char Byte I8..U64 [T] Vec<…> Map<…> Fn(…)->… …
impl NAME { fn … }              # methods
impl TRAIT for NAME { fn … }    # trait implementation
extern fn NAME(TYPE, …) ?-> TYPE ?from "LIB"   # LIB: shared library the symbol is imported from;
                                                # a cstruct NAME here passes by value
extern "HEADER" link "LIB"      # C binding
cstruct NAME { FIELD: SIZEDINT | Ptr | [SIZEDINT; N] | CSTRUCTNAME, … }   # a C-ABI layout; FIELD may name
                                                # another cstruct (laid out inline) or a fixed array;
                                                # `let ?var NAME: CSTRUCTNAME = EXPR` or a `NAME: CSTRUCTNAME`
                                                # parameter makes NAME a view over an address it already holds
law NAME(PARAMS) { assert(EXPR)  … }
namespace NAME { ITEMS }
cfg(NAME) { ITEMS }             # conditional (target or --cfg)
schedule NAME { … }
```

## Types

```
Int  Float  Bool  Char  Str            # primitives
I8 I16 I32 I64  U8 U16 U32 U64          # sized integers
Vec<T>   Map<K, V>   Set<T>   [T]      # collections (arrays are [T]; map keys: Str, Int,
                                        # Char, Bool, or an all-scalar-field struct)
Task<T>  Channel<T>  Timed<T>  Deferred<T>  Mutex
Option<T>  Result<T, E>  Json           # prelude enums — Option/Result need NO import;
                                        # Result<T> = Result<T, Str>
NAME  NAME<T, …>                        # user struct/enum, possibly generic
(T, U, …)                               # tuple
Fn(T, …) -> R                           # closure type
CFn(T, …) ?-> R                         # C function pointer (an address; calling one is unsafe)
```

## Bindings and statements

```
let NAME ?: TYPE = EXPR                 # immutable
var NAME ?: TYPE = EXPR                 # mutable
var NAME = ?view|copy|inout|sink EXPR   # what the name gives you: view is the default
let (A, B, …) = EXPR                    # tuple destructuring
versioned var NAME: TYPE = EXPR         # with @undo/@redo history
NAME = EXPR                             # assignment (var only)
LVALUE[i] = EXPR   LVALUE.field = EXPR  # element / field assignment
EXPR                                    # expression statement (e.g. a call)
return ?EXPR
break ?LABEL       continue ?LABEL
```

Control flow:

```
if COND { … } ?else if COND { … } ?else { … }
while COND { … }
foreach NAME in LO..HI { … }
foreach NAME in COLLECTION { … }        # Vec/Channel/session — or any type with
                                        # `fn next(self) -> Option<T>` (structural iterator)
LABEL: foreach …                        # labeled loop
match EXPR { PATTERN => { … }  … }
if let PATTERN = EXPR { … } ?else { … }
while let PATTERN = EXPR { … }
defer STMT                            # run STMT when the enclosing block ends, by any path
region { … }
unsafe { … }
```

Concurrency:

```
spawn EXPR        spawn { … }           # -> Task<T>
await EXPR
parallel { let A = …  let B = …  … }    # fork/join
parallel foreach NAME in RANGE { … }
parallel reduce(OP, INIT) over NAME in RANGE { … }
lock MUTEX as NAME { … }
within N?ms { … } ?else { … }           # deadline (-> T, or Timed<T> without else)
race { … } or { … }
hedge N?ms { … } or { … }
select { recv NAME = CHAN => …  after N?ms => … }
{ … } until { … } ?else { … }           # -> Deferred<T>
yield EXPR                              # publish a best-so-far inside `within`
```

## Expressions

```
LITERAL                                 # 42  0xFF  0b1010  3.14  true  'c'  "s"  "${e}"
NAME                                    # variable / global / function
EXPR OP EXPR                            # binary (see precedence below)
-EXPR   !EXPR   ~EXPR                    # unary
EXPR(ARGS)                              # call
cfn_addr(NAME)                          # C-callable address of a top-level fn (unsafe)
EXPR.NAME(ARGS)                         # method call
EXPR.NAME                               # field access
EXPR[i]     EXPR[i, j]                   # index (1-D / 2-D)
NAME::VARIANT ?(ARGS)                   # enum construction
NAME { FIELD: EXPR, … }                 # struct literal
(A, B, …)                               # tuple
|PARAMS| EXPR                           # closure
if COND { EXPR } else { EXPR }          # if-expression
match EXPR { … }                        # match-expression
try EXPR ?with CONV                     # unwrap-or-propagate; CONV converts the error
                                        # (fn(E1)->E2 or an E2 variant ctor) across error types
EXPR else EXPR                          # fallback
NAME@undo   NAME@redo   NAME@mark   NAME@goto     # versioned-value ops
why NAME                                # causal-debug query
```

Operator precedence (tightest first): unary / call / index / field · `* / %` ·
`+ -` · `<< >>` · `&` · `^` · `|` · `== != < <= > >=` · `&&` · `||` ·
`expr else default`. All binary operators are left-associative. See
[Operators](05_operators.md).

## Patterns

Used in `match`, `if let`, `while let`:

```
NAME::VARIANT(BIND, …)                  # enum variant, binding payloads
LITERAL                                 # 0  "text"  'c'  true  matched by value
LO..HI                                  # half-open Int/Char range: LO <= value < HI
NAME { FIELD, … }                       # struct destructure: binds the named fields (a subset is fine)
_                                       # wildcard
```

Every `match` arm accepts two composable extensions:

```
PATTERN | PATTERN | … => { … }          # or-pattern: any alternative matches (alternatives may not bind)
PATTERN if COND => { … }                # guard: pattern matched and COND true — else the match
                                        # keeps trying the arms below (a guarded arm never counts
                                        # toward exhaustiveness; guards run with the binds in scope)
```

Only the first pattern of an or-chain may bind a payload (`A::X(n) | A::Y =>` is rejected —
`n` would be unset when `A::Y` matches); a guard-free `Name { … }` struct arm is irrefutable
and covers like `_`.

```nori
enum Shape { Circle(Int), Rect(Int, Int), Dot, Line }
struct Point { x: Int, y: Int }
fn shape_kind(s: Shape) -> Str {
    match s {
        Shape::Circle(r) if r > 10 => { return "big" }
        Shape::Circle(r) => { return "circle " + r }
        Shape::Rect(w, h) if w == h => { return "square" }
        Shape::Rect(w, h) => { return "rect" }
        Shape::Dot | Shape::Line => { return "thin" }
    }
}
fn quadrant(p: Point) -> Str {
    match p {
        Point { x, y } if x > 0 && y > 0 => { return "Q1" }
        Point { x } if x < 0 => { return "left" }
        _ => { return "elsewhere" }
    }
}
fn grade(n: Int) -> Str {
    match n {
        0..60 => { return "F" }
        60..80 | 85 => { return "mid" }
        _ => { return "top" }
    }
}
fn main() -> Int {
    printl(shape_kind(Shape::Rect(4, 4)) + " " + quadrant(Point { x: 2, y: 3 }) + " " + grade(71))
    return 0
}
```

---

That completes the reference. Start over at the [index](README.md).
