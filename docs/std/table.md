# std/table

```nori
import "std/table" as table
```

std/table: `Table<T>`, the query layer for `durable var`.

A `Table<T>` is an ordered collection of rows with typed, closure-driven queries. It is an ordinary
serializable struct, so `durable var users: Table<User>` persists with no glue: the compiler's
type-directed serializer walks it like any other value.

Predicates are plain closures (`users.where(|u| u.age > 18)`), so they are typed and checked at
compile time, with no SQL string and no marshaling.

`where` scans. For a repeated equality lookup, `index_get(name, key, keyfn)` keeps a keyed index and
answers in O(1). The key function is passed at the call site rather than stored, because a closure
cannot be serialized and a `Table` has to persist, so the index holds only data (key -> row
positions) and is brought up to date lazily: a lookup indexes whatever rows were appended since the
last one, which is O(new rows), not a full rebuild.

You rarely write that yourself: an equality query on a field (`where`, `count_where`, `any` or
`find` with a predicate like `|u| u.name == who`) is compiled into the matching index call, keyed
on that field. The closure stays the API; the index is an implementation detail. The compiler leaves
the scan in place when it cannot prove the two agree: a non-equality predicate, a key that is a call
(a scan would evaluate it per row), a receiver whose type it cannot see, or a program that assigns
that field name anywhere (a row mutated in place would leave a keyed index stale). `index_get` and
friends stay public for the cases the rewrite declines, and for keys that are not a plain field.
### `struct Table<T>`

An ordered, persistable collection of rows. `next_id` makes ids monotonic even across deletions,
so a stored id stays meaningful for the life of the table.

### `fn new<T>() -> Table<T>`

an empty table. Use this rather than a struct literal: the index fields are implementation detail
and may change; a `durable var` gets an empty table automatically and needs no constructor at all.

## Table

### `fn count(self) -> Int`

number of rows.

### `fn insert(inout self, sink row: T) -> Int`

append `row`; returns its id (monotonic, never reused).

### `fn all(self) -> view Vec<T>`

every row, in insertion order.

### `fn id_at(self, i: Int) -> Int`

the id of the row at position `i`.

### `fn where(self, pred: Fn) -> Vec<T>`

rows matching `pred`, in insertion order. Scans.

### `fn count_where(self, pred: Fn) -> Int`

how many rows match `pred`.

### `fn any(self, pred: Fn) -> Bool`

true if any row matches.

### `fn find(self, pred: Fn) -> Int`

position of the first matching row, or -1. Use with `at` to read it.

### `fn at(self, i: Int) -> view T`

the row at position `i`.

### `fn index_of_id(self, id: Int) -> Int`

position of the row with id `id`, or -1.

### `fn index_get(inout self, name: Str, key: Str, keyfn: Fn(T)->Str) -> Vec<T>`

rows whose key equals `key`, using the index called `name`. `keyfn` maps a row to its key and is
applied only to rows not yet indexed, so the first call costs one scan and later calls are O(1).
Passing a different `keyfn` for the same `name` is a caller error, since the index is keyed by name.

### `fn index_count(inout self, name: Str, key: Str, keyfn: Fn(T)->Str) -> Int`

how many rows have this key. Same indexing rules as `index_get`.

### `fn index_has(inout self, name: Str, key: Str, keyfn: Fn(T)->Str) -> Bool`

true if any row has this key.

### `fn index_find(inout self, name: Str, key: Str, keyfn: Fn(T)->Str) -> Int`

position of the first row with this key, or -1: the indexed answer to `find`. Positions are
recorded in insertion order, so this is the same row a scan would stop at.

### `fn index_reset(inout self)`

forget every index. They are derived data, so this only costs the next lookup a rescan.

### `fn delete_where(inout self, pred: Fn) -> Int`

delete every row matching `pred`; returns how many went. Order of the survivors is preserved.

### `fn clear(inout self)`

remove every row.


