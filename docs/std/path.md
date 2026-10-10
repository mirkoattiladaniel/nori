# std/path

```nori
import "std/path" as path
```

std/path: path manipulation that already knows which platform it is on. String work only:
operates on the path text (no filesystem access; that's std/os): join/split components, take the
dir or file part, the extension, and normalize `.`/`..`. All functions are total and return fresh
strings.
  import "std/path" as path
  path::join("src", "main.nori")     // "src/main.nori", or "src\\main.nori" on Windows
  path::basename("D:\\src\\a.nori")  // "a.nori", on every platform
  path::ext("a/b.tar.gz")            // "gz"
  path::normalize("a/./b/../c")      // "a/c"

Reading accepts both separators, writing uses the native one. A path you are handed comes from
somewhere you do not control: an argument, an environment variable, a recent-files list written
on another machine, a manifest someone checked in. On Windows it is spelled with `\`, and a
`/`-only reader answers nonsense for it rather than failing: `basename("D:\src\thing")` gives
back the whole string, `dirname` gives ".". A path you build is yours, so it gets the separator
the platform actually uses.
### `fn sep_of(p: Str) -> Str`

the separator `p` is written with (its first one), or the platform's when it has none.

`normalize` re-emits components and has to choose. Choosing the native one turns
`normalize("D:\\a\\.\\b")` on Linux into `D:\\a/b`, which is neither spelling and is what a
tool would then print at someone. A path that says how it is spelled gets to keep saying it.

### `fn is_sep(c: Int) -> Bool`

is `c` a path separator? Both of them, everywhere. A Windows path read on Linux (a manifest, a
log, a recent list) is still a Windows path, and a `/` inside a Windows path is accepted by every
Win32 call there is.

### `fn root_len(p: Str) -> Int`

how many leading characters of `p` are its root: the part `..` can never climb above and that a
relative path does not have. 0 when `p` is relative.
  "/usr/lib"        -> 1   ("/")
  "C:\\src"          -> 3   ("C:\\")
  "C:"              -> 2   (the drive, with no directory said)
  "\\\\server\\share"  -> the whole share prefix

### `fn is_abs(p: Str) -> Bool`

is `p` absolute, i.e. rooted, so that resolving it does not depend on where you are standing?

`C:\foo` is. `C:foo` is not: it names a path relative to the current directory on drive C, which
is a thing Windows keeps per drive, and treating it as absolute is how a file lands somewhere
nobody asked for.

### `fn to_native(p: Str) -> Str`

`p` with every separator replaced by the platform's own. What to call just before handing a path
to something outside this program that might be fussy about it.

### `fn to_slash(p: Str) -> Str`

`p` with every separator replaced by `/`. Config files, manifests and anything else meant to be
read on a machine other than this one want this spelling.

### `fn join(a: Str, b: Str) -> Str`

join two path segments with exactly one separator, the platform's. An empty side is dropped; an
absolute `b` wins. `a`'s own spelling is left alone: only the separator this inserts is native, so
joining onto a path someone handed you does not rewrite the half they gave.

### `fn join_all(parts: Vec<Str>) -> Str`

join a list of segments left-to-right (`join_all(["a","b","c"])` -> "a/b/c").

### `fn dirname(p: Str) -> Str`

the directory part of `p`: everything before the last separator; "." when there is none, and the
root itself at the root ("/", "C:\\", a UNC share), which is where climbing stops.

### `fn basename(p: Str) -> Str`

the final component of `p`, after the last separator.

The root is its own name, and a path that is nothing but root has no last component: "/" answers
"/", "C:\\" answers "C:\\". The result is never "", including for anything ending in a
separator, since callers tend to put the name straight into a label or a log line.

### `fn ext(p: Str) -> Str`

the extension of `p` without the dot ("a/b.tar.gz" -> "gz"); "" if none. A leading dot is not an
extension (".bashrc" -> "").

### `fn stem(p: Str) -> Str`

the filename of `p` without its extension ("a/b.txt" -> "b"; ".bashrc" -> ".bashrc").

### `fn with_ext(p: Str, e: Str) -> Str`

`p` with its extension replaced by `e` (no dot in `e`); an empty `e` removes the extension.

### `fn split(p: Str) -> Vec<Str>`

split `p` into its non-empty components ("/a//b/" -> ["a", "b"]).

### `fn normalize(p: Str) -> Str`

lexically normalize `p`: drop `.`, resolve `..` against earlier components, and collapse repeated `/`.
Absolute-ness is preserved; an empty result is "." (relative) or "/" (absolute).


