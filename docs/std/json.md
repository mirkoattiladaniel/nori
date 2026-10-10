# std/json

```nori
import "std/json" as json
```

std/json: a JSON parser and serializer. A JSON document is a `Json` value tree (null / bool /
number / string / array / object). `parse` builds the tree; `stringify` renders it back; the
`get`/`at`/`as_*` accessors read it. Numbers are stored as Float. Malformed input parses to JsNull
(best-effort, non-trapping). Objects preserve insertion order.

  import "std/json" as json
  var v = json::parse(text)
  var name = json::as_str(json::field(v, "name"))
  var first = json::as_num(json::at(json::field(v, "scores"), 0))
  printl(json::stringify(v))
### `struct Entry`

one key/value member of a JSON object.

### `enum Json`

a JSON value: one of null, bool, number (Float), string, array, or object (ordered members).

### `fn jp_failed(pos: Vec<Int>) -> Bool`

did the parse that used this cursor fail?

### `fn jp_peek(s: Str, pos: Vec<Int>) -> Int`

the byte at the cursor, or -1 at end of input.

### `fn jp_take(s: Str, inout pos: Vec<Int>) -> Int`

the byte at the cursor, then advance.

### `fn jp_ws(s: Str, inout pos: Vec<Int>)`

skip JSON whitespace (space, tab, newline, carriage return).

### `fn jp_is_digit(c: Int) -> Bool`

true if `c` (a byte) is an ASCII digit.

### `fn jp_string(s: Str, inout pos: Vec<Int>) -> Str`

read one JSON string literal from `s` starting at the opening quote at `pos[0]`, and leave
`pos[0]` just past the closing quote. Escapes are resolved, `\uXXXX` pairs included; an
unpaired surrogate becomes U+FFFD rather than an error, so a scan never stalls on bad input.

### `fn jp_number(s: Str, inout pos: Vec<Int>) -> Float`

parse a JSON number into a Float (handles sign, fraction, and e/E exponent), correctly rounded.

The conversion itself lives in `std/parse` (`parse::scan_float`), because "decimal text -> nearest
double" is not a JSON question and every other reader needs the same answer. This function is only
the cursor plumbing.

Accumulating the fraction as `v + digit * scale` with `scale * 0.1` is not correct: 0.1 is not
representable, so the error compounds and most non-dyadic literals come out wrong in the last
bits (`0.75` would parse as 0.75000000000000016). Scaling an integer mantissa once by a power of
ten is not enough either for mantissas past 2^53 or net exponents past +-22. `parse::scan_float`
uses an exact integer long division, and is correctly rounded wherever it reports `exact`.

A malformed number leaves the cursor where it was and yields 0.0; `jp_value` only calls this when
the next byte is `-` or a digit, so that cannot happen through the normal entry points.

### `fn jp_lit(s: Str, inout pos: Vec<Int>, word: Str) -> Bool`

does the input match the literal `word` (e.g. "true") at the cursor? consumes it on success.

### `fn jp_value(s: Str, inout pos: Vec<Int>) -> Json`

parse any JSON value at the cursor (the core recursive routine).

### `fn jp_array(s: Str, inout pos: Vec<Int>) -> Json`

parse a JSON array (cursor on '[').

### `fn jp_object(s: Str, inout pos: Vec<Int>) -> Json`

parse a JSON object (cursor on '{').

### `fn parse(text: Str) -> Json`

Parse a JSON document, best-effort: what it could make of the input, with no way to ask whether
the input was valid. Use `try_parse` when that matters, which is most of the time.

This cannot report failure, and the failure does not always look like failure: `"@@@@"` gives
JsNull, which is also what the valid document `null` gives, and `"{"` gives an empty JsObj, which
is what `{}` gives. A caller cannot tell a document from a mistake.

### `fn try_parse(text: Str) -> Result<Json>`

Parse a JSON document, reporting malformed input as Err.

Checks both halves of "valid": that the parser met nothing it could not read, and that the
document ends where the value ends. Trailing junk (`{"a":1} oops`) is not a document with
something after it; it is not a document.

### `fn field(v: Json, key: Str) -> view Json`

the value for `key` in an object, or JsNull if absent / not an object.

### `fn at(v: Json, i: Int) -> view Json`

the i-th element of an array, or JsNull if out of range / not an array.

### `fn jlen(v: Json) -> Int`

the number of elements (array) or members (object); 0 for scalars.

### `fn as_str(v: Json) -> view Str`

the string value, or "" if not a string.

### `fn as_num(v: Json) -> Float`

the number value (Float), or 0.0 if not a number.

### `fn as_bool(v: Json) -> Bool`

the bool value, or false if not a bool.

### `fn is_null(v: Json) -> Bool`

true if the value is JSON null (or absent, since `get`/`at` return JsNull).

### `fn entry_key(e: Entry) -> view Str`

the key of an object member.

### `fn entry_val(e: Entry) -> view Json`

the value of an object member.

### `fn js_escape(s: Str) -> Str`

escape a string for JSON output (quotes, backslash, and control chars).

### `fn stringify(v: Json) -> Str`

render a JSON value back to a compact JSON string.

Numbers go through `std/fmt`'s `float_json`, i.e. the shortest decimal that reads back as the same
double, not the language's `"" + n`, which is a six-significant-digit `%g` and rendered
`0.25882354378700256` as `0.258824`. With the correctly-rounded reader on the other side, that
makes `parse -> stringify -> parse` lossless on the value and a fixed point on the text. An
integral number prints without a `.0` (JSON has one number type and so does `Json`), and the
three doubles JSON cannot hold (NaN and the two infinities) print as `null`.


