# std/sys

```nori
import "std/sys" as sys
```

std/sys: host facts (the target triple "<arch>-<os>"). No C
shim: reads the OS `uname(2)` through `extern fn` into a buffer and pulls the machine + sysname strings
out with `unsafe` peek (libc linked automatically). The `struct utsname` field width differs per OS
(Linux 65, macOS 256), so the layout lives in `cfg(...)` blocks; the formatting is shared.
### `fn read_cstr(p: Int) -> Str`

a NUL-terminated C string at address `p` -> a Nori Str.

### `fn lower(s: Str) -> Str`

ASCII-lowercase (uname's sysname is "Linux"/"Darwin"; the triple is lowercased).

### `fn target() -> Str`

the host target triple, "<arch>-<os>" lowercased, e.g. "x86_64-linux", "aarch64-darwin".


