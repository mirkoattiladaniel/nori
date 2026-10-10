# examples/systems/kernel: a Nori kernel

A minimal operating-system kernel written in Nori: boots via multiboot, climbs into 64-bit
long mode, and runs the full heap-backed language (`Str`, `Vec`, `Map`, `Float`, `printl`)
at ring 0 with output over COM1.

| file | role |
|---|---|
| `kmain.nori` | the kernel: plain Nori, `fn main()` |
| `boot.S` | multiboot header + 32→64-bit trampoline (paging, GDT, SSE, stack) |
| `boot.nori` | the same trampoline in Nori: `@section` assembly blocks, no `.S` file |
| `knori.nori` | the root of the all-Nori build; imports `boot.nori` and `kmain.nori` |
| `kernel.lds` | memory layout (loads at 1 MiB) |

## Build

```sh
noric --build kmain.nori kernel --freestanding --asm boot.S --lds kernel.lds --flatbin
```

The same kernel with no assembly file at all: the boot trampoline is `boot.nori`, and the
native backend places its blocks itself:

```sh
noric --build knori.nori kernel --native --freestanding --lds kernel.lds --flatbin
```

## Run

```sh
qemu-system-x86_64 -kernel kernel.bin -display none -serial stdio \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04
```

Expected serial output ends with `NoriOS: all systems go`, then QEMU exits via the
debug-exit port.
