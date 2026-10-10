# std/hashmap

```nori
import "std/hashmap" as hashmap
```

std/hashmap: Int-keyed hash collections (the runtime `Map` is Str-keyed only). Open-addressing
hash tables with linear probing and power-of-two capacity, growing at a 0.75 load factor.

Keys and values are plain `Int` (i64). Because Nori values are i64-uniform, an `Int` slot can hold
any i64 value: Int, Bool, Char, or a handle to a Str/Vec/Map/struct. The one exception is Float,
which is f64 and cannot be stored here.

  import "std/hashmap" as hm
  var m = hm::intmap_new()
  m.put(42, 100)
  m.get_or(42, -1)     // 100
  m.contains(42)       // true
  m.remove(42)         // true; m.size() is now 0

  var s = hm::intset_new()
  s.add(7)  s.add(7)   // second add returns false
  s.contains(7)        // true
  s.items()            // Vec<Int> of members (any order)
### `fn intmap_new() -> IntMap`

a fresh empty Int->Int map (initial capacity 8).

## IntMap

### `fn put(inout self, k: Int, v: Int) -> Int`

insert or overwrite key `k` with value `v`. grows (x2) at a 0.75 load factor. returns 0.

### `fn get_or(self, k: Int, dflt: Int) -> Int`

value for `k`, or `dflt` if `k` is absent.

### `fn contains(self, k: Int) -> Bool`

is `k` present?

### `fn remove(inout self, k: Int) -> Bool`

remove `k`; returns true if it was present.

### `fn size(self) -> Int`

number of present entries.

### `fn keys(self) -> Vec<Int>`

all present keys, in arbitrary order.

### `fn intset_new() -> IntSet`

a fresh empty set of Ints (initial capacity 8).

## IntSet

### `fn add(inout self, k: Int) -> Bool`

insert `k`; returns true if newly added, false if already present.

### `fn items(self) -> Vec<Int>`

the members, in arbitrary order.


