# std/iter

```nori
import "std/iter" as iter
```

std/iter: functional operations over Vec<T> driven by closures (`|x| ...`). Each takes a Vec plus one
or more `Fn` closures and returns a fresh Vec or a value; the input is never mutated. The Vec-returning
ops (vmap/vfilter/vsort) recover the concrete element type at the call site, so `vmap(words, ...)[i]`
is a Str. fold/sum accumulate into an Int.

Closure params default to Int; annotate the param for Str/Float elements (`|w: Str| ...`). Use `slen`
for string length (`len` is Vec-only).

  import "std/iter" as it
  let evens = it::vfilter(xs, |x| x % 2 == 0)
  let doubled = it::vmap(xs, |x| x * 2)
  let total = it::vfold(xs, 0, |acc, x| acc + x)
  let longish = it::vfilter(words, |w: Str| slen(w) > 4)
### `fn vmap<T>(v: Vec<T>, f: Fn(T) -> T) -> Vec<T>`

map: a new Vec with f applied to every element.

### `fn vfilter<T>(v: Vec<T>, pred: Fn(T) -> Bool) -> Vec<T>`

filter: a new Vec of the elements for which pred returns true.

### `fn vfold<T>(v: Vec<T>, init: Int, f: Fn(Int, T) -> Int) -> Int`

fold: thread an Int accumulator left-to-right through f(acc, elem).

### `fn vsum(v: Vec<Int>) -> Int`

sum of an Int Vec (vfold with +).

### `fn vcount<T>(v: Vec<T>, pred: Fn(T) -> Bool) -> Int`

count elements satisfying pred.

### `fn vany<T>(v: Vec<T>, pred: Fn(T) -> Bool) -> Bool`

any / all: short-circuiting existential / universal quantifiers.

### `fn vall<T>(v: Vec<T>, pred: Fn(T) -> Bool) -> Bool`

all: true iff pred holds for every element (short-circuits on the first false).

### `fn vsort<T>(v: Vec<T>, less: Fn(T, T) -> Bool) -> Vec<T>`

sort: a new Vec ordered by the comparator `less(a, b) -> Bool` (true when a should come before b).
Stable insertion sort; O(n^2) worst case.

### `trait Iterator`

an iterator yielding Int elements: call `has_next`, then `next` to advance and return the element.
Elements are Int; Str/struct elements flow through as i64 handles if cast.

### `struct Range`

a half-open integer range [lo, hi) as an Iterator.

### `fn range(lo: Int, hi: Int) -> Range`

make a Range iterator over [lo, hi).

## Iterator for Range

### `fn has_next(self) -> Bool`

true while the range has elements left.

### `fn next(inout self) -> Int`

the current element; advances the cursor.

### `fn isum<I: Iterator>(it: I) -> Int`

sum of all elements an iterator yields (consumes it).

### `fn icount<I: Iterator>(inout it: I) -> Int`

number of elements an iterator yields (consumes it).

### `fn icollect<I: Iterator>(inout it: I) -> Vec<Int>`

collect an iterator's elements into a fresh Vec<Int>.

### `struct Lazy`

a lazy half-open Int range: `foreach i in it::lazy(0, n) { … }`-style via a bound var.

### `fn lazy(lo: Int, hi: Int) -> Lazy`

lazy(lo, hi): yields lo, lo+1, …, hi-1 on demand.

## Lazy

### `struct Bytes`

a lazy iterator over a Str's raw bytes (what `s.at(i)` returns), with no Vec materialized.
For characters (Unicode scalars) use `std/unicode`'s `chars(s)`.

### `fn bytes(s: Str) -> Bytes`

a `Bytes` iterator over `s`, for `foreach b in bytes(s)`.

## Bytes


