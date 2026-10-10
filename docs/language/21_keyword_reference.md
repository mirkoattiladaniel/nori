# 21. Keyword Reference

Every Nori keyword, with a one-line meaning and where it is defined. Nori has a
small set of **reserved** words (never usable as identifiers) and a larger set of
**contextual** words (keywords only in a specific position, ordinary identifiers
elsewhere; see [Lexical Structure](02_lexical_structure.md).

## Reserved words

| Word | Meaning | Chapter |
|---|---|---|
| `as` | import alias: `import "…" as name` | [13](13_modules.md) |
| `break` | exit the innermost (or labeled) loop | [7](07_control_flow.md) |
| `continue` | next iteration of a loop | [7](07_control_flow.md) |
| `defer` | run a statement when the enclosing block ends, by any path | [7](07_control_flow.md) |
| `else` | alternative branch; also the fallback operator | [7](07_control_flow.md), [14](14_error_handling.md) |
| `enum` | declare a sum type | [11](11_enums_and_matching.md) |
| `false` | the `Bool` false | [3](03_values_and_types.md) |
| `fn` | declare a function | [8](08_functions.md) |
| `foreach` | iterate a range or collection | [7](07_control_flow.md) |
| `if` | conditional (statement or expression) | [7](07_control_flow.md) |
| `impl` | method / trait implementation block | [10](10_structs.md), [12](12_traits_and_generics.md) |
| `import` | bring in a module | [13](13_modules.md) |
| `in` | the `foreach x in …` separator | [7](07_control_flow.md) |
| `inout` | name a value you may change: write-back for a scalar, permission to mutate for a handle; at a **store** position, share it with the container | [8](08_functions.md), [15](15_memory_model.md) |
| `law` | a universally-quantified property | [19](19_verification.md) |
| `let` | immutable binding | [4](04_variables.md) |
| `lock` | enter a mutex's critical section | [16](16_concurrency.md) |
| `match` | pattern match | [11](11_enums_and_matching.md) |
| `over` | the reduction domain in `reduce … over …` | [16](16_concurrency.md) |
| `reduce` | parallel reduction | [16](16_concurrency.md) |
| `region` | scoped allocation arena | [15](15_memory_model.md) |
| `return` | return from a function | [8](08_functions.md) |
| `schedule` | attach a scheduling strategy to a loop nest | [16](16_concurrency.md) |
| `set` | write-through ownership hint | [15](15_memory_model.md) |
| `sink` | move the value: `sink x`, `sink r.f`, `sink g` on a global (the place is left empty), `match sink x`, `sink` parameter | [15](15_memory_model.md) |
| `spawn` | start a concurrent task | [16](16_concurrency.md) |
| `struct` | declare a product type | [10](10_structs.md) |
| `trait` | declare an interface | [12](12_traits_and_generics.md) |
| `true` | the `Bool` true | [3](03_values_and_types.md) |
| `unsafe` | raw-memory / FFI block | [17](17_unsafe_and_ffi.md) |
| `view` | a read-only second name that owns nothing and is **deep**: nothing reachable through it may change; also `-> view T` results and `f: view T` fields | [15](15_memory_model.md) |
| `copy` | a deep copy with its own owner; changing it never affects the original | [15](15_memory_model.md) |
| `var` | mutable binding | [4](04_variables.md) |
| `while` | conditional loop | [7](07_control_flow.md) |

## Contextual words

These are keywords only in the position shown; anywhere else they are ordinary
identifiers.

| Word | Meaning | Chapter |
|---|---|---|
| `pub` | export an item from its module | [13](13_modules.md) |
| `for` | names the type an `impl` is for: `impl Trait for Type`; a name everywhere else, so `let for = 1` is legal | [12](12_traits_and_generics.md) |
| `global` | a top-level constant | [4](04_variables.md) |
| `threadlocal` | `threadlocal global`: one copy of the global per thread (per CPU in a freestanding image) | [4](04_variables.md), [16](16_concurrency.md) |
| `frozen` | `frozen global`: written only before the first `spawn`, so a task may read it | [16](16_concurrency.md) |
| `naked` | `naked fn`: no prologue or epilogue, for freestanding entry points | [17](17_unsafe_and_ffi.md) |
| `record` | declare a raw memory layout as a type: values are addresses, fields are loads and stores | [17](17_unsafe_and_ffi.md#records) |
| `@section` / `@align` / `@code32` | on a `naked fn`: the section its assembly block is placed in, its alignment, and 32-bit mode | [17](17_unsafe_and_ffi.md) |
| `@irq` | on a function: an interrupt handler, which with everything it calls may neither allocate nor block (on an `extern fn` or a `naked fn`: its author's word that it does not) | [17](17_unsafe_and_ffi.md#irq) |
| `@takes(L)` / `@needs(L)` / `@sleeps` / `@nospin` | on a function: lock effects: what locks it (or its caller) holds, and whether it may block (`@sleeps`) or provably never does (`@nospin`); checked over the whole call graph against a `lockorder` declaration | [17](17_unsafe_and_ffi.md#locks) |
| `cstruct` | a raw-memory record with C layout: fixed-width fields, `cnew`/`cfree`, `s.field` access, views, `sizeof`/`offsetof` | [17](17_unsafe_and_ffi.md) |
| `packed` | `packed cstruct`: the same, with every field at offset 0,1,2,… (alignment 1, no padding): the x86 tables C alignment cannot say | [17](17_unsafe_and_ffi.md#packed-cstruct) |
| `nori` | `extern nori fn`: a Nori-ABI symbol defined in a separately-compiled dependency | [17](17_unsafe_and_ffi.md) |
| `durable` | a global that persists across program runs (`durable var`) | [4](04_variables.md) |
| `migrate` | load an older persisted shape through a converter (`durable var X: T migrate from Old with fn`; a plain `global` may carry clauses too, for `snapshot()` images) | [4](04_variables.md) |
| `transient` | a struct field that is derived state: never persisted, rebuilt empty on load, and outside the schema | [4](04_variables.md) |
| `extern` | declare a C-ABI function / inline C | [17](17_unsafe_and_ffi.md) |
| `from` | `extern fn f(..) -> R from "libz.so.1"`: the shared library a C function is imported from; also `migrate from` | [17](17_unsafe_and_ffi.md), [4](04_variables.md) |
| `cfg` | conditional compilation | [18](18_conditional_compilation.md) |
| `namespace` | group declarations under a sub-name | [13](13_modules.md) |
| `use` | bring a name into scope from a namespace | [13](13_modules.md) |
| `versioned` | a value with undo/redo history | [19](19_verification.md) |
| `protocol` | a two-party conversation as a type; `<role> sends <Type>` steps | [16](16_concurrency.md) |
| `sends` | one step of a `protocol` | [16](16_concurrency.md) |
| `streams` | a `protocol` step that repeats zero or more times | [16](16_concurrency.md) |
| `chooses` | a `protocol` step where one role picks a branch | [16](16_concurrency.md) |
| `await` | wait for a `Task` to finish | [16](16_concurrency.md) |
| `parallel` | fork/join block or parallel loop | [16](16_concurrency.md) |
| `within` | deadline-bounded execution | [16](16_concurrency.md) |
| `race` | run branches, take the first | [16](16_concurrency.md) |
| `hedge` | start a backup only if the primary is slow | [16](16_concurrency.md) |
| `select` | wait on channels and/or a timeout | [16](16_concurrency.md) |
| `until` | async placeholder that upgrades in place | [16](16_concurrency.md) |
| `yield` | publish a best-so-far in a `within` block | [16](16_concurrency.md) |
| `never` | a whole-program policy that must not be violated | [19](19_verification.md) |
| `shadow` / `shadows` | differential replacement of a function | [19](19_verification.md) |
| `deprecated` | warn at every call site of a name you still export | [13](13_modules.md) |
| `with` | an oracle for a shadowed function | [19](19_verification.md) |
| `or` | the second branch of `race`/`hedge` | [16](16_concurrency.md) |

## Not keywords

Core operations (`printl`, `push`, `pop`, `len`, `slen`, `to_str`, `vec`, `map`,
`fill`, `channel`, `mutex`, `spawn`'s partner built-ins, and the rest) are
**built-in functions**, not keywords. They live in the ordinary function
namespace and are documented with the types they operate on.

---

Next: [Grammar Summary](22_grammar.md).
