# 19. Verification and Contracts

Nori treats correctness as a gradient you can dial up per function: from a fuzzed
property, to a compiler-enforced performance contract, to a whole-program policy,
to a machine-checked proof. These features are optional and cost nothing when
unused.

## Laws

A `law` is a universally-quantified property; a check that should hold for *all*
inputs. It reads like a function with `assert`s over its parameters:

```nori,excerpt
law add_commutes(a: Int, b: Int) {
    assert(a + b == b + a)
}

law reverse_twice(v: Vec<Int>) {
    assert(reverse(reverse(v)) == v)
}
```

`roll law` generates inputs, runs each law, and **shrinks** any counterexample to
a minimal failing case; it caches passing laws and tracks a "proof rung"
(stated → fuzzed → proven). `roll law prove <name>` scaffolds a Lean proof
obligation for a law you want to discharge formally. `noric --fuzz-laws` runs them
in a plain build.

## Performance contracts {#performance_contracts}

A contract after a function's return type makes a performance property a
**compile error** if unmet: the guarantee is checked, not hoped for:

- **`noalloc`**: the function (and everything it calls) performs no heap
  allocation.
- **`notrap`**: no operation that could trap (e.g. an unchecked index).
- **`bounded`**: runs in bounded time/space.

An index proves itself in range in two shapes: `foreach i in 0..len(a)`, and a
`while i < a.len()` loop whose `i` starts at a nonnegative literal and is only
ever written as `i = i + k` (`k` nonnegative), and read before the body advances
`i`, since the condition re-establishes the bound only at the top of an
iteration. Those indices carry no runtime check whether or not the function
declares `notrap`; `--erasure-report` prints the verdict and its reason for
every index in the program.

```nori
fn mix(a: Float, b: Float, t: Float) -> Float noalloc {
    return a + (b - a) * t          // ok: no allocation
}

fn scale(v: Float) -> Float noalloc {
    return mix(0.0, v, 0.5)         // ok: calls another noalloc fn
}

// fn build(n: Int) -> Int noalloc { var v: Vec<Int> = vec()  … }  // ERROR: allocates
```

Contracts are transitive: a `noalloc` function may only call `noalloc`
functions. This lets you carve out a real-time or hot path and have the compiler
keep it honest as the code evolves.

## `never` blocks, whole-program policy

A `never` block states a policy over the whole call graph; the build fails if it
is violated. It answers "can X ever reach Y?" across all of the code.

```nori
never { unsafe reached from render_pure }      // render_pure must stay safe
never { alloc inside audio_callback }          // no allocation anywhere under it
never { network after user_logout }            // ordering constraint
```

The forms are `OP reached from ORIGIN`, `OP inside ORIGIN`, and `A after B`.
Policies can also live in the manifest's `[never]` section so they apply
project-wide.

## Shadowing, differential testing

`fn new(…) shadows old` declares `new` as a candidate replacement for `old`.
Built with `--shadow` (or `roll shadow`), the program runs **both** and logs any
divergence, so you can roll out a rewrite with evidence it matches the original.

```nori
fn parse_v1(s: Str) -> Int { return s.len() }                    // the original
fn parse_v2(s: Str) -> Int shadows parse_v1 { return s.len() }   // the candidate
```

## Versioned values, time travel

A `versioned var` records a version on every assignment, so you can move a value
through its own history with `@undo`, `@redo`, `@mark`, and `@goto`:

```nori
versioned var x: Int = 0
x = 10  x = 20  x = 30
printl(to_str(x))            // 30
x = x@undo  printl(to_str(x))   // 20
x = x@undo  printl(to_str(x))   // 10
x = x@redo  printl(to_str(x))   // 20
x = 99                        // an edit truncates the redo future
```

It is backed by structural sharing, so a long history is cheap.

## `why`, causal debugging

`why value` prints the causal chain that produced a value (the sequence of
assignments and operations that led to it), so a surprising result explains
itself. A crash under `--debug` can auto-explain the value involved.

```nori,excerpt
why total          // prints how `total` came to hold its current value
why buf[i]         // works on an element too
```

Built without `--why`, this machinery is entirely absent.

## Proof-carrying builds

`roll build --evidence` embeds an evidence section in the binary recording its
capabilities, the laws it upholds, and the contracts it satisfies; `roll verify`
reads it back. This lets a consumer of a binary see what it claims *without*
rebuilding it.

Know what that is worth. The section is a blob appended to the binary by the
compiler that built it; it is **not signed**, and nothing binds it to the code.
For an artifact you built, or one from a publisher you trust, it is a faithful
record and a genuine audit trail. For a binary handed to you by someone hostile it
is a self-report: they can append whatever section they like. Reading evidence is
therefore not a substitute for controlling what an untrusted artifact is allowed to
do.

## The gradient

Pick the rung per function: leave it plain, add a `law` for a property, a contract
for a performance guarantee, a `never` policy for an invariant, `shadows` to
migrate safely, or a Lean proof for the few things that must be certain. You pay
only for the rungs you use.

---

Next: [Tooling](20_tooling.md).
