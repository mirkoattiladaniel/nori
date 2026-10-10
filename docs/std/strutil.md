# std/strutil

```nori
import "std/strutil" as strutil
```

std/strutil: string helpers built on the string builtins (slen/char_at/substr/chr/eq_str/`+`).
Strings are immutable; every transforming helper returns a fresh Str.

  import "std/strutil" as s
  s::split("a,b,c", ",")        // Vec<Str> ["a","b","c"]
  s::join(parts, "-")           // "a-b-c"
  s::trim("  hi \n")            // "hi"
  s::replace("aXbXc", "X", "_") // "a_b_c"
### `fn match_at(s: Str, p: Str, at: Int) -> Bool`

does `p` occur in `s` exactly at index `at`?

### `fn starts_with(s: Str, p: Str) -> Bool`

does `s` begin with prefix `p`?

### `fn ends_with(s: Str, p: Str) -> Bool`

does `s` end with suffix `p`?

### `fn index_of(s: Str, sub: Str) -> Int`

first index of `sub` in `s`, or -1. Empty needle matches at 0.

### `fn contains(s: Str, sub: Str) -> Bool`

does substring `sub` occur anywhere in `s`?

### `fn count_char(s: Str, c: Byte) -> Int`

number of times character `c` appears in `s`.

### `fn is_space(c: Byte) -> Bool`

is `c` whitespace (space, tab, newline, or carriage return)?

### `fn to_upper(sink s: Str) -> Str`

ASCII uppercase of `s` (non-letters pass through unchanged). `sink` + in-place, like to_lower.

### `fn to_lower(sink s: Str) -> Str`

ASCII lowercase of `s` (non-letters pass through unchanged).

Takes its argument by `sink` and rewrites the bytes in place, so a case fold costs no allocation:
building a second string is proportional to the input, which dominates on large text. Callers that
still need the original spell `to_lower(copy s)`.

### `fn trim(s: Str) -> Str`

strip leading/trailing whitespace (space, tab, newline, carriage return).

### `fn repeat(s: Str, n: Int) -> Str`

`s` concatenated `n` times.

### `fn replace(s: Str, from: Str, to: Str) -> Str`

replace every (non-overlapping) occurrence of `from` with `to`.

### `fn split(s: Str, sep: Str) -> Vec<Str>`

split on a (non-empty) separator into a Vec<Str>; an empty separator yields the whole string.

### `fn join(parts: Vec<Str>, sep: Str) -> Str`

join the parts with `sep` between each.


