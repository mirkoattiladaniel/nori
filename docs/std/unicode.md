# std/unicode

```nori
import "std/unicode" as unicode
```

std/unicode: UTF-8 between Nori byte strings and Unicode code points. A `Str` is raw bytes (each
`char_at` is one byte); this module reads/writes the multi-byte UTF-8 encoding on top of that.
  import "std/unicode" as utf8
  utf8::count("héllo")            // 5 code points (6 bytes)
  utf8::encode(0x20AC)            // the 3 UTF-8 bytes of "€"
  utf8::to_codepoints(s)         // Vec<Int> of code points
### `fn encode(cp: Int) -> Str`

encode one Unicode code point as its 1–4 UTF-8 bytes.

### `fn decode_at(s: Str, i: Int) -> CodePoint`

decode the code point starting at byte index `i`. Malformed input yields the raw lead byte advancing 1.

### `fn count(s: Str) -> Int`

number of Unicode code points in `s`.

### `fn to_codepoints(s: Str) -> Vec<Int>`

the code points of `s` as a Vec<Int>.

### `fn from_codepoints(cps: Vec<Int>) -> Str`

encode a sequence of code points as a UTF-8 byte string.

### `fn is_valid(s: Str) -> Bool`

is `s` well-formed UTF-8 (correct lead/continuation byte structure)?

### `fn width(cp: Int) -> Int`

terminal display width of code point `cp`: 0 for combining/zero-width, 2 for East-Asian wide/fullwidth,
else 1. Follows Unicode 17.0.0 (East Asian Width W/F => 2; general category Mn/Me/Cf => 0), via
binary search over packed range tables.

### `fn display_width(s: Str) -> Int`

total display width of `s` (sum of code-point widths), i.e. how many terminal columns it occupies.

### `struct Chars`

iterate a Str's characters (Unicode scalars): `var cs = uni::chars(s)  foreach c in cs { … }`.
For the raw byte view use `it::bytes(s)` (std/iter); bytes are what `s.at(i)` returns.

### `fn chars(s: Str) -> Chars`

a `Chars` iterator over `s`, for `foreach c in chars(s)`.

## Chars


