# Nori

Nori is a low-level programming language aiming at C's speed with memory safety by default: no garbage
collector, and no lifetimes for the programmer to prove. The compiler does the proving, and spends its time
erasing what safety costs.

Its compiler, `noric`, compiles itself and builds native executables with its own code generator and linker,
with no C compiler and no LLVM. It has built an entire operating system, [NoriOS](https://nori-os.com/).

## Install

Linux (x86-64):

```sh
curl -fsSL https://raw.githubusercontent.com/mirkoattiladaniel/nori/main/install.sh | sh
```

This installs `roll`, Nori's project tool, into `~/.nori/bin` and adds it to your `PATH`; `roll` then
installs the newest compiler. Open a new terminal, and:

```sh
roll init hello
cd hello
roll run
```

`roll learn` is an interactive tutorial, `roll update nori` updates the compiler, and `roll --help` lists the
rest. Every release is on the [Releases](https://github.com/mirkoattiladaniel/nori/releases) page.

## Documentation

- [The language reference](docs/language/): every construct of the language, its syntax and its
  semantics, and the project manifest.
- [The standard library](docs/std/): every module of `std`, including the `nori_ui` toolkit with its
  `.ui` and `.style` languages and its plugin ABI.

The same documentation is at [nori-lang.com](https://nori-lang.com/docs/).

## What is here

| Directory | |
|---|---|
| `std/` | the standard library's source, as `import "std/…"` sees it |
| `roll/` | the source of `roll`, the project tool |
| `examples/` | small example programs |
| `docs/` | the language and standard library reference |

The compiler's source is not published yet; the compiler itself comes with every release.

## License

Licensed under either of the Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE)) or the MIT
license ([LICENSE-MIT](LICENSE-MIT)), at your option.

## Contributing

Bug reports and fixes for real defects are welcome; new features and redesigns are discussed in an issue
first. See [CONTRIBUTING.md](CONTRIBUTING.md).
