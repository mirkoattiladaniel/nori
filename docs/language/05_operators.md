# 5. Operators

## Arithmetic

`+ - * /` and `%` (remainder) on `Int` and `Float`. `Int` and `Float` never mix
implicitly (see [Values and Types](03_values_and_types.md)).

```nori
7 + 2        // 9
7 - 2        // 5
7 * 2        // 14
7 / 2        // 3    (Int: truncates toward zero)
7 % 2        // 1    (remainder)
7.0 / 2.0    // 3.5  (Float)
```

On `Int`, `+`, `-` and `*` wrap at 64 bits by default and **trap** under
`--overflow-checks`; `wrapping_*` / `saturating_*` / `*_overflows` state the intent
where wrapping is deliberate. See
[Overflow](03_values_and_types.md#overflow).

There is no negative literal; unary `-` negates: `-x`, or `0 - 1` for a constant.

## Comparison

`== != < <= > >=` produce a `Bool`. `==` and `!=` compare `Str` **by content**
(not identity) and also work on `Int`, `Float`, `Char`, and `Bool`.

```nori
3 < 5                 // true
"abc" == "abc"        // true — content comparison
'a' != 'b'            // true
```

## Logical

`&&` (and), `||` (or), `!` (not) on `Bool`. `&&` and `||` **short-circuit**; the
right operand is not evaluated if the left settles the result.

```nori,excerpt
ready && count > 0
!done || retry
```

Only `Bool` is a condition; there is no truthiness (a non-zero `Int` is not
"true").

## Bitwise

`& | ^ ~ << >>` on integers. `~` is unary complement; `>>` is a logical
(unsigned) shift.

```nori
6 & 3        // 2
6 | 1        // 7
6 ^ 3        // 5
~0           // -1
1 << 4       // 16
64 >> 2      // 16
```

These are operators, not function calls (older `band`/`bor`/`shl` builtins are
retired).

## Lanewise (vector operands)

When both operands are the same [vector type](03_values_and_types.md#vector-types-explicit-simd),
`+ - * & | ^ << >>` apply to every lane at once, and `/` does too on float
vectors. The result is a vector of the same type.

```nori
let a = u64x4(1, 2, 3, 4)
let b = u64x4_splat(7)
let c = a ^ b            // lane i is a[i] ^ b[i]
```

Two rules differ from the scalar operators. A vector and a scalar do **not** mix:
splat the scalar first (`a * u64x4_splat(2)`), and there is no lanewise
comparison, because one answer per lane is not a `Bool`.

## String concatenation

`+` on `Str` concatenates. Interpolation (`"${x}"`) is the ergonomic form and
lowers to the same concatenation.

```nori,excerpt
"foo" + "bar"            // "foobar"
"n = " + to_str(n)       // build strings with explicit to_str
```

## Precedence and associativity

All binary operators are **left-associative**. From **tightest** to **loosest**:

| Tier | Operators |
|---|---|
| 1 (tightest) | unary `-x` `!x` `~x`, calls, indexing `a[i]`, field `a.b` |
| 2 | `*` `/` `%` |
| 3 | `+` `-` |
| 4 | `<<` `>>` |
| 5 | `&` |
| 6 | `^` |
| 7 | `\|` |
| 8 | `==` `!=` `<` `<=` `>` `>=` |
| 9 | `&&` |
| 10 | `\|\|` |
| 11 (loosest) | `expr else default` |

So `a + b * c` is `a + (b * c)`, and — note — bitwise operators bind **tighter
than comparisons**: `a & b == c` parses as `(a & b) == c`. When mixing bitwise
and comparison or arithmetic and shifts, parenthesize for clarity.

The `expr else default` form at the bottom tier supplies a fallback for an
`Option`/`Result`-style value; see [Error Handling](14_error_handling.md).

Use parentheses `( … )` to group explicitly wherever the precedence isn't obvious
at a glance.

---

Next: [Strings and Characters](06_strings_and_chars.md).
