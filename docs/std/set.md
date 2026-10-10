# std/set

```nori
import "std/set" as set
```

std/set: an insertion-ordered set of strings, backed by the Str-keyed Map (O(1) membership) plus a
Vec that preserves insertion order for iteration. Strings are the element type (other types stringify
via `"" + v` before insertion).

  import "std/set" as set
  var s = set::new()
  s.add("a")  s.add("b")  s.add("a")   // add returns false the second time
  s.contains("a")        // true
  s.remove("a")          // true (was present); s.size() is now 1
  s.size()               // 2
  foreach-style: iterate s.items() (a Vec<Str> in insertion order)
  set::unique(words)     // dedup a Vec<Str>, order-preserving
### `fn inew() -> ISet`

a fresh empty Int set.

## ISet

### `fn add(inout self, k: Int) -> Bool`

insert `k`; returns true if newly added, false if already present.

### `fn contains(self, k: Int) -> Bool`

is `k` a member of the set?

### `fn size(self) -> Int`

number of elements in the set.

### `fn items(self) -> view Vec<Int>`

the members in insertion order (do not mutate the returned Vec).

### `fn remove(inout self, k: Int) -> Bool`

remove `k`; returns true if it was present, false if it wasn't in the set.

### `fn new() -> Set`

a fresh empty set.

## Set

### `fn add(inout self, k: Str) -> Bool`

insert `k`; returns true if newly added, false if already present.

### `fn contains(self, k: Str) -> Bool`

is `k` a member of the set?

### `fn items(self) -> view Vec<Str>`

the members in insertion order (do not mutate the returned Vec).

### `fn remove(inout self, k: Str) -> Bool`

remove `k`; returns true if it was present, false if it wasn't in the set.

### `fn from_vec(v: Vec<Str>) -> Set`

a set containing every element of a Vec<Str> (deduplicated).

### `fn unique(v: Vec<Str>) -> Vec<Str>`

dedup a Vec<Str>, preserving first-seen order.

### `fn union(a: Set, b: Set) -> Set`

a ∪ b: every element of either, a's order first.

### `fn intersect(a: Set, b: Set) -> Set`

a ∩ b: elements present in both (a's order).

### `fn difference(a: Set, b: Set) -> Set`

a \ b: elements of a not in b (a's order).


