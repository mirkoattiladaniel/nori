# examples

Small, self-contained Nori programs, each showing one thing. Read them to see the language, or build and
run them to check your toolchain. `tests/check_examples.sh` (in the grain repository) builds and runs every
one of them and compares the output with the `NAME.expected` file next to it, so what this page says is
what happens.

To try one:

```sh
roll init demo && cd demo
cp ../hello.nori src/main.nori      # any single-file example
roll run
```

or build it directly: `noric --build hello.nori hello && ./hello`.

| example | what it shows | run it | expected |
|---|---|---|---|
| [`hello.nori`](hello.nori) | the smallest program: `printl`, string interpolation | `./hello` | two lines, exit 0 |
| [`shout.nori`](shout.nori) | a command-line tool: arguments, reading and writing files, exit codes | `./shout IN OUT` | `shout: 25 bytes from IN to OUT`; OUT holds IN in upper case; exit 2 without arguments, 1 for a missing file |
| [`wordcount.nori`](wordcount.nori) | text handling and `Map` | `./wordcount wordcount.input` | each word with its count, alphabetical, then `words: 10, distinct: 6` |
| [`generics.nori`](generics.nori) | generic functions, enums and structs (`Opt<T>`, `Box<T>`) | `./generics` | `42 hello 7 a 99 5 314`, one per line |
| [`threads.nori`](threads.nori) | threads without data races: `spawn`, `mutex`, `lock`, `sync` | `./threads` | `total: 400000`, exit 0 |
| [`oob.nori`](oob.nori) | memory safety: an out-of-bounds read is a trap, not undefined behaviour. **It fails on purpose.** | `./oob` | `trap: index out of bounds`, exit 1 |
| [`httpd.nori`](httpd.nori) | an HTTP server on `std/http`: routing, path parameter, query string, JSON, 404 | `./httpd 8080`, then `curl localhost:8080/hello/Nori` | `hello, Nori` |
| [`raylib-snake/`](raylib-snake) | C interop: a playable game bound to raylib from its header, with inline-C helpers | `./build-raylib.sh && roll run` | a window with the game (zlib-licensed raylib is downloaded by the script, not shipped here) |
| [`systems/kernel/`](systems/kernel) | a bare-metal kernel in Nori: boots under QEMU with `Str`, `Vec`, `Map` and `Float` at ring 0 | see its README | serial line `NoriOS: all systems go` |
| [`systems/uefi/`](systems/uefi) | a UEFI application (PE32+) that firmware loads | see its README | `hello from Nori on UEFI` on the console |

Speed and low-level control, three more:

| example | what it shows | expected |
|---|---|---|
| [`primes.nori`](primes.nori) | recursion, arrays, a sieve of Eratosthenes | `55` and `10` |
| [`matmul_order.nori`](matmul_order.nori) | `schedule { order ... }`: loop interchange chosen apart from the algorithm | `6144` |
| [`reduce.nori`](reduce.nori) | `reduce(+, 0) over ...`: a declared-associative reduction the compiler may vectorize | `60` |
| [`unsafe_vec.nori`](unsafe_vec.nori) | a growable vector written in Nori itself with `unsafe`, raw memory and `extern fn` | `1000`, `998001`, `332833500` |

Part of the [`nori`](../) repository; see its root for the license and how to contribute.
