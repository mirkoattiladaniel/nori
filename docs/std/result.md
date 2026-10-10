# std/result

```nori
import "std/result" as result
```

std/result: recoverable errors, so a failure can be handled instead of trapping the program.

`Option<T>` is "a value, or nothing"; `Result<T, E>` is "a value, or an error". The success case
is the first variant (Some / Ok), which `try` relies on: `try e` unwraps Some/Ok, or propagates
None/Err out of the enclosing function.

These wrappers are non-trapping alternatives to the trapping builtins (read_file, parse_int,
lookup, v[i], a/b): they return Ok/Err or Some/None, which you then `match`, `try`, or feed to a
`*_or` helper.
### `fn is_int(s: Str) -> Bool`

can `s` be parsed as an integer? (optional leading '-', then digits)

### `fn file_exists(path: Str) -> Bool`

does a file exist at `path`? (libc `access` with F_OK)

### `fn is_regular_file(path: Str) -> Bool`

is `path` a regular file, which is what read_file can read?

Existence is not enough. `access` succeeds on a directory, so a wrapper that only checks
existence hands the path to read_file and the "non-trapping" call traps after all. S_IFREG.

### `fn is_directory(path: Str) -> Bool`

is `path` an existing directory? S_IFDIR.

### `fn try_parse_int(s: Str) -> Result<Int>`

parse `s` as an Int, returning Ok(value) or Err(message) instead of trapping. Use this when the
caller needs to know why it failed; when a fallback is enough, the `parse_int_or(s, dflt)` builtin
is cheaper: it scans once, where this validates and then parses again, and allocates a `Result`.

### `fn try_read_file(path: Str) -> Result<Str>`

read the file at `path`, returning Ok(contents) or Err(message) instead of trapping.

Checks that the path is a regular file, not merely that something exists there: `access` is happy
with a directory and read_file is not, so testing existence alone would trap on a directory.

### `fn try_write_file(path: Str, content: Str) -> Result<Str>`

write `content` to `path`, returning Ok(path) or Err(message) instead of trapping.

Catches what can be known before writing: a destination that is a directory, a parent directory
that does not exist or refuses writes. A failure during the write itself (a full disk, an
exceeded quota, an I/O error) cannot be known in advance and still traps, with the cause
named, so write_file never reports success for a write that did not happen.

### `fn vget<T>(v: Vec<T>, i: Int) -> Option<T>`

safe element access: None when out of range (never traps), for a Vec of any element type

### `fn mget<V>(m: Map<Str, V>, key: Str) -> Option<V>`

safe map lookup: None when the key is absent, for a Map of any value type

### `fn lookup_or<V>(m: Map<Str, V>, key: Str, sink dflt: V) -> V`

the value at `key`, or `dflt` when it is absent: a non-trapping read that costs nothing extra.
Prefer this over `mget` on a hot path: an `Option` is a heap object (both variants allocate), so
`mget(...) else d` pays an allocation per read where this pays none.

### `fn try_div(a: Int, b: Int) -> Result<Int>`

safe integer division: Err on divide-by-zero

### `fn unwrap_or<T>(sink o: Option<T>, sink default: T) -> T`

the Some value, or `default` on None

### `fn is_some<T>(o: Option<T>) -> Bool`

true if the Option is Some.

### `fn ok_or<T>(sink r: Result<T>, sink default: T) -> T`

the Ok value, or `default` on Err

### `fn is_ok<T>(r: Result<T>) -> Bool`

true if the Result is Ok.


