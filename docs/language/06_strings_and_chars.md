# 6. Strings and Characters

## `Str`

A `Str` is an immutable string of bytes. Literals use double quotes. Strings are
compared by content, concatenated with `+`, and built up with interpolation.

```nori
let s = "hello"
let t = s + ", world"          // "hello, world"
let same = (s == "hello")      // true — by content
```

Strings are immutable: operations that "modify" a string return a new one. There
is no in-place mutation of a `Str`.

### Length and indexing

```nori
let s = "héllo"
s.len()              // byte length (may exceed the character count for non-ASCII)
s.at(0)              // the byte at index 0, as a Byte
```

`.len()` is the length in **bytes**. `s.at(i)` returns the byte at index `i` as
a `Byte`. Indexing is bounds-checked; an out-of-range index traps. For
**characters** rather than bytes, see [Characters](#byte-and-char) below.

### Substrings

`s.slice(lo, hi)` returns the half-open byte range `[lo, hi)`, from `lo` up to
but not including `hi`.

```nori
let s = "abcdef"
s.slice(0, 3)      // "abc"
s.slice(2, 5)      // "cde"
s.slice(0, s.len())// the whole string
```

Method calls chain naturally:

```nori
let s = "abcdef"
let tail = s.slice(2, s.len())     // "cdef"
let n = s.slice(0, 3).len()        // 3
```

## Editing bytes in place: `s.set_at(i, b)`

`s.set_at(i, b)` writes the byte `b` at index `i`, mutating the string rather than
building a new one. The index is bounds-checked exactly like `s.at(i)`.

```nori
fn shout(sink s: Str) -> Str {
    var i = 0
    while i < s.len() {
        let c = s.at(i)
        if c >= 'a' && c <= 'z' { s.set_at(i, ord(c) - 32) }
        i = i + 1
    }
    return s
}
```

Note the `sink`. A transform that rewrites its input must **own** it, and `sink`
is how the caller says so: after `shout(t)`, `t` has been given away. A caller
that still needs the original passes `shout(copy t)`.

This matters for more than tidiness. Without it, every string transform has to
allocate a second string the size of the first, where C would just write
`buf[i] += 32` in place.

A string **literal** is interned and immortal (one record shared by every use of
that literal in the program), so it can never be written to. Passing one to a
`sink Str` parameter copies it on entry, so `shout("abc")` is safe and leaves
every other `"abc"` alone. Writing to a literal you reached some other way traps
rather than corrupting it.

Using a literal never allocates, not even the first time: its record is one of a
reserve of static records, and its bytes are the program's own. That is what lets
an interrupt handler pass one to a function (see [`@irq`](17_unsafe_and_ffi.md#irq)).
A literal a variable is *given* — `var s = "abc"`, an assignment, a `return` — is
the exception: the variable owns its value, so it gets a copy of its own.

## `Byte` and `Char`

A **`Byte`** is one raw byte of a string and its **own type**, distinct from
`Int`. A single-quoted literal is a `Byte`, and `s.at(i)` returns one.

```nori
let c = 'A'
ord('A')              // 65        — Byte -> Int
byte(97)              // 'a'       — Int -> Byte (`chr` is the historical alias)
byte(ord('a') + 1)    // 'b'
// 'a' + 1            // ERROR: Byte is not an Int
```

A **`Char`** is a Unicode **scalar**: a whole character in any script. Build one
with `uchr(codepoint)`; printing or concatenating it emits its UTF-8 bytes.

```nori
import "std/unicode" as uni
import "std/iter" as it

fn main() -> Int {
    let s = "héllo"
    printl(s.len())            // 6 — bytes
    printl(uni::count(s))      // 5 — characters
    var cs = uni::chars(s)     // lazy: Char values, decoded on demand
    var out = ""
    foreach c in cs { out = out + c + "|" }
    printl(out)                // h|é|l|l|o|
    var bs = it::bytes(s)      // lazy: the raw Byte view
    var n = 0
    foreach b in bs { n = n + 1 }
    printl(n)                  // 6
    return 0
}
```

`Byte` and `Char` never mix: comparing them is a build error, because one byte
of a multi-byte character is not that character. Byte-level code (parsers,
protocols) stays fast and says what it is; text that must respect scripts
iterates characters.

## Escapes and interpolation

Inside a `"…"` literal:

- **Escapes**: `\n`, `\t`, `\"`, `\\`, and companions.
- **Interpolation**: `${ expr }` splices a value into the string, lowering to
  `+` concatenation at compile time (no runtime reflection).

```nori
let name = "Ada"
let n = 3
printl("hi ${name}, n=${n}")       // hi Ada, n=3
printl("tab\tand\nnewline")
printl("sum = ${2 + 2}")           // sum = 4
```

A run of string literals joined by `+` is folded into one literal at compile
time, so a long constant may be written as many joined pieces without paying for
a concatenation between each pair every time the expression runs:

```nori
let banner = "usage: tool [options]\n"
    + "  -v   be louder\n"
    + "  -q   be quieter\n"                 // one literal, no concatenation at run time
```

Only literals fold; a piece that is a variable or a call splits the run, and
interpolation splices values in the same way it always does.

## Reading a file in at compile time

`include_str("path")` is the file's bytes, as a string literal, read while the program is
being compiled. There is no file beside the binary afterwards; the text is in it.

```nori,excerpt
let panel = include_str("panel.ui")       // beside this source file
let style = include_str("theme/app.style")
```

The path is resolved **beside the source file that names it**, not against the working
directory, so a module in `src/ui/` includes `src/ui/panel.ui` by writing `panel.ui`. It must
be written out in quotes: the file is read while this one compiles, so there is nothing to
compute a path from. For a path known only when the program runs, use `read_file`.

This is for a program that must **ship sealed**: a UI document and its stylesheet inside the
executable, with nothing alongside to lose or to be edited. A program that can find its own
files should read them at run time instead: the same text, and it can be changed without a
rebuild. `roll` notices a changed included file and recompiles, so it is not a stale-build
trap either way.

## Converting to and from strings

```nori
to_str(42)           // "42"      — Int / Float / Bool / Char render with to_str
to_str(3.14)         // "3.14"
to_str(true)         // "true"
parse_int("123")     // 123       — Str -> Int; traps on a non-integer
parse_int_or("x", 0) // 0         — Str -> Int with a fallback; never traps
```

There is no implicit `to_str`: building a message always names the conversion,
which is why you write `"n = " + to_str(n)` rather than `"n = " + n`.

`parse_int` traps on input that is not an integer, and the trap names the string it
was given. Use `parse_int_or(s, d)` when a fallback will do, or
`try_parse_int(s)` from [`std/result`](../std/result.md) when the caller needs
the reason. See [Conversions](03_values_and_types.md#conversions).

## Iterating over characters

Walk a string by index:

```nori
fn count_spaces(s: Str) -> Int {
    var n = 0
    var i = 0
    while i < s.len() {
        if s.at(i) == ' ' { n = n + 1 }
        i = i + 1
    }
    return n
}
```

For line splitting, building strings efficiently, and reading files as text, see
`std/io` (`io::lines`, `io::builder`, `io::read_lines`) in the
[standard library reference](../std/).

---

Next: [Control Flow](07_control_flow.md).
