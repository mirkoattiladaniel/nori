# std/check

```nori
import "std/check" as check
```

std/check: property generators, shrinkers, and value renderers for `law` fuzzing.

You do not normally import this by hand. `noric --fuzz-laws FILE` (or `roll law`) generates a harness
that imports this module, draws random + adversarial inputs for every `law`, calls the law, and on
any input that makes the law return false **shrinks** it to a minimal failing case using the
`shrink_*` candidate lists here. Generators are biased toward the boundary values
(0, ±1, empty, all-equal, ascending) that curated tests miss.

Supported `law` parameter types: Int, Float, Bool, Char, Str and Vec of each.
### `fn gen_int(inout r: rand::Rng, iter: Int) -> Int`

a random Int: often a boundary value (0, ±1, ±2), otherwise magnitude grows with `iter`.

### `fn gen_float(inout r: rand::Rng, iter: Int) -> Float`

a random Float: often 0/±1, otherwise uniform in [-bound, bound] with bound growing with `iter`.

### `fn gen_bool(inout r: rand::Rng, iter: Int) -> Bool`

a fair coin.

### `fn gen_char(inout r: rand::Rng, iter: Int) -> Byte`

a Char: often 'a'/'A'/'0'/space, otherwise a random printable ASCII char.

### `fn gen_str(inout r: rand::Rng, iter: Int) -> Str`

a Str: sometimes empty, otherwise a short run of generated chars (length grows with `iter`).

### `fn gen_veci(inout r: rand::Rng, iter: Int) -> Vec<Int>`

a Vec<Int>: empty, all-equal, ascending 0..n, or random elements (length grows with `iter`).

### `fn gen_vecf(inout r: rand::Rng, iter: Int) -> Vec<Float>`

a Vec<Float> of random elements (length grows with `iter`; sometimes empty).

### `fn gen_vecb(inout r: rand::Rng, iter: Int) -> Vec<Bool>`

a Vec<Bool> of random elements (length grows with `iter`; sometimes empty).

### `fn gen_vecs(inout r: rand::Rng, iter: Int) -> Vec<Str>`

a Vec<Str> of random elements (length grows with `iter`; sometimes empty).

### `fn shrink_int(x: Int) -> Vec<Int>`

simpler Ints than `x`: 0, then halfway to 0, then one step toward 0.

### `fn shrink_float(x: Float) -> Vec<Float>`

simpler Floats than `x`: 0.0, then half.

### `fn shrink_bool(b: Bool) -> Vec<Bool>`

the simpler Bool: only `false` is simpler than `true`.

### `fn shrink_char(c: Byte) -> Vec<Byte>`

simpler Chars than `c`: 'a' (the canonical simplest char).

### `fn shrink_str(s: Str) -> Vec<Str>`

simpler Strs than `s`: empty, first half, drop-last-char.

### `fn shrink_veci(v: Vec<Int>) -> Vec<Vec<Int>>`

simpler Vec<Int>s than `v`: empty, halves, and each single-element deletion.

### `fn shrink_vecf(v: Vec<Float>) -> Vec<Vec<Float>>`

simpler Vec<Float>s than `v`: empty, halves, and each single-element deletion.

### `fn shrink_vecb(v: Vec<Bool>) -> Vec<Vec<Bool>>`

simpler Vec<Bool>s than `v`: empty, then each single-element deletion.

### `fn shrink_vecs(v: Vec<Str>) -> Vec<Vec<Str>>`

simpler Vec<Str>s than `v`: empty, then each single-element deletion.

### `fn show_int(x: Int) -> Str`

render an `Int` for a counterexample line.

### `fn show_float(x: Float) -> Str`

render a `Float` for a counterexample line.

### `fn show_bool(x: Bool) -> Str`

render a `Bool` as `true` / `false`.

### `fn show_char(x: Byte) -> Str`

render a `Byte` in single quotes, the way it is written in source.

### `fn show_str(x: Str) -> Str`

render a `Str` in double quotes, so an empty or space-only counterexample is visible.

### `fn show_veci(v: Vec<Int>) -> Str`

render a `Vec<Int>` as `[1, 2, 3]`.

### `fn show_vecf(v: Vec<Float>) -> Str`

render a `Vec<Float>` as `[1.5, 2.0]`.

### `fn show_vecb(v: Vec<Bool>) -> Str`

render a `Vec<Bool>` as `[true, false]`.

### `fn show_vecs(v: Vec<Str>) -> Str`

render a `Vec<Str>` with each element quoted: `["a", "b"]`.


