# std/rand

```nori
import "std/rand" as rand
```

std/rand: a fast pseudo-random number generator: xoshiro256** seeded via splitmix64.
All state is held in an `Rng` struct (four 64-bit words); each draw advances the state in place.
Integers wrap mod 2^64 (two's complement).

  import "std/rand" as rand
  var r = rand::seeded(12345)       // deterministic: same seed -> same sequence
  var x = r.next_u64()              // a raw 64-bit value
  var d = r.below(6)                // uniform in [0, 6)
  var n = r.between(10, 20)         // uniform in [10, 20)
  var f = r.float01()               // a Float in [0, 1)
  var b = r.flip()                  // a coin flip
  r.shuffle(v)                      // Fisher-Yates shuffle a Vec<Int> in place
  var e = r.pick(v)                 // a random element of a non-empty Vec<Int>
For a clock-seeded (non-reproducible) generator use `rand::new()`.
### `struct Rng`

a xoshiro256** generator: four 64-bit state words. Construct via `seeded` or `new`.

### `fn rng_rotl(x: Int, k: Int) -> Int`

rotate the 64-bit value `x` left by `k` bits.

### `fn rng_splitmix(inout state: Vec<Int>) -> Int`

one step of splitmix64 over the mutable seed cell held in a 1-element Vec.
splitmix64 is defined on wraparound: the golden-ratio increment and the two mixing multiplies all
overflow 64 bits, so they use `wrapping_*`. That keeps the sequence bit-identical
while letting a build with `--overflow-checks` trap on accidental overflows elsewhere.

### `fn seeded(seed: Int) -> Rng`

a generator deterministically seeded from `seed`: the four state words come from successive
splitmix64 outputs, so the same seed always yields the same sequence.

### `fn new() -> Rng`

a generator seeded from the system clock (not reproducible across runs).

## Rng

### `fn next_u64(inout self) -> Int`

the next raw 64-bit value; the full state advances.
The `* 5` / `* 9` scrambler is defined on wraparound; like splitmix above it uses
`wrapping_mul`, so the sequence stays bit-identical while a `--overflow-checks` build keeps
trapping on accidental overflows elsewhere.

### `fn next_nonneg(inout self) -> Int`

the next raw value forced non-negative (sign bit cleared), in [0, 2^63).

### `fn below(inout self, n: Int) -> Int`

a uniform integer in [0, n); returns 0 if n <= 0.

### `fn between(inout self, lo: Int, hi: Int) -> Int`

a uniform integer in [lo, hi); returns lo if hi <= lo.

### `fn float01(inout self) -> Float`

a Float uniformly in [0, 1), built from the top 53 random bits.

### `fn flip(inout self) -> Bool`

a fair coin flip.

### `fn shuffle(inout self, inout v: Vec<Int>) -> Int`

shuffle `v` (a Vec<Int>) uniformly in place via Fisher-Yates; returns 0.

### `fn pick(inout self, v: Vec<Int>) -> Int`

a random element of the non-empty Vec<Int> `v`.


