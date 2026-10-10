# The Nori Language

The **language reference**: every construct of the Nori language, its syntax,
and its semantics. It describes what the compiler (`noric`)
accepts and how programs behave. For the standard library, see
[the standard library reference](../std/).

The code examples are meant to compile with the accompanying `noric`.
Examples are shown as complete programs or as fragments inside `fn main`, and
expected output is given in comments.

---

## Contents

1. [Getting Started](01_getting_started.md): a first program, how source files
   are compiled (`noric`), and `roll` projects.
2. [Lexical Structure](02_lexical_structure.md): comments, identifiers, the
   reserved and contextual keywords, tokens, and source encoding.
3. [Values and Types](03_values_and_types.md): the type system: `Int`, `Float`,
   `Bool`, `Char`, `Str`, sized integers, vector types (explicit SIMD),
   containers, and the built-in generic types; the all-`i64` value model.
4. [Variables and Bindings](04_variables.md): `let`, `var`, `global`, type
   annotations, scope, and the no-block-shadowing rule.
5. [Operators](05_operators.md): arithmetic, bitwise, comparison, logical, and
   string operators, with precedence.
6. [Strings and Characters](06_strings_and_chars.md): `Str`, the strict `Char`
   type, interpolation, escapes, and the text builtins.
7. [Control Flow](07_control_flow.md): `if`/`else` (and `if` as an expression),
   `while`, `foreach`, `match`, `break`/`continue` and loop labels.
8. [Functions and Closures](08_functions.md): `fn`, parameters, `inout`, return
   types, recursion, and `|x| …` closures.
9. [Collections](09_collections.md): `Vec<T>`, `Map<K, V>`, `Set<T>`, fixed arrays (`fill`),
   and `Pool<T>`.
10. [Structs](10_structs.md): declaration, fields, methods (`impl`), and
    structs are handles; `copy` takes an independent value.
11. [Enums and Pattern Matching](11_enums_and_matching.md): sum types, payloads,
    `match`, and the built-in `Option`/`Result`/`Json`.
12. [Traits and Generics](12_traits_and_generics.md): `trait`/`impl`, bounds,
    dynamic dispatch (trait objects), and monomorphized generics.
13. [Modules and Namespaces](13_modules.md): `import`, `import … as`, `::`,
    `pub` visibility, directory modules, and `namespace`.
14. [Error Handling](14_error_handling.md): `Result`/`Option`, `try`, `fail`,
    and error traces.
15. [The Memory Model](15_memory_model.md): value semantics, no aliasing by
    construction, automatic reclamation, and `region`.
16. [Concurrency](16_concurrency.md): `spawn`/`Task<T>`/`await`, `parallel`,
    channels, `lock`, and deadline control flow (`within`/`race`/`hedge`/
    `select`/`until`, `yield`).
17. [Unsafe and FFI](17_unsafe_and_ffi.md): `unsafe`, raw memory (`peek`/`poke`),
    `extern fn`, `cstruct`, inline `asm`, `naked fn`, and C interop.
18. [Conditional Compilation](18_conditional_compilation.md): `cfg(os/arch/libc)`
    and user flags (`--cfg`).
19. [Verification and Contracts](19_verification.md): `law`, performance
    contracts (`noalloc`/`notrap`/`bounded`), `never` blocks, `shadow` functions,
    `versioned` values, `why`, and proof-carrying builds.
20. [Tooling](20_tooling.md): `noric` flags, `roll`, `--test`, `--doc`, `--fmt`,
    the LSP, `--xray`, and `--memprof`.
    Its companion, [The Manifest](manifest.md): every key of `nori.manifest`, the
    file at the root of every project.
21. [Keyword Reference](21_keyword_reference.md): every keyword, contextual or
    reserved, with a one-line meaning and a link.
22. [Grammar Summary](22_grammar.md): a consolidated syntactic overview.
