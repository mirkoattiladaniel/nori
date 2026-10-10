# 18. Conditional Compilation

`cfg` compiles code in or out based on the build target or user-defined flags.
Code that is flagged **out** is blanked before the file is even parsed, so it is
not type-checked and leaves no trace in the binary; the exclusion is provable,
not just a dead branch.

## Target conditions

`cfg(NAME) { … }` keeps its contents only when `NAME` is active for this build.
The built-in names come in three dimensions (the operating system, the CPU, and
the libc mode), and a block is kept when its name matches any one of them.

### The operating system

```nori
cfg(linux)   { fn platform() -> Str { return "linux" } }
cfg(macos)   { fn platform() -> Str { return "macos" } }
cfg(windows) { fn platform() -> Str { return "windows" } }
```

`bare` is the fourth: a **freestanding** build, selected by
`noric --build --freestanding` (which compiles the program and the runtime for
`--os bare`). There is no operating system underneath one, so `cfg(bare)` is how
a program names its own floor where a hosted build would name a syscall.

`unix` is not an operating system. It is active on **every target that is not
Windows**, and it exists because `cfg` has no negation: without it, "everywhere
but Windows" could only be written by repeating the block once per platform,
which is precisely the shape that drifts. One implementation over the POSIX
floor and one over the Win32 one is what most portable code actually has:

```nori
cfg(unix)    { fn sep() -> Str { return "/" } }
cfg(windows) { fn sep() -> Str { return "\\" } }
```

`android` is a platform name that rides alongside `linux`, not instead of it: an
`*-android` triple activates both, because Android is Linux (same kernel, same
syscalls) and only the layer above differs (no X11/Wayland, fonts under
`/system/fonts`, bionic, the app sandbox). So `cfg(linux)` still selects the
syscall floor on Android, and `cfg(android)` guards what is genuinely different.
It follows that `cfg(unix)` is active there too.

### The CPU

`x86_64` and `arm64`. The default is **the CPU the compiler itself is running
on**, so an arm64 compiler emits a program's arm64 blocks with no `--arch`;
`--arch A` or a `--target` triple names another.

Conditions nest, which is how platform-and-arch-specific code is gated:

```nori
cfg(linux) {
    cfg(x86_64) {
        // Linux/x86-64 only — e.g. raw syscalls
    }
}
```

### The libc mode

`libc` and `nolibc` are the two halves of one dimension, and exactly one of them
is active in every build. **On Linux the default is `nolibc`**: a native Linux
build is static and libc-free unless the program reaches a function imported
from a shared library. Cross builds to other systems default to `libc`.
`noric --nolibc` and `noric --libc` say which explicitly, and the inactive half's
blocks are dropped exactly like an inactive OS.

### No negation, and no grouping

`cfg(NAME)` takes one name. There is no `cfg(not(windows))`, no `any`/`all`, and
no `else` branch; `unix` exists because of the first of those, and the rest is
what the three dimensions and nesting are for. A name that matches nothing (and
is not a `--cfg` flag, below) is simply inactive, so a misspelled target name
silently compiles its block out rather than failing.

The active target is chosen at build time: `noric --os NAME` / `--arch NAME`, a
`--target` triple (which sets both), or `roll build --target …` to cross-compile.
With none of them, the target is this host and this CPU.

## User-defined flags

You can define your own flags. `noric --cfg NAME` activates `NAME`, and
`cfg(NAME) { … }` blocks for that name are kept; all other user names are blanked.
This works at **both** top level (gating a declaration) and statement position
(gating a call site):

```nori
cfg(devtools) {
    fn dev_menu() { /* … */ }        // definition gated
}

fn main() -> Int {
    cfg(devtools) { dev_menu() }     // call site gated too
    return 0
}
```

## Wiring flags through `roll`

A build profile declares which flags it turns on:

```toml
[profile.dev]
cfg = ["devtools"]        # dev builds include dev tools

[profile.release]
# release: devtools not listed -> compiled out, provably
```

`roll build` uses the dev profile (dev tools compiled in); `roll build --release`
uses release (they're gone). Because flagged-out code isn't compiled, this is a
**build-system guarantee**: a released binary cannot contain a dev-only subsystem
(a cheat menu, a debug backdoor). You can confirm with `strings` on the output —
the gated code's symbols simply aren't there.

## Semantics

- A flagged-out block is **blanked before lexing**; it need not even be
  well-formed relative to the rest of the program (an inactive `cfg(macos)` block
  on Linux can reference macOS-only externs).
- An active block's contents are spliced in as if written directly.
- The same mechanism serves target `cfg` and user `--cfg`; they share one active
  set.

---

Next: [Verification and Contracts](19_verification.md).
