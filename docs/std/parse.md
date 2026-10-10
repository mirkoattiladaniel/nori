# std/parse

```nori
import "std/parse" as parse
```

std/parse decimal: decimal text -> Float, correctly rounded.

Why this exists: Nori has no string-to-float in the language (`to_float(Str)` is not a parser).
The obvious accumulator (`v = v + digit * scale` with `scale = scale * 0.1`) is wrong, because
neither `0.1` nor its powers are representable in binary and the error compounds.
The next-obvious fix (accumulate an integer mantissa, scale once by an exact power of ten) is
Clinger's fast path: correct while the mantissa is below 2^53 and `|exp10| <= 22`, and approximate
everywhere else.

The algorithm here has no such window over the range a real literal reaches. A decimal literal is
exactly the rational `mant * 10^exp10`. Rewrite it as `num / den` with both sides exact 64-bit
integers, then produce the binary mantissa by long division (one bit at a time, in integers, with
the running remainder carried as the sticky bit) and round half-to-even on a 54th guard bit. No
floating-point arithmetic touches the value: the finished significand is assembled straight into
its IEEE-754 fields, so there is nothing for error to accumulate in. This is the construction
`strtod` uses, restricted to the range where numerator and denominator both fit in an Int.

What widens that range is splitting the power of ten: `10^k` is `5^k * 2^k`, and the `2^k` half is
free: it rides in the exponent field of the assembled result and is never an operation. So only
`5^k` has to fit in 63 bits, which roughly doubles the exponent reach compared with folding whole
powers of ten (`5^26` fits where `10^18` was the limit).

Overflow safety: `den = 5^k <= 5^26 < 2^62`, the remainder is always `< den`, and the only
operation on it is `rem * 2 < 2^63`. The numerator stays below `2^63` because a fold only happens
while `m <= (2^63-1)/5`. The working significand never exceeds 2^54.

Where it is not exact: all four cases are reported rather than silent (`ParsedFloat.exact == false`):
  · more than 18 significant digits: the tail is dropped (its non-zero-ness is kept as a sticky
    hint, so the value is at worst about one ulp out, but a tie could round the wrong way);
  · a net power of ten too large for any `5^k` to absorb, beyond roughly `10^±26` after the
    mantissa is normalised. The residual is then applied in Float in chunks of `10^22` (the largest
    power of ten that is still an exact double), one rounding per chunk;
  · subnormal results, which are reached by halving a normal value;
  · overflow to infinity, which is not the rounded value of anything.

  import "std/parse" as parse
  let r = parse::parse_float("0.25882354378700256")
  if r.ok && r.exact { use(r.value) }
### `struct ParsedFloat`

the result of a decimal->Float conversion.

`ok` is whether the text was a number at all; `exact` is whether the conversion is provably the
correctly rounded nearest double (see the module header for the four cases where it is not);
`value` is the number; `used` is how many bytes were consumed; `error` explains an `ok == false`.
The `exact` flag is the point of the type: a parser that can say "I could not represent this
exactly" is more useful than one that silently cannot.

### `fn pow5_int(k: Int) -> Int`

5^k as an exact Int, for `0 <= k <= 26`; -1 outside. 26 is the limit that keeps `2 * 5^k` inside a
signed 64-bit integer, which is what the long division below does to the remainder every step.

### `fn float_from_ratio(num: Int, den: Int, exp2: Int, sticky_in: Bool) -> Float`

`(num / den) * 2^exp2`, correctly rounded (ties to even). Requires `num >= 0`, `0 < den <= 2^62`.

`sticky_in` folds in "digits were dropped below the numerator", so that a value which only *looks*
like an exact tie is still rounded away from the tie. The quotient is developed one binary digit at
a time in integer arithmetic; the surviving remainder is the sticky bit that decides the tie. The
`exp2` scaling is free: it is added to the exponent field of the assembled result, never applied
as an arithmetic operation, which is what lets the caller split `10^k` into `5^k * 2^k` and pay
integer range only for the `5^k`.

### `fn float_scale10(neg: Bool, mant: Int, exp10: Int, truncated: Bool) -> ParsedFloat`

`mant * 10^exp10` (negated when `neg`) as the nearest double, with `exact` saying whether it
provably is. Set `truncated` when significant digits below `mant` were dropped by the caller.

The decomposition: `10^k` is `5^k * 2^k`, and the `2^k` half costs nothing (it rides in the
exponent field). So only the `5^k` half has to fit in an Int, which roughly doubles the decimal
exponent range that stays exact compared with folding whole powers of ten. What is left over after
that (a decimal exponent too large for any `5^k` to absorb) is applied in Float, in chunks of
`10^22` (the largest power of ten that is still an exact double, so each chunk costs one rounding
instead of twenty-two), and `exact` is set false to say so.

### `fn scan_float(s: Str, start: Int) -> ParsedFloat`

read one JSON-shaped number (`-?digits[.digits][eE[+-]digits]`) out of `s` starting at `start`.

`used` is how many bytes it ate, so a caller driving a cursor can advance by it; `ok == false` with
`used == 0` means there was no number there at all. A leading `+` and a bare fraction (`.5`) are
accepted, which is more than JSON allows; reject those in the caller if the grammar is strict.

### `fn parse_float(s: Str) -> ParsedFloat`

the whole string as one number. Trailing text is an error; this is a parser, not a prefix scan.
This is the correctly-rounded replacement for the `to_float(Str)` the language does not have.


### `fn parser(inout toks: Vec<Token>, src: Str, path: Str) -> Parser`

a parser over `toks`, positioned at the first one. `src` is the text those tokens index into
and `path` is what diagnostics name; both are carried only so errors can point at a place.

## Parser

### `fn at_end(self) -> Bool`

past the last token?

### `fn peek_tok(self) -> view Token`

the current token (an EOF token positioned at end-of-input when past the last).

### `fn peek_kind(self) -> Int`

the current token's kind (TK_EOF at end).

### `fn peek2_kind(self) -> Int`

the next token's kind (one lookahead; TK_EOF at end).

### `fn advance_tok(inout self) -> Token`

consume and return the current token.

### `fn check(self, kind: Int) -> Bool`

is the current token of kind `kind`? (no consume)

### `fn accept(inout self, kind: Int) -> Bool`

consume the current token iff it is `kind`; return whether it was.

### `fn expect(inout self, kind: Int, what: Str) -> Bool`

consume `kind` or record an "expected <what>" error; return whether it matched.

### `fn error(inout self, msg: Str)`

record a diagnostic at the current token's position.

### `fn recover(inout self, sync_kind: Int)`

error recovery: skip tokens up to (not past) the next `sync_kind`, or end-of-input.


### `fn pratt_expr<G: PrattGrammar>(g: G, inout ps: Parser, min_bp: Int) -> Int`

precedence-climbing driver: parse an expression binding tighter than `min_bp` (start at 0). Nodes are
opaque Ints (your AST handle or an evaluated value; the driver never inspects them).


std/parse: a parsing toolkit, built in layers so you write a fast hand-written parser on
shared infrastructure instead of re-rolling a scanner/positions/diagnostics every time:
  1. Scanner: a byte cursor over a string + char classes + source positions/diagnostics.
  2. Token: a { kind, span } token, consumed by the parser layer.
  3. Parser: recursive-descent helpers: peek/check/expect/accept + error recovery.
  4. Pratt: a precedence-climbing expression driver over a PrattGrammar.
Not included: parser combinators (a closure call per byte isn't inlined in Nori → slow) or a
grammar generator. You keep your recursive descent; this removes the boilerplate under it.
  import "std/parse" as parse
  var s = parse::scanner(src)
  while !s.at_end() { ... s.peek() ... s.advance() ... }
### `fn scanner(src: Str) -> Scanner`

a scanner positioned at the start of `src`.

### `fn span(start: Int, end: Int) -> Span`

a span covering the half-open byte range `[start, end)`.

## Scanner

### `fn at_end(self) -> Bool`

are we at (or past) the end of the input?

### `fn peek(self) -> Int`

the current byte as an Int, or -1 at end. (Bytes, not Char, so end can be -1 on the hot path.)

### `fn peek_at(self, k: Int) -> Int`

the byte `k` ahead (0 = current), or -1 past the end.

### `fn at(self) -> Int`

current byte offset.

### `fn advance(inout self) -> Int`

consume and return the current byte (-1 at end, cursor unchanged).

### `fn seek(inout self, p: Int)`

jump the cursor to an absolute offset (clamped to [0, len]).

### `fn eat(inout self, c: Int) -> Bool`

consume `c` if it is the current byte; return whether it was.

### `fn looking_at(self, lit: Str) -> Bool`

does the input at the cursor start with `lit`?

### `fn eat_str(inout self, lit: Str) -> Bool`

consume `lit` if the input starts with it here; return whether it did.

### `fn find(self, needle: Str) -> Int`

next index of `needle` at/after the cursor, or -1. (Cursor unchanged.)

### `fn find_ci(self, needle: Str) -> Int`

case-insensitive `find` (ASCII); next index of `needle` at/after the cursor, or -1. Cursor unchanged.

### `fn text(inout self, sp: Span) -> Str`

the source text of a span taken from this scanner (no need to thread the source around).

### `fn to_end(inout self)`

move the cursor to end-of-input.

### `fn skip_ws(inout self)`

skip spaces/tabs/newlines/CR/FF.

### `fn skip_line(inout self)`

skip to (but not past) the next newline.

### `fn take_ident(inout self) -> Span`

consume an identifier `[A-Za-z_][A-Za-z0-9_]*` and return its span (empty span if none here).

### `fn take_digits(inout self) -> Span`

consume a run of decimal digits and return its span.

### `fn take_until(inout self, c: Int) -> Span`

consume up to (not including) byte `c` (or end); return the span consumed.

### `fn take_while(inout self, pred: Fn(Int)->Bool) -> Span`

consume while `pred(byte)` holds; return the span. (Closure per byte; use the specialized
`take_ident`/`take_digits` on hot paths; this is for one-off custom classes.)

### `fn span_text(src: Str, sp: Span) -> Str`

the source text of a span.

### `fn span_len(sp: Span) -> Int`

how many bytes the span covers.

### `fn span_empty(sp: Span) -> Bool`

does the span cover nothing? (True for a reversed one, which is how a failed scan reports.)

### `fn is_digit(c: Int) -> Bool`

`0`-`9`.

### `fn is_lower(c: Int) -> Bool`

`a`-`z`.

### `fn is_upper(c: Int) -> Bool`

`A`-`Z`.

### `fn is_alpha(c: Int) -> Bool`

an ASCII letter, either case.

### `fn is_alnum(c: Int) -> Bool`

a letter or a digit.

### `fn is_space(c: Int) -> Bool`

space, tab, LF, CR or FF. (Not the Unicode spaces; this layer is bytes.)

### `fn is_hex(c: Int) -> Bool`

a hex digit, either case.

### `fn is_ident_start(c: Int) -> Bool`

may `c` start an identifier: a letter or `_`.

### `fn is_ident_cont(c: Int) -> Bool`

may `c` continue an identifier: a letter, digit or `_`.

### `fn to_lower(c: Int) -> Int`

ASCII-lowercase one byte, leaving anything else alone.

### `fn to_upper(c: Int) -> Int`

ASCII-uppercase one byte, leaving anything else alone.

### `fn hex_val(c: Int) -> Int`

the value of a hex digit, or -1 if `c` is not one, so a caller can test and convert at once.

### `fn line_col(src: Str, offset: Int) -> LineCol`

1-based line + column of a byte offset in `src`.

### `fn format_error(src: Str, path: Str, offset: Int, msg: Str, hint: Str) -> Str`

a caret-style diagnostic: `▲ msg\n  path:line:col\n    <source line>\n    <spaces>^\n  hint`.


### `fn token(kind: Int, sp: Span) -> Token`

a token of `kind` covering `sp`.

### `fn token_at(kind: Int, start: Int, end: Int) -> Token`

a token of `kind` covering `[start, end)`, for a lexer that already has the offsets.

### `fn tok_kind(t: Token) -> Int`

the token's kind: your own Int, or `TK_EOF`.

### `fn tok_span(t: Token) -> Span`

the token's span.

### `fn tok_text(t: Token, src: Str) -> Str`

the token's source text. Tokens store offsets, not text, so this is where the slice happens.

### `fn eof_token(offset: Int) -> Token`

an empty `TK_EOF` token at `offset`, so "past the end" still has a position to report errors at.


