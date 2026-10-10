# 13. Modules and Namespaces

A module is a source file (or a directory of files). `import` brings one module's
items into another. Paths are resolved relative to the importing file and against
any configured import roots (`roll` sets these up from your manifest and
dependencies).

## Importing

There are two forms:

```nori,excerpt
import "std/os" as os        // namespaced: refer to items as os::NAME
import "helpers"             // flat: items merge into the current module
```

- **Aliased** (`import … as name`) puts the module behind a namespace; you reach
  its public items with `name::item`.
- **Flat** (`import "…"` with no alias) merges the imported module's items into
  the importer, so you call them unqualified. This is how a multi-file module's
  entry file pulls in its own parts.

```nori
import "std/os" as os

fn main() -> Int {
    if os::path_exists("/tmp") { printl("yes") }   // os:: prefix
    return 0
}
```

Import paths are written **without** the `.nori` extension. A directory module is
imported by its directory (`import "std/os"`, not `"std/os/os.nori"`). If an
import can't be resolved, the compiler reports it clearly rather than failing
obscurely.

## Visibility: `pub`

Items are **private to their module by default**. Only items marked `pub` are
visible to modules that import them. This applies to functions, types, and
globals.

```nori
pub fn public_api() -> Int { return helper() }   // callable by importers
fn helper() -> Int { return 42 }                 // private to this module

pub struct Point { x: Int, y: Int }
pub global DEFAULT_PORT: Int = 8080
```

Reaching a private item across a module boundary is an error, with a fix-it
telling you to mark it `pub`.

## Directory modules

Importing a **directory** loads it as one module. Its root `.nori` files share a
single scope (they can call each other's functions directly, as if one file),
and each subfolder becomes a nested namespace. This lets a large module be split
across files without ceremony; the `std/*` modules are organized this way.

```
std/os/
  os.nori          ← root files: one shared scope
  os_test.nori
```

```nori
import "std/os" as os      // the whole directory, behind `os::`
```

When a directory has multiple root files, all of their `pub` items are exported
(auto-discovery), not just an entry file's.

## Submodules: a library made of modules

A subfolder becomes a namespace *unless the module itself says otherwise*. If one
of the module's root files imports the subfolder **with an alias**, that folder is
a module of its own, with its own scope, its own `pub` surface, and its own build unit,
and the enclosing module is simply one of its consumers.

```
geo/              ← the library
  shapes.nori      ← root files: one shared scope
  mesh_api.nori
  mesh/            ← a module of it, because a root file imports it aliased
    mesh.nori
    simplify.nori
```

```nori,excerpt
// geo/mesh_api.nori — a root file of geo
import "geo/mesh" as geo_mesh
use geo_mesh::*                      // …and its names read as they always did
```

Nothing else declares it: the import *is* the declaration. Anyone else can import
it directly too: `import "geo/mesh" as geo_mesh` from outside geo names
the same module, with one copy of its code and one copy of its globals.

Symbols carry the module's whole path, so `geo/mesh` and `mesh` are
different modules with different symbols and can coexist in one program.

Why split a library this way: each module is a separate unit for
separate compilation, so editing one part rebuilds that
part and its dependents rather than the whole library. Splitting pays when the
pieces are *below* the bulk of the library (a shared low layer everything sits
on), because that is what makes the big shared scope smaller.

## Nested namespaces

A module's entry file groups its parts into sub-namespaces with
`namespace NAME { import "file" … }`; the namespace wraps the imports of the
files whose `pub` items it should expose. Consumers reach those items through the
importing alias as `alias::NAME::name`.

```nori,excerpt
// Three files, shown together because the point is the arrangement between them: two files that
// each define `vec2`, and a module entry that puts them in separate namespaces so both names can
// live. It is not one compilation unit, so it is only parsed, not built.
// core.nori
pub fn vec2(a: Int, b: Int) -> Int { return a + b }

// widget.nori
pub fn vec2(a: Int, b: Int) -> Int { return a - b }   // same name, different namespace below

// ui.nori — the module entry: group files into namespaces
namespace core   { import "core.nori" }
namespace layout { import "widget.nori" }
```

```nori,excerpt
// a consumer
import "ui" as ui
fn main() -> Int {
    printl(to_str(ui::core::vec2(3, 4)))     // 7
    printl(to_str(ui::layout::vec2(3, 4)))   // -1  (distinct symbol, same name)
    return 0
}
```

Nested namespaces are strict: `alias::NAME::name` names the item precisely, the
same name may be reused across namespaces (distinct symbols), and `pub` is
enforced per namespace. This is how a large multi-file module presents an
organized nested API; directory subfolders form namespaces the same way (see
[directory modules](#directory-modules) above).

## `use`

`use` brings a name from a sibling namespace into scope so you can drop the
prefix, either one name or a whole namespace:

```nori
use core::Color        // now `Color` refers to core::Color
use core::*            // bring in all of core's names (fall-through)
```

The first segment may also be an **import alias**, which brings an imported
module's public names into scope unqualified:

```nori,excerpt
import "geo/mesh" as geo_mesh
use geo_mesh::*                      // `simplify`, `Mesh`, … without the prefix
use geo_mesh::simplify               // or just the one name
```

This is what makes splitting a module into submodules cheap: the names move, the
call sites don't.

A sibling is named relative to the **module root**, and that root is whatever the
module was reached as, so the same `use core::*` resolves to `mylib__core` when
the tree is imported as `mylib`, and to plain `core` when the directory itself is
the program being compiled (`noric --check mylib/`, where there is no module name
above the subfolders). Both spellings are tried, module-rooted first, so a module
whose subfolders reference each other checks the same either way.

## Qualifying types and enum variants

An enum variant is named `Enum::Variant`. When the enum comes from an aliased
module (and is not a prelude type), qualify it through the alias:

```nori,excerpt
import "mylib" as lib
let t = lib::Token::Number(42)      // aliased enum variant
```

The prelude enums `Option`, `Result`, and `Json` are the exception: after
importing their module they are named unqualified (`Option::Some`), as noted in
[Enums and Pattern Matching](11_enums_and_matching.md).

## Deprecating what you export

A `pub` name is a promise, and the cost of breaking it lands on people who
cannot see why it changed. Write `deprecated` after a function's return type to
say so at the call site instead:

```nori
pub fn old_hash(s: Str) -> Int deprecated "use `new_hash` — old_hash collided on short keys" {
    return s.len()
}
pub fn new_hash(s: Str) -> Int { return s.len() * 7 }
```

Every call now warns, quoting your message verbatim, which is the whole point, so
name the replacement rather than just the fact:

```text
▲ [1] `old_hash` is deprecated
  app.nori · 3:19

    3 ▏     printl(to_str(old_hash("abc")))
      ▏                   ━━━━━━━━ deprecated by the module that defines it

  ▸ use `new_hash` — old_hash collided on short keys
```

It is a **warning**: the call still compiles, so a deprecation never breaks an
existing build. Methods take the same clause:

```nori
struct Box { n: Int }
impl Box {
    fn size(self) -> Int deprecated "use `count()` — `size` counted bytes" { return self.n }
    fn count(self) -> Int { return self.n }
}
```

The message survives separate compilation; it is part of the signature
`--emit-interface` emits, so a consumer that never reads your source still gets
it. The string is free text and is not parsed; a bare `deprecated` with no
message is allowed and says only that the name is deprecated.

Two calls are deliberately exempt, both inside the library rather than in a
user's code: a deprecated function calling **itself**, and one deprecated
function calling **another**. That second shape is the useful one; an old entry
point kept as a thin wrapper over an old implementation warns its callers
without warning at its own body every time you build the library.

```nori,excerpt
fn old_wrapper(s: Str) -> Int deprecated "use `new_hash`" { return old_hash(s) }   // no warning here
```

## Removing what you exported

`deprecated` needs the declaration to still exist. Once you actually delete a
`pub` name there is nothing left to hang a message on, and your users get the
least useful error a compiler has — "undefined function" — usually with a
did-you-mean pointing at something unrelated.

So the message outlives the code, in the manifest of the package that removed it:

```toml
[removed]
old_hash  = "use `new_hash` — 2.0 removed it (it collided on short keys)"
Box::size = "use `count()` — `size` counted bytes, not items"
```

`roll` reads `[removed]` from **every resolved dependency's** manifest (the
package that deleted the name is the one that knows why) and forwards the
entries to the compiler, which reports them at the call site:

```text
◆ [1] `old_hash` was removed
  src/main.nori · 3:19

    3 ▏     printl(to_str(old_hash("abc")))
      ▏                   ━━━━━━━━ the module that defined it has deleted this name

  ▸ fix · use `new_hash` — 2.0 removed it (it collided on short keys)
```

This is an **error**, unlike `deprecated`; the name really is gone, so the
build was going to fail either way. What the tombstone changes is that it fails
with the replacement named instead of a spelling suggestion.

A method is keyed `Type::name`. Your own manifest's `[removed]` is read too, so
a library can exercise its own tombstones. Write the version into the message:
the entry lives in the version that does not have the name, so a reader who hits
it has already upgraded and wants to know what to write instead.

`roll update deps` reads the tombstones of the versions it just resolved and
tells you **before it writes the lock** which of your lines will stop compiling:

```text
   Warning 3 call sites use a name these versions removed — migrate before building
  ▲ mathx removed `old_hash` — use `new_hash` — 2.0 removed it (it collided on short keys)
      src/main.nori:5
      src/main.nori:7
  ▲ mathx removed `Box::size` — use `count()` — `size` counted bytes, not items
      src/main.nori:6
```

so an upgrade is a decision with the migration in front of you, rather than a
surprise on the next build. That scan is textual and whole-word (`roll` has no
parser), so it can name a line that merely mentions the word; it is a heads-up
to read, not a diagnostic.

Together the three cover a rename's whole life: `deprecated` while the old name
still works, `[removed]` once it is gone, and the `roll update` pre-flight at
the moment you choose to take the change.

## Duplicate definitions and diamonds

Importing the same module by two paths resolves to one copy (a diamond
deduplicates), and defining the same top-level name twice in a module is an error.
`roll` resolves version diamonds across dependencies with minimal-version
selection so a program links one version of each.

---

Next: [Error Handling](14_error_handling.md).
