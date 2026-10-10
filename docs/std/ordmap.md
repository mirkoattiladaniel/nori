# std/ordmap

```nori
import "std/ordmap" as ordmap
```

std/ordmap: an insertion-ordered string-keyed map to `Int` values (i64-uniform, so values may be any
handle/Bool/Char). The map companion to std/set: O(1) lookup via the runtime Str `Map`, plus a Vec that
preserves insertion order for `keys`/`values` iteration. Re-inserting a key updates its value and keeps
its original position.
  import "std/ordmap" as ordmap
  var m = ordmap::new()
  m.insert("a", 1)  m.insert("b", 2)
  m.get("a")               // 1
  m.keys()                 // ["a", "b"]

The operations are methods on `OrdMap`, as in std/set: `insert`, `get`, `keys` and `values` are
built-ins' names, and a top-level function wearing one is a function nothing can call.
### `fn new() -> OrdMap`

a new empty map.

## OrdMap

### `fn size(self) -> Int`

number of entries.

### `fn is_empty(self) -> Bool`

is the map empty?

### `fn contains(self, key: Str) -> Bool`

does `key` exist?

### `fn insert(inout self, key: Str, val: Int)`

set `key` to `val` (appended on first insert; value updated in place if it already exists).

### `fn get(self, key: Str) -> Int`

the value for `key` (call only when `contains` is true).

### `fn get_or(self, key: Str, dflt: Int) -> Int`

the value for `key`, or `dflt` if absent.

### `fn keys(self) -> view Vec<Str>`

the keys in insertion order.

### `fn values(self) -> Vec<Int>`

the values in insertion order.


