# std/pvec

```nori
import "std/pvec" as pvec
```

std/pvec: a persistent vector: a bit-partitioned trie (branching factor 32). `append` and `assoc`
return a new version that shares every untouched node with the old one (they copy only the
~log32(n) nodes on the path from the root to the change), so keeping many versions (e.g. an undo
history) costs O(log n) per version instead of O(n). Old versions are never mutated.

Shared nodes have many owners, which no single version can be. So the nodes live in a `Trie`, an
arena every version of a vector refers into by index; a version is a small scalar record (a count,
a shift, a root index) that copies freely. The arena owns every node and frees them all when it
dies; until then a node stays as long as the arena does, whether or not a live version still
reaches it; that retention is the price of sharing, paid once per arena rather than never.

  import "std/pvec" as pv
  var t = pv::trie()
  var a = pv::empty()
  var b = pv::append(t, a, 10)     // a is still empty; b has one element (shares nothing new)
  var c = pv::assoc(t, b, 0, 99)   // b unchanged; c[0] == 99, sharing b's other nodes
  pv::at(t, c, 0)  pv::count(c)  pv::to_vec(t, c)

To build one from data you already have, reach for `of_vec` rather than folding `append` over it:
each `append` mints a fresh root-to-leaf spine, so a fold leaves n-1 intermediate versions in the
arena, while `of_vec` builds the same trie bottom-up and allocates only the nodes the result keeps.
### `struct Trie`

the arena every version of a vector refers into: node `i` is `nodes[i]`, a leaf's values or a
branch's child indices (`kinds[i]` = 0 leaf, 1 branch).

### `struct PVec`

a persistent vector: element count, root bit-shift (5·(height-1)), and the root node's index
(-1 for the empty vector, which owns no node).

### `fn trie() -> Trie`

a fresh, empty arena.

### `fn empty() -> PVec`

the empty persistent vector.

### `fn count(v: PVec) -> Int`

the number of elements.

### `fn is_empty(v: PVec) -> Bool`

is it empty?

### `fn node_count(t: Trie) -> Int`

the number of nodes the arena holds (every version's, shared ones counted once).

### `fn append(inout t: Trie, v: PVec, x: Int) -> PVec`

append `x`, returning a new version that shares all of `v`'s untouched nodes.

### `fn at(t: Trie, v: PVec, i: Int) -> Int`

the element at index `i` (call with 0 <= i < count).

### `fn assoc(inout t: Trie, v: PVec, i: Int, x: Int) -> PVec`

a new version with index `i` replaced by `x`, sharing every other node with `v`.

### `fn of_vec(inout t: Trie, a: Vec<Int>) -> PVec`

build a persistent vector from a Vec<Int>.

Built bottom-up: the leaves first, then a level of branches over them, until one node is left.
The layout produced here is the one `append` would have arrived at, so appending to the result
still works.

### `fn to_vec(t: Trie, v: PVec) -> Vec<Int>`

materialize a persistent vector back into a Vec<Int>.


