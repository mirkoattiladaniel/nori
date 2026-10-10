# std/deque

```nori
import "std/deque" as deque
```

std/deque: a double-ended queue of `Int` (i64-uniform, so it also holds any handle/Bool/Char).
Backed by a Vec with a moving head index: push/pop at both ends, peek, size. O(1) amortized at the
back and for pop-front; push-front is O(1) when there's slack, else O(n) to make room.
  import "std/deque" as deque
  var d = deque::new()
  deque::push_back(d, 1)  deque::push_front(d, 0)
  deque::pop_front(d)     // 0
### `fn new() -> Deque`

a new empty deque.

### `fn size(d: Deque) -> Int`

number of live elements.

### `fn is_empty(d: Deque) -> Bool`

is the deque empty?

### `fn push_back(inout d: Deque, x: Int)`

append `x` at the back.

### `fn push_front(inout d: Deque, x: Int)`

prepend `x` at the front.

### `fn peek_front(d: Deque) -> Int`

the front element (call only when non-empty).

### `fn peek_back(d: Deque) -> Int`

the back element (call only when non-empty).

### `fn pop_front(inout d: Deque) -> Int`

remove and return the front element (call only when non-empty; it traps on an empty deque, the
same as `peek_front`, so guard with `is_empty`).

### `fn pop_back(inout d: Deque) -> Int`

remove and return the back element (call only when non-empty; it traps on an empty deque, the
same as `peek_back`, so guard with `is_empty`).


