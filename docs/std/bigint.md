# std/bigint

```nori
import "std/bigint" as bigint
```

std/bigint: arbitrary-precision signed integers. Sign-magnitude with base-10^9 limbs
(little-endian), so products of two limbs fit in an i64 and decimal I/O is cheap. Values are immutable;
every operation returns a fresh Big.
  import "std/bigint" as big
  let a = big::parse("123456789012345678901234567890")
  big::to_string(big::mul(a, a))
  big::to_string(big::pow(big::from_int(2), 100))     // "1267650600228229401496703205376"
### `fn zero() -> Big`

the additive identity.

### `fn is_zero(b: Big) -> Bool`

is `b` zero?

### `fn from_int(n: Int) -> Big`

a Big from a machine integer.

### `fn neg(b: Big) -> Big`

the negation of `b`.

### `fn cmp(a: Big, b: Big) -> Int`

signed comparison: -1 if a<b, 0 if equal, 1 if a>b.

### `fn add(a: Big, b: Big) -> Big`

a + b.

### `fn sub(a: Big, b: Big) -> Big`

a - b.

### `fn mul(a: Big, b: Big) -> Big`

a * b.

### `fn divmod(a: Big, b: Big) -> DivMod`

truncated division: q = a/b (toward zero), r = a - q*b (sign of a). Dividing by zero traps, as
`Int` division does.

### `fn div(a: Big, b: Big) -> Big`

a / b (truncated toward zero).

### `fn rem(a: Big, b: Big) -> Big`

a mod b (sign of a).

### `fn pow(b: Big, e: Int) -> Big`

base raised to a non-negative machine-int exponent (binary exponentiation).

### `fn modpow(b: Big, exp: Big, m: Big) -> Big`

base^exp mod m, for a non-negative `exp` and a positive `m`: the operation RSA verification
is, and the one `pow` cannot stand in for. `pow` takes a machine-int exponent and never
reduces, so it computes the full number: for RSA that is a value with more digits than the
machine has memory. Reducing at every step is the whole difference.

Zero when `m` is not positive or `exp` is negative. A negative exponent would be a modular
inverse, which is a different algorithm and is not implemented; returning zero rather than
guessing keeps a caller from mistaking one for the other.

The cost is one modular reduction per exponent bit, and reduction here is long division, so
this suits public exponents (the 17 bits of 65537) and not private ones. A private-key
operation would need a different representation (power-of-two limbs, Montgomery form) and,
more importantly, blinding: the square-and-multiply below branches on exponent bits and its
timing says which they were. That is harmless for a public exponent, where the bits are
published, and disqualifying for a secret one.

### `fn from_bytes_be(s: Str) -> Big`

A big-endian unsigned byte string as a Big. This is how a key, a modulus or a signature arrives
from a certificate: as bytes, not as decimal.

### `fn to_bytes_be(b: Big, n: Int) -> Str`

`b` as exactly `n` big-endian bytes, zero-padded on the left. "" if it does not fit in `n`, or
if `b` is negative: a modulus or a signature that will not fit the width it is supposed to
have is malformed, and truncating it would turn that into a value.

### `fn parse(s: Str) -> Big`

parse a decimal string (optional leading `-`); non-digits are ignored.

### `fn to_string(b: Big) -> Str`

decimal string form. Not `to_str`: that is the built-in every value already answers to, and a
top-level function wearing a built-in's name is one nothing can call.


