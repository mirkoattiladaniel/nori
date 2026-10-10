# std/heap

```nori
import "std/heap" as heap
```

std/heap: a binary min-heap / priority queue of `Int` (smallest comes out first). For a max-heap or a
keyed priority, store negated / encoded priorities. O(log n) insert and pop, O(1) peek.
  import "std/heap" as heap
  var h = heap::new()
  h.insert(5)  h.insert(1)  h.insert(3)
  h.pop_min()         // 1

The operations are methods on `Heap`, as in std/set: `insert` is a built-in's name, and a top-level
function wearing one is a function nothing can call.
### `fn new() -> Heap`

a new empty heap.

## Heap

### `fn size(self) -> Int`

number of elements.

### `fn is_empty(self) -> Bool`

is the heap empty?

### `fn peek_min(self) -> Int`

the current minimum without removing it (call only when non-empty).

### `fn insert(inout self, x: Int)`

insert `x`, restoring the heap property (sift up).

### `fn pop_min(inout self) -> Int`

remove and return the minimum (call only when non-empty).


