# std/dl

```nori
import "std/dl" as dl
```

std/dl: load a shared library at runtime and call into it.

  import "std/dl" as dl
  fn main() -> Int {
    let h = dl::load("./demo.so")
    if h == 0 { printl(dl::error())  return 1 }
    let f = dl::sym(h, "nori_plugin_abi")
    if f != 0 { printl(dl::call0(f)) }        // 1
    dl::unload(h)
    return 0
  }

Addresses, and any C string, are plain `Int`. A call goes through a `CFn` function pointer of the
callee's signature, one of the `call*` functions below, so the same source serves an LLVM build and
a `--native` one: on Linux the loader itself is imported from `libc.so.6`, which makes a native
executable that loads a library a glibc-hosted one; on macOS from `libSystem.B.dylib`, and on Windows
`LoadLibraryA`/`GetProcAddress`/`FreeLibrary` from `kernel32.dll`.

A library loaded this way has its own copy of whatever runtime it was linked with, so a handle
minted inside it (a `Str`, a `Vec`) means nothing out here. Only scalars and C strings may cross:
convert with `os::os_cstr` / `os::os_from_cstr`.
### `fn load(path: Str) -> Int`

open a shared library by path. 0 = failed; call `error()` for why.
Not `open`/`close`: those are the POSIX calls std/os declares as `extern`, and an extern name is
never mangled, so a program that imports this module unaliased would collide with them.

### `fn sym(h: Int, name: Str) -> Int`

look up a symbol in an open library. 0 = not found.

### `fn unload(h: Int) -> Bool`

close an open library. true = closed.

### `fn error() -> Str`

the last loader error, or "" when there was none.

### `fn call0(f: Int) -> Int`

call `f()` -> Int.

### `fn call1(f: Int, a: Int) -> Int`

call `f(a)` -> Int.

### `fn call2(f: Int, a: Int, b: Int) -> Int`

call `f(a, b)` -> Int.

### `fn call3(f: Int, a: Int, b: Int, c: Int) -> Int`

call `f(a, b, c)` -> Int.

### `fn call_iff(f: Int, a: Int, w: Float, h: Float) -> Int`

call `f(a, w, h)` where the last two are Floats -> Int.

### `fn call_f32(f: Int, a: Float) -> Int`

call `f(x)` where `x` is a C **float** (32-bit), -> Int. The narrowing is required: a C float
and a double travel in the same register in different formats, so the callee's 32-bit parameter
must arrive as a float. A `void`-returning callee is fine; the Int result is then meaningless.

### `fn call5(f: Int, a: Int, b: Int, c: Int, d: Int, e: Int) -> Int`

call `f(a, b, c, d, e)` -> Int. Five Ints is the widest fixed arity here. It serves the plugin
surface entry point: `(id: cstr, device, queue, w, h) -> texture view`.

### `fn call0_str(f: Int) -> Str`

call `f()` and read the returned C string. "" when the call returns NULL.

### `fn call1_str(f: Int, s: Str) -> Str`

call `f(s)` with `s` as a C string, and read the returned C string.

### `fn call2_str(f: Int, a: Str, b: Str) -> Int`

call `f(a, b)` with both args as C strings, returning the Int result.

### `fn call2_cstr(f: Int, a: Str, b: Str) -> Str`

call `f(a, b)` with both args as C strings, and read the returned C string.

The two arguments belong to this side: allocated here, freed here, and the callee must copy them
rather than keep them. The reply belongs to the callee: it is copied in immediately and never
freed on this side, since a block allocated by another image's malloc cannot be returned to ours.

### `fn host_call(f: Int, key: Str, arg: Str) -> Str`

invoke a host callback address as `(key: cstr, arg: cstr) -> cstr`, the host-callback shape.

This is the plugin side of the host callback: a plugin that was handed an address by
`nori_plugin_init` calls its host with `dl::host_call(addr, key, arg)` and gets JSON back. A
null address or a null reply answers with the same `{"ok":false,"error":…}` shape a host uses for an
unknown key, so a plugin has one shape to parse and never has to guard the call.

### `fn call_s4i(f: Int, id: Str, a: Int, b: Int, c: Int, d: Int) -> Int`

call `f(id, a, b, c, d)` with `id` as a C string and four Ints after it, returning the Int
result. The string is allocated and freed here, so the callee must copy it rather than
keep the pointer, as with every other C-string argument in this file.

### `fn call_iff_str(f: Int, id: Str, w: Float, h: Float) -> Str`

call `f(id, w, h)` with `id` as a C string, and read the returned C string.

### `fn ext() -> Str`

the platform's shared-library extension, without the dot: `so`, `dylib`, or `dll`.


