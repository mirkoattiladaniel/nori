# 3. Values and Types

Nori is statically typed. Every value has a type known at compile time. This
chapter describes the built-in types and their literals.

## The value model

At runtime, every value occupies one 64-bit slot. `Int`, `Bool`, `Char`, and all
handle-based values (`Str`, `Vec`, `Map`, structs, enums, `Task`, …) are a single
`i64`; `Float` is an `f64`. A handle names a record the runtime owns, so copying a
handle gives a second name for the *same* record. Nori makes that safe by
requiring you to say so: a bare second name is a **view**, which reads but cannot
write, so a mutation through it is a compile error. Say
[`inout`](15_memory_model.md) to share it or [`copy`](10_structs.md) to take your
own. `Str` is immutable, so none of this is observable for strings.

This uniform representation is why generics are cheap (a type parameter is just an
`i64` at runtime) and why the type system, not runtime tags, is responsible for
knowing what a slot means.

## Primitive types

### `Int`

A 64-bit signed integer. The default type of an integer literal.

```nori
let a = 42
let b = 0xFF        // hexadecimal -> 255
let c = 0b1010      // binary      -> 10
let d = 0 - 1       // -1  (there is no negative literal; negate with `-`)
```

Integer division truncates toward zero, and `%` is the remainder:

```nori
17 / 5      // 3
17 % 5      // 2
```

### Overflow

`Int` is two's complement, so `+`, `-` and `*` **wrap** at 64 bits by default: one
past `9223372036854775807` is `-9223372036854775808`.

Because a silent wrap is a bug far more often than it is intent, a build can ask for
it to be caught instead. Under `--overflow-checks` a signed `+`, `-` or `*` that
overflows **traps**, naming the operation and both operands:

```text
trap: integer overflow: 9223372036854775807 + 1 -- Int is 64-bit and wraps when
checks are off; say `wrapping_add/sub/mul` if you want the wrap, …
```

`roll` turns it on from a build profile, and `roll init` writes it into the dev
profile, so `roll build` catches overflow while `roll build --release` wraps:

```toml
[profile.dev]
overflow-checks = true
```

When wrapping *is* what you mean — a hash, a PRNG, a checksum — say so, and the
code keeps working under either setting:

| Builtin | Meaning |
|---|---|
| `wrapping_add/sub/mul(a, b)` | wrap at 64 bits, never trap |
| `saturating_add/sub/mul(a, b)` | clamp to the `Int` range instead of wrapping |
| `add_overflows/sub_overflows/mul_overflows(a, b)` | `Bool`: would this operation overflow? |

### Counting bits

Three builtins answer the questions bit-level code actually asks. Each is one
instruction on every target Nori emits for, and each is **total**: zero has an
answer rather than a poison value, so no caller has to special-case it.

| Builtin | Meaning |
|---|---|
| `trailing_zeros(x)` | zeros below the lowest set bit; `64` for `0` |
| `leading_zeros(x)` | zeros above the highest set bit; `64` for `0` |
| `count_ones(x)` | how many bits are set |

```nori,excerpt
trailing_zeros(8)        // 3
trailing_zeros(0)        // 64 — defined, not undefined
leading_zeros(1)         // 63
count_ones(255)          // 8
64 - leading_zeros(v)    // v's bit width: 12 for 4080
```

The shape these exist for is finding where two byte strings first differ:
`trailing_zeros(a ^ b) / 8` is the number of equal low bytes, in one instruction
rather than a search.

```nori
let mx = 9223372036854775807
wrapping_add(mx, 1)        // -9223372036854775808 — stated, so never a trap
saturating_add(mx, 1)      //  9223372036854775807 — clamped
add_overflows(mx, 1)       //  true
```

This is why `std/rand`'s splitmix64 uses `wrapping_mul`: the algorithm is *defined*
on wraparound, so it says so and stays trap-free in a checked build.

Two notes. Division and remainder cannot overflow except `INT_MIN / -1`, which is
covered by the same check. The **sized** integer types (`I8`…`U64`) always wrap at
their own width (they exist for layout and interop, where wrapping is the point),
and unsigned operands are never checked.

### `Float`

A 64-bit IEEE-754 double. A float **literal must contain a decimal point**: `3`
is an `Int`, `3.0` is a `Float`. `Int` and `Float` do **not** mix implicitly; you
convert explicitly.

```nori
let x = 3.0
let y = x * 2.0         // 6.0
let n = 3
// let bad = x + n      // ERROR: cannot mix Float and Int
let ok = x + to_float(n)
let back = to_int(x)    // truncates to Int

// An annotation does not convert. It says what the binding is, not what to do
// with the value, so the same rule holds at a `let` as at an operator.
// let f: Float = n     // ERROR: cannot initialise `f`, which is Float, with Int
let f: Float = to_float(n)
```

This strictness is deliberate: it removes a class of silent-rounding bugs and
keeps numeric code's intent explicit.

### `Bool`

`true` or `false`. Produced by comparisons and the logical operators; required by
`if`/`while` conditions.

```nori
let ready = true
let bigger = 5 > 3          // true
```

### `Byte` and `Char`

Text has two honest views, and Nori gives each its own type.

**`Byte`** is one raw byte of a string: what a `'x'` literal denotes and what
indexing yields (`s.at(i)`). Bytes are ordered (`c >= 'a' && c <= 'z'`) but are
**not** `Int`s: convert with `ord` (Byte → Int) and `byte` (Int → Byte); `chr`
is `byte`'s historical alias.

**`Char`** is a Unicode **scalar**; one character, whatever its script. Build
one with `uchr(codepoint)`; it prints as the character it is (UTF-8 encoded on
demand). Iterate a string's characters with `std/unicode`'s `chars(s)`, and its
raw bytes with `std/iter`'s `bytes(s)`.

`Byte` and `Char` never mix: comparing them is a build error, because a byte of
a multi-byte character is not that character.

```nori
import "std/unicode" as uni

fn main() -> Int {
    let s = "héllo"
    printl(s.len() + " bytes, " + uni::count(s) + " characters")   // 6 bytes, 5 characters
    let b = s.at(0)             // Byte 'h'
    printl(ord(b))              // 104
    let e = uchr(233)           // Char 'é'
    printl(e)                   // é   (UTF-8 encoded when rendered)
    var cs = uni::chars(s)
    var last = uchr(0)
    foreach c in cs { last = c }
    printl(last)                // o
    return 0
}
```

Indexing a string yields a `Byte` (see [Strings](06_strings_and_chars.md)); for
characters, iterate with `chars`.

### `Str`

An immutable string of bytes. String literals use double quotes and support
escapes and interpolation (below). Strings are compared **by content** with `==`
and `!=`, and concatenated with `+`.

```nori
let greeting = "hello"
let who = "world"
let msg = greeting + ", " + who     // "hello, world"
let same = (greeting == "hello")    // true — content comparison
let n = msg.len()                   // 12  (byte length)
```

`printl` writes a string followed by a newline; `print_str`/`concat`/`eq_str` do
not exist; `printl`, `+`, and `==` are the whole surface.

## Sized integers

For byte-level and interop work, Nori has fixed-width integer types:

```
I8  I16  I32  I64        signed
U8  U16  U32  U64        unsigned
```

These control storage width (e.g. in a `fill` array or a `cstruct` field). Values
still compute in 64-bit registers; the width governs how they are stored and how
they wrap. Ordinary arithmetic uses `Int`; the sized types are for layout and FFI.

The width an operation computes at follows from its operands:

| operands | computes at | example |
|---|---|---|
| two of one sized type | that type, wrapping at its width | `U8` 200 + `U8` 100 is 44 |
| a sized type and an integer literal | the sized type | `U8` 200 + 100 is 44 |
| a sized type and a non-literal `Int` | `Int`, no wrap | `Int` 400 + `U8` 200 is 600 |
| two different sized types | the wider; the unsigned one when equally wide | `U32` + `U8` wraps at 32 bits |

Storing a value into a sized variable, field or element wraps it to that width, so
`let b: U8 = t` keeps the low 8 bits of `t`. Unsigned comparison and division apply
when the operands compute at an unsigned type.

## Vector types (explicit SIMD)

A vector type is a fixed group of lanes that arithmetic applies to all at once —
one machine instruction for the whole group, where the scalar version needs one
per lane. It is spelled `<element>x<lanes>`:

```
U8x16  U16x8  U32x8  U64x4  I32x4  I64x2  F32x8  F64x4      … and so on
```

Any of the sized scalars above may be the element. The lane count is a power of
two, and the whole vector must be between 16 and 512 bits wide, so `U64x4` (256
bits) is a type and `U64x3` is not. A name that is *shaped* like a vector but is
not one is an error rather than an unknown type; it would otherwise be read as a
generic type parameter and compile as a plain scalar, delivering none of the speed
it looks like it asks for.

The **operators apply lanewise**: `+ - * & | ^ << >>` on integer vectors, and
`+ - * /` on float ones. Both sides must be the same vector type. A vector and a
scalar do not mix; there is no implicit splat, because `v * 2` reads as "twice
the vector" in some languages and "shift by two lanes" in others, and a silent
guess is exactly what an explicit vector type exists to prevent.

Values are made by naming the type (the compiler cannot infer it from an address
or a single scalar), and consumed by generic operations that read the type off
their argument:

| | |
|---|---|
| `u64x4(a, b, c, d)` | one value per lane |
| `u64x4_splat(x)` | every lane the same |
| `u64x4_load(addr)` | unaligned load from a raw address, `unsafe` |
| `vstore(addr, v)` | unaligned store, `unsafe` |
| `vlane(v, k)` | lane `k` as a scalar |
| `vwith(v, k, x)` | a new vector, lane `k` replaced |
| `vperm(v, i0, …)` | permute lanes |
| `vshuffle(a, b, i0, …)` | lanes chosen from `a` followed by `b` |
| `vrotr(v, n)` / `vrotl(v, n)` | lanewise bit rotate (integer vectors) |
| `vsum(v)` | horizontal sum, as a scalar |
| `vlanes(v)` | the lane count |

Lane indices are part of the instruction, not data, so they must be written as
literals. There is no lanewise comparison: comparing two vectors would produce one
answer per lane rather than a `Bool`, and there is no mask type yet.

```nori
fn mix(a: U64x4, b: U64x4) -> U64x4 {
    var x = a ^ b
    x = vrotr(x, 24)
    return x + b
}

fn main() {
    let a = u64x4(1, 2, 3, 4)
    let b = u64x4_splat(7)
    printl(to_str(vlane(a ^ b, 0)))       // 6
    printl(to_str(vsum(a)))               // 10
    printl(to_str(vlane(mix(a, b), 0)))   // 6597069766663
}
```

`mix` above compiles to three instructions on x86-64 with AVX (`vpxor`, `vprolq`,
`vpaddq`); the rotate is one instruction, not a shift/shift/or.

Vector types are for the cases the compiler cannot find on its own. Nori's
`--simd` flag already auto-vectorizes elementwise float loops, and LLVM
auto-vectorizes more; reach for an explicit vector type when the parallelism is
real but not elementwise; a block cipher round, a hash mix, a permutation
network. The trade is deliberate: an explicit vector type is either compiled as
vectors or reported as an error, whereas an auto-vectorizer that stops recognizing
your loop says nothing at all and simply runs slower.

## String literals: escapes and interpolation

Inside a `"…"` literal:

- **Escapes** use a backslash: `\n` (newline), `\t` (tab), `\"` (quote),
  `\\` (backslash), and the usual companions.
- **Interpolation**: `${ expr }` splices the value of `expr` into the string. It
  desugars to string concatenation, so `expr` may be any expression whose value
  can be rendered.

```nori
let n = 3
let name = "Ada"
printl("hi ${name}, n = ${n}")     // hi Ada, n = 3
printl("line1\nline2")
printl("2 + 2 = ${2 + 2}")         // 2 + 2 = 4
```

Interpolation is a zero-cost compile-time rewrite into `+` concatenation; there is
no reflection at runtime.

## Container types

### `Vec<T>`, growable list

An ordered, growable sequence of `T`. Create with `vec()`, grow with `push`,
read with indexing, measure with `len`.

```nori
var xs: Vec<Int> = vec()
xs.push(10)
xs.push(20)
let first = xs[0]        // 10
let n = xs.len()          // 2
```

### `Map<K, V>`, hash map

A map whose **keys are always `Str`** and whose values are `V`. Write it as
`Map<K, V>`; the key type is always written. Use `put`, `lookup`, `has`, `delete`,
and `keys`.

```nori
var ages: Map<Str, Int> = map()
ages.put("ada", 36)
let a = ages.get("ada")     // 36
let known = ages.has("bob")    // false
```

### Fixed arrays, `fill`

`fill(n, init)` allocates a fixed-size, contiguous array of `n` elements each set
to `init`. Unlike `Vec`, its length is fixed and its storage is a flat buffer
(ideal for numeric/data-parallel work). `fill2` makes a 2-D array.

```nori
var a = fill(4, 0)       // [0, 0, 0, 0]
a[2] = 9
var m = fill2(3, 3, 0)   // a 3x3 grid of zeros
m[1, 2] = 7              // 2-D arrays index with a comma: a[row, col]
```

### `Pool<T>`

A pool/arena of `T` records with stable handles, used where you want many
long-lived objects with cheap allocation. See [Collections](09_collections.md).

## Built-in generic and opaque types

These types come from language constructs; no import needed. They are opaque
handles you interact with through their operations, documented in
[Concurrency](16_concurrency.md):

| Type | Produced by |
|---|---|
| `Task<T>` | `spawn { … }` |
| `Channel<T>` | `channel()` |
| `Timed<T>` | `within N { … }` (no `else`) |
| `Deferred<T>` | `{ … } until { … }` |

### Prelude enums (`Option`, `Result`, `Json`)

`Option<T>` and `Result<T, E>` are the prelude: in scope in every program with no
import (`try` and `expr else d` are language syntax over them). `Result<T>` is
shorthand for `Result<T, Str>`; the omitted error type is a `Str` message.
Their helper functions (`mget`, `vget`, `lookup_or`, …) live in the standard library
(`std/result`), and `Json` in `std/json`; you `import` the module to use them.
Once imported they behave like *prelude* types: you name them **unqualified**
(`Option::Some`, not `result::Option::Some`) even when the module is imported
under an alias.

```nori
fn head(v: Vec<Int>) -> Option<Int> {
    if v.len() == 0 { return Option::None }
    return Option::Some(v[0])
}
```

See [Enums and Pattern Matching](11_enums_and_matching.md) and
[Error Handling](14_error_handling.md).

## User-defined types

Programs introduce their own types with `struct` (product types),
`enum` (sum types), and `trait` (interfaces). Generic types and functions are
parameterized with `<T>`. These are covered in [Structs](10_structs.md),
[Enums and Pattern Matching](11_enums_and_matching.md), and
[Traits and Generics](12_traits_and_generics.md).

## Conversions

Conversions are always explicit:

```nori,excerpt
to_str(x)        // any renderable value -> Str  (Int/Float/Bool/Char)
to_int(f)        // Float -> Int (truncate)
to_float(n)      // Int -> Float
ord(c)  chr(i)   // Char <-> Int
parse_int(s)       // Str -> Int;   traps if `s` is not an integer
parse_int_or(s, d) // Str -> Int,   or `d` if `s` is not an integer (never traps)
parse_float(s)       // Str -> Float; traps if `s` is not a number
parse_float_or(s, d) // Str -> Float, or `d` if `s` is not a number (never traps)
bits_of_f64(x)   // Float -> Int: its IEEE-754 bits, unchanged
f64_of_bits(b)   // Int -> Float: the double with these bits
bits_of_f32(x)   // F32 -> Int: its 32 bits, zero-extended
f32_of_bits(b)   // Int -> F32: the single with the low 32 bits of `b`
```

The four `bits` functions reinterpret rather than convert: no value changes, a NaN
keeps its payload, and each is one register move (no memory).

`to_int` and `to_float` convert **between numbers**. They do not read text, and
handing one a `Str` is refused at compile time with a pointer to the parser you
meant.

There are no implicit numeric conversions, no truthiness (only `Bool` is a
condition), and no implicit `to_str`; you ask for each conversion by name.

`parse_int` is for input you have already established is numeric; on anything else
it traps, naming the offending string. When the input can legitimately be
malformed, choose by what you need to do about it:

- **`parse_int_or(s, d)`**: a fallback is enough. One scan, no allocation.
- **`try_parse_int(s)`** (`std/result`): the caller needs to know *why*; yields
  `Result::Ok(n)` or `Result::Err(msg)`.

```nori
parse_int_or("42", 0 - 1)      // 42
parse_int_or("4x", 0 - 1)      // -1  — trailing junk is not a number
parse_int_or("", 0)            // 0
```

`parse_float` accepts an optional sign, a decimal point, and an exponent; the same
`_or` form applies.

```nori
parse_float("0.22")            // 0.22
parse_float("-12.5")           // -12.5
parse_float("2.5e-2")          // 0.025
parse_float_or("1.2.3", 0.0)   // 0.0  — two points is not a number
```

---

Next: [Variables and Bindings](04_variables.md), naming and mutating values.
