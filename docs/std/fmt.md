# std/fmt

```nori
import "std/fmt" as fmt
```

std/fmt: value formatting. Four pieces:
  * Display: a trait giving your own types a string form (primitives already stringify via `"" + x`).
  * format: positional `{}` template substitution.
  * padding / numeric helpers: lpad/rpad, zero-padded ints, hex.
  * floats: `float_shortest`, the shortest decimal that reads back as the same double. The
    language's own `"" + f` is a six-digit `%g` and loses the value; this does not. `std/json`'s
    `stringify` is built on it, which is what makes a JSON number round trip.

  import "std/fmt" as fmt
  struct Pt { x: Int, y: Int }
  impl fmt::Display for Pt { fn show(self) -> Str { return "(" + self.x + ", " + self.y + ")" } }
  fn main() -> Int {
    var p = Pt { x: 3, y: 4 }
    printl(fmt::display(p))                              // (3, 4)
    printl(fmt::format("{} -> {}", vec_str("a", "b")))  // a -> b  (caller stringifies args)
    printl(fmt::lpad("7", 4))                           // "   7"
    printl(fmt::pad_int(42, 5))                         // 00042
    printl(fmt::to_hex(255))                            // ff
    printl(fmt::float_shortest(0.1 + 0.2))              // 0.30000000000000004  ("" + f gives 0.3)
    printl(fmt::float_prec(3.141592653589793, 5))       // 3.1416
    return 0
  }
### `trait Display`

implement Display to give a type a string form; `display(x)` then works on any T: Display.

### `fn show(self) -> Str`

return this value's string form.

### `fn display<T: Display>(x: T) -> Str`

stringify any `T: Display` by calling its `show`.

### `fn padfill(c: Str, k: Int) -> Str`

`c` repeated `k` times (k <= 0 -> "").

### `fn format(tpl: Str, args: Vec<Str>) -> Str`

format(tpl, args): each `{}` is replaced by the next arg in order; `{{`/`}}` emit literal braces.
Args are pre-stringified Strs (use `"" + v` to stringify a value). A `{}` with no remaining arg is
left verbatim; surplus args are ignored.

### `fn vec_str(sink a: Str, sink b: Str) -> Vec<Str>`

a two-element Vec<Str> [a, b] (format-args builder).

### `fn vec_str3(sink a: Str, sink b: Str, sink c: Str) -> Vec<Str>`

a three-element Vec<Str> [a, b, c] (format-args builder).

### `fn lpad(s: Str, width: Int) -> Str`

pad with spaces to `width` (no truncation when already longer).

### `fn rpad(s: Str, width: Int) -> Str`

right-pad with spaces to `width` (no truncation when already longer).

### `fn pad_int(v: Int, width: Int) -> Str`

zero-pad an Int to at least `width` digits (a leading '-' is preserved and not counted).

### `fn to_hex(v: Int) -> Str`

lowercase hexadecimal of a non-negative Int (0 -> "0").

### `fn float_shortest(v: Float) -> Str`

the shortest decimal text that reads back as exactly `v`; the round-trip formatter.

`parse::parse_float(float_shortest(v)).value == v` holds bit for bit for every finite `v`, and no
shorter digit run does. Notation, integral values, negative zero and the non-finite spellings are
as the section header states. This is what you want whenever a Float has to survive text.

### `fn float_prec(v: Float, prec: Int) -> Str`

`v` at exactly `prec` significant digits (clamped to 1..17), with the same notation rules as
`float_shortest`. For display; use `float_shortest` when the text has to read back.

The digits are the nearest `prec`-digit decimal to `v`. Trailing zeros are kept (`float_prec(0.5,
4)` is `0.5000`) because a fixed precision is a request for that many digits.

### `fn float_json(v: Float) -> Str`

`float_shortest`, with JSON's spelling for the three values JSON cannot hold: NaN and both
infinities render as `null`. This is what `std/json`'s `stringify` uses.


