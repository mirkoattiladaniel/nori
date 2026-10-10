# std/flag

```nori
import "std/flag" as flag
```

std/flag: a getopt-style command-line flag parser. Define flags (string/int/bool) with defaults,
then `parse` a Vec<Str> of arguments. Everything is stored in Str-keyed Maps inside the struct;
values are kept stringified and converted on the way out by the typed getters.

  import "std/flag" as flag
  var f = flag::new()
  f.add_str("name", "world")
  f.add_int("count", 1)
  f.add_bool("verbose")
  f.parse(flag::argv())              // or pass your own Vec<Str>
  f.get_str("name")                  // parsed value, else default, else ""
  f.get_int("count")                 // parse_int of the stored string
  f.get_bool("verbose")              // true iff stored value is "1"
  f.positionals()                    // Vec<Str> of the non-flag args, in order

Supported forms: `--name value`, `--name=value`, bare `--name` (a bool → "1"), short `-n`
(treated like `--n`), and `--` which ends option parsing (the rest become positionals).
Unknown `--x` is lenient: recorded as a string so `get_str` can still read it.
### `struct Flags`

kinds[name] = 0 (string) | 1 (int) | 2 (bool); defs/vals hold stringified defaults/parsed values;
pos holds the positional (non-flag) arguments in order.

### `fn new() -> Flags`

a fresh parser with no flags defined and no positionals.

### `fn argv() -> Vec<Str>`

build a Vec<Str> from the live process argv (`argc()`/`arg(i)`), for `f.parse(argv())`.

### `fn strip_dashes(tok: Str) -> Str`

strip leading dashes from a flag token: `--name` and `-n` both yield `name`.

## Flags

### `fn add_str(inout self, name: Str, sink dflt: Str) -> Int`

declare a string flag `name` with default `dflt`. Returns 0.

### `fn add_int(inout self, name: Str, dflt: Int) -> Int`

declare an int flag `name` with default `dflt` (stored stringified). Returns 0.

### `fn add_bool(inout self, name: Str) -> Int`

declare a bool flag `name`; default is false ("0"). Returns 0.

### `fn parse(inout self, args: Vec<Str>) -> Int`

parse `args`: option forms `--name value`, `--name=value`, bare `--name` (bool), short `-n`,
and `--` which ends option parsing (the rest are positional). Returns 0.

### `fn get_str(self, name: Str) -> Str`

the parsed string for `name`, else its default, else "".

### `fn get_int(self, name: Str) -> Int`

the parsed value of `name` as an Int. Traps when the value is not a number; see `get_int_or`
and `try_get_int`, which do not.

### `fn get_int_or(self, name: Str, dflt: Int) -> Int`

the value of `name` as an Int, or `dflt` when it is absent or not a number.

A command line is typed by a person, and `--count abc` is a typo rather than a reason to abort:
`get_int` reaches parse_int, which traps. Use this when the program should carry on, or
`try_get_int` when it should say what was wrong.

### `fn try_get_int(self, name: Str) -> Result<Int>`

the value of `name` as an Int, or Err naming what was wrong with it. Does not trap.

### `fn get_bool(self, name: Str) -> Bool`

true iff the stored value for `name` is "1".

### `fn positionals(self) -> view Vec<Str>`

the positional (non-flag) arguments, in order.


