# std/toml

```nori
import "std/toml" as toml
```

std/toml: a practical TOML reader for the common config subset (the shape roll's `nori.manifest`
uses): `[section]` / `[a.b]` tables, `key = value` pairs, `#` comments, `"..."` strings, ints, floats,
and bools. The API queries the document by (section, key) rather than building a value tree, which fits
Nori's types and matches how manifests are read. The root (before any `[section]`) is section "".
  import "std/toml" as toml
  toml::get_str(src, "package", "name")     // the string value, or "" if absent
  toml::get_int(src, "build", "opt")
  toml::sections(src)                       // every [section] name

Not `get`/`has`/`keys`: those are built-ins' names, and a top-level function wearing one is a
function nothing can call. The reader is stateless (it queries the text), so there is no document
type to hang methods on the way std/ordmap does.
### `fn get_str(text: Str, section: Str, key: Str) -> Str`

the value of `key` in `[section]` ("" for the root), or "" if not present.

### `fn has_key(text: Str, section: Str, key: Str) -> Bool`

does `[section]` define `key`?

### `fn get_or(text: Str, section: Str, key: Str, dflt: Str) -> Str`

the value of `key` in `[section]`, or `dflt` if absent.

### `fn get_int(text: Str, section: Str, key: Str) -> Int`

the value as an Int. Traps when the key is absent or its value is not an integer. A
configuration file is data, and a typo in one is not a reason to abort a program, so prefer
`get_int_or` for a default or `try_get_int` to report it.

### `fn get_int_or(text: Str, section: Str, key: Str, dflt: Int) -> Int`

the value as an Int, or `dflt` when the key is absent or is not an integer. Does not trap.

### `fn try_get_int(text: Str, section: Str, key: Str) -> Result<Int>`

the value as an Int, or Err naming what was wrong with it. Does not trap.

### `fn get_bool(text: Str, section: Str, key: Str) -> Bool`

the value as a bool (`true` only when the value is exactly "true").

### `fn sections(text: Str) -> Vec<Str>`

every `[section]` name declared in the document (in order; the root "" is not listed).

### `fn section_keys(text: Str, section: Str) -> Vec<Str>`

every key defined directly under `[section]` ("" for the root).


