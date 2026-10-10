# std/sort

```nori
import "std/sort" as sort
```

std/sort: ordering. Implement the `Ord` trait (`cmp(self, other) -> Int`, returning negative /
zero / positive like `a - b`) on your type, then `sort` any Vec of it; `min`/`max` find extremes.
For the built-in scalar types use `sort_int` / `sort_str` (and `strcmp` for strings). All sorts
are stable merge sorts, O(n log n), and return a fresh Vec.

  import "std/sort" as sort
  struct Person { age: Int, name: Str }
  impl sort::Ord for Person { fn cmp(self, o: Person) -> Int { return self.age - o.age } }
  var sorted = sort::sort(people)         // by age
  var names  = sort::sort_str(raw_names)  // lexicographic
  var lowest = sort::min(people)
### `trait Ord`

a totally-ordered type: `cmp` returns <0 if self<other, 0 if equal, >0 if self>other.
(the `other: Int` here is a placeholder; your impl declares the real type.)

### `fn sort<T: Ord>(v: Vec<T>) -> Vec<T>`

sort a Vec of any `Ord` type ascending (stable; merge sort).

A bottom-up merge sort rather than an insertion sort, so large inputs (thousands of paths, say)
stay O(n log n). Stable (a tie keeps its left element first), and moves elements rather than
copying them.

### `fn min<T: Ord>(v: Vec<T>) -> T`

the smallest element of a non-empty Vec<T: Ord>.

### `fn max<T: Ord>(v: Vec<T>) -> T`

the largest element of a non-empty Vec<T: Ord>.

### `fn strcmp(a: Str, b: Str) -> Int`

lexicographic comparison of two strings by byte: <0 if a<b, 0 if equal, >0 if a>b.

### `fn sort_str(v: Vec<Str>) -> Vec<Str>`

sort a Vec<Str> ascending (lexicographic, via strcmp; stable merge sort).

### `fn sort_int(v: Vec<Int>) -> Vec<Int>`

sort a Vec<Int> ascending (merge sort).


