# std/regex

```nori
import "std/regex" as regex
```

std/regex: a small backtracking regular-expression engine. The pattern is compiled once into a
node table (parallel Vec<Int> fields inside `Regex`), then matched by recursive backtracking.
Worst-case backtracking is exponential in the input length for pathological patterns (e.g.
`(a*)*b` on a long run of `a`s), so keep patterns simple on untrusted input.

  import "std/regex" as re
  var r = re::compile("^[a-z]+@[a-z]+\.[a-z]+$")
  r.full("a@b.com")     // true   (whole string must match)
  r.test("see a@b.com") // true   (matches somewhere; respects ^ / $)
  r.find("xx cat")      // 3      (start index of first match, or -1)
  re::matches("cat|dog", "hotdog")   // true

Supported: literal chars; `.` (any char except newline); char classes `[...]` / negated `[^...]`
with ranges (`a-z`, `0-9`); escapes `\d \D \w \W \s \S` and escaped metachars (`\. \* \( \\` …);
greedy quantifiers `* + ?`; anchors `^` and `$`; alternation `|`; grouping `( … )` (non-capturing,
for precedence / quantifier scope).

Not supported: capturing groups / backreferences (`\1`); lazy quantifiers (`*?`); counted
repetition (`{m,n}`); word boundaries (`\b`); inline flags. Groups are non-capturing.
### `struct Regex`

a compiled regular expression. The node table is stored as parallel Int Vecs; `root` is the index
of the root node (or -1 if the pattern failed to parse). Character-class accepted sets live in
`cls_lo`/`cls_hi`/`cls_neg`, addressed via a per-node `cstart`/`cend` range into those Vecs.

### `fn re_add(inout r: Regex, kind: Int, a: Int, b: Int) -> Int`

append a node with the given fields, returning its index.

### `fn re_peek(r: Regex) -> Int`

byte at the cursor, or -1 at end.

### `fn re_peek2(r: Regex) -> Int`

byte one past the cursor, or -1.

### `fn re_take(inout r: Regex) -> Int`

take the current byte and advance.

### `fn re_cls_range(inout r: Regex, lo: Int, hi: Int)`

append a [lo,hi] byte range to the active class' range list.

### `fn re_cls_escape(inout r: Regex, e: Int)`

emit the ranges for an escape class (`d w s` etc) into the active class range list.
`e` is the escape letter byte (e.g. ord('d')). Only positive ranges are added; the caller flips
`cneg` for the uppercase (negated) variants.

### `fn re_escape_negates(e: Int) -> Bool`

true if the escape letter byte is an uppercase (negated) class escape.

### `fn re_is_class_escape(e: Int) -> Bool`

true if `e` is one of the class-escape letters d/D/w/W/s/S.

### `fn re_parse_alt(inout r: Regex) -> Int`

parse a full alternation (lowest precedence). Returns a node index, or -1 on error.

### `fn re_parse_concat(inout r: Regex) -> Int`

parse a concatenation: a run of repeat-terms until '|', ')', or end.

### `fn re_parse_repeat(inout r: Regex) -> Int`

parse an atom followed by an optional greedy quantifier (`* + ?`).

### `fn re_parse_atom(inout r: Regex) -> Int`

parse a single atom: group, anchor, class, escape, '.', or literal char.

### `fn re_parse_escape_atom(inout r: Regex) -> Int`

parse a backslash escape used as an atom: either a class escape (\d \w \s …) → CLASS node, or an
escaped literal (\. \* \\ …) → CHAR node.

### `fn re_parse_class(inout r: Regex) -> Int`

parse a `[...]` / `[^...]` character class. Cursor is on the '['.

### `fn re_class_member(r: Regex, node: Int, ch: Int) -> Bool`

does byte `ch` belong to the class node `node`?

### `fn re_match_node(inout r: Regex, node: Int, s: Str, pos: Int, n: Int) -> Int`

try to match `node` against `s` starting at `pos`. Returns the position after a successful match,
or -1 on failure. `slen(s)` is `n`. Quantifiers/alternation backtrack by trying options in turn.
CONCAT threads the remainder: matching `left` then `right` from each position `left` can end at.

### `fn re_match_star(inout r: Regex, child: Int, s: Str, pos: Int, n: Int, min: Int) -> Int`

greedy repetition without a following continuation: match `child` as many times as possible, then
accept. `min` is 0 for STAR, 1 for PLUS. (Used only when there is no `cont`; CONCAT-aware matching
goes through re_match_seq for correct backtracking across the remainder.)

### `fn re_match_seq(inout r: Regex, node: Int, s: Str, pos: Int, n: Int, cont: Int) -> Int`

match `node` at `pos`, requiring `cont` to match the remainder afterwards. Returns final pos or -1.

### `fn re_match_seq_with(inout r: Regex, first: Int, second: Int, cont: Int, s: Str, pos: Int, n: Int) -> Int`

match `first` with a continuation of "match `second`, then `cont`". This expresses the chained
continuation needed for nested CONCAT without closures: we wrap by re-dispatching.

### `fn re_link(inout r: Regex, second: Int, cont: Int) -> Int`

build (or reuse) a CONCAT node linking `second` then `cont`, returning a node that means "match
second, then cont". If `cont` is -1, that is just `second`.

### `fn re_cont(inout r: Regex, cont: Int, s: Str, pos: Int, n: Int) -> Int`

run the continuation `cont` at `pos` (cont == -1 means accept here).

### `fn re_match_star_cont(inout r: Regex, child: Int, cont: Int, s: Str, pos: Int, n: Int, min: Int) -> Int`

greedy star/plus with a continuation: try the most repetitions of `child` that still let `cont`
match the remainder. Recursive: match one more `child`, recurse; on failure, try `cont` here.

### `fn re_match_star_then(inout r: Regex, child: Int, second: Int, cont: Int, s: Str, pos: Int, n: Int, min: Int) -> Int`

star/plus with a trailing `second` node then `cont` (the CONCAT case): greedily match `child`, and
the "rest" after the repetition is "match second, then cont".

### `fn re_star_rec(inout r: Regex, child: Int, rest: Int, restcont: Int, s: Str, pos: Int, n: Int, min: Int, count: Int) -> Int`

the shared greedy-with-backtracking recursion. `rest`/`restcont` describe what must match after the
repetition: if `rest` >= 0, run `re_match_seq(rest, …, restcont)`; if `rest` < 0, run `cont`-style
(`restcont` is the continuation, which may be -1 = accept). `count` is reps so far.

### `fn compile(pat: Str) -> Regex`

compile pattern `pat` into a `Regex`. On a malformed pattern, `ok` is false and the regex matches
nothing (`full`/`test`/`find` return false / -1).

## Regex

### `fn match_from(self, s: Str, start: Int) -> Int`

match starting exactly at index `start`; returns the end index after the match, or -1.

### `fn full(self, s: Str) -> Bool`

does the whole string match the pattern (anchored at both ends)?

### `fn test(self, s: Str) -> Bool`

does the pattern match somewhere in `s`? Tries each start index (a leading `^` anchor makes only
start 0 viable). Returns true on the first start that matches.

### `fn find(self, s: Str) -> Int`

the start index of the first (leftmost) match, or -1 if the pattern matches nowhere.

### `fn matches(pat: Str, s: Str) -> Bool`

convenience: compile `pat` and test whether it matches somewhere in `s`.


