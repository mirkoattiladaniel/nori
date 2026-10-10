# std/io

```nori
import "std/io" as io
```

std/io: the small I/O helpers not already covered by std/os + the read_file/write_file builtins:
an efficient string Builder (accumulate parts, join once, which avoids O(n^2) repeated `+`), line
splitting (handles LF and CRLF), and streamed file copy (chunked, never loads the whole file into
memory). For env/dirs/spawn/stdin/stdout and raw fd ops see std/os.
  import "std/io" as io
  var b = io::builder()  io::add(b, "x=")  io::add_line(b, "1")
  io::build(b)                    // "x=1\n"
  io::read_lines("file.txt")     // Vec<Str>, one per line
  io::copy_file("a.bin", "b.bin") // streamed 64K-at-a-time copy; creates b.bin
### `fn builder() -> Builder`

a new empty string builder.

### `fn add(inout b: Builder, sink s: Str)`

append `s`.

### `fn add_line(inout b: Builder, sink s: Str)`

append `s` followed by a newline.

### `fn build(b: Builder) -> Str`

join everything appended so far into one string.

### `fn lines(s: Str) -> Vec<Str>`

split `s` into lines (separators removed; LF and CRLF both end a line). A trailing newline does not
produce a final empty line.

### `fn read_lines(path: Str) -> Vec<Str>`

read file `path` and split it into lines ("" -> [] if it can't be read).

### `fn copy_file(src: Str, dst: Str) -> Bool`

copy `src` to `dst` a chunk at a time (64 KB), creating/truncating `dst` (mode 0644). It never loads
the whole file into memory, so it handles files far larger than RAM. Returns true on success. This is
the safe, no-`unsafe`-needed alternative to slurping a file with read_file + write_file.

### `fn append_file(path: Str, data: Str) -> Bool`

append `data` to `path` (creating it if absent, mode 0644). Returns true on success.


