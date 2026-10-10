# The plugin ABI

A **plugin** is a shared library a Nori application loads at runtime to add panels and behaviour,
without the application being rebuilt. This page is the contract between the two.

ABI version: **1**, and that is the only version there is. A plugin reporting any other version is
**refused by version** — it does not load, and `err` says so.

```nori
import "std/nori_ui" as ui

let p = ui::plugin::load("plugins/notes.so")
if p.ok { printl(ui::plugin::render(p, "notes.main", 320.0, 420.0)) }
else    { printl(p.err) }
```

## Writing one

A plugin is an ordinary Nori file that exports the entry points below, built with `--shared`:

```nori
import "std/os" as os

global bumps: Int = 0

pub fn nori_plugin_abi() -> Int { return 3 }

pub fn nori_plugin_manifest() -> Int {
    return os::os_cstr("{\"name\":\"notes\",\"version\":\"0.1.0\",\"panels\":[{\"id\":\"notes.main\",\"name\":\"Notes\",\"position\":3}]}")
}

pub fn nori_plugin_init(host_call: Int) -> Int { bumps = 0  return 0 }

pub fn nori_plugin_render(idp: Int, w: Float, h: Float) -> Int {
    return os::os_cstr(("column { padding: 12 text { text: \"bumps: " + (to_str(bumps) + "\" } }")))
}

pub fn nori_plugin_event(idp: Int, actionp: Int) -> Int {
    if os::os_from_cstr(actionp) == "notes/bump" { bumps = bumps + 1 }
    return 0
}
```

```
noric --build-lib notes.nori plugins/notes.so --shared --runtime compiler/runtime.nori
```

The library exports **only** the `nori_plugin_*` symbols; everything else, including the runtime
linked inside it, stays local.

Add `--native` and noric writes the `.so` itself — its own code generator, its own linker, no clang
and no LLVM in the way:

```
noric --build-lib notes.nori plugins/notes.so --shared --native --runtime compiler/runtime.nori
```

The two libraries export the same symbols (`nm -D` agrees) and answer the same calls, and a host
built either way loads either one. Linux x86-64 only.
`--build-lib` without `--shared` has no native path and says so.

## Entry points

| symbol | signature | required |
|---|---|---|
| `nori_plugin_abi` | `() -> Int` — must return 3 | yes |
| `nori_plugin_manifest` | `() -> cstr` | yes |
| `nori_plugin_init` | `(host_call: Int) -> Int` — 0 = ready | no |
| `nori_plugin_render` | `(id: cstr, w: Float, h: Float) -> cstr` | if it declares panels |
| `nori_plugin_event` | `(id: cstr, action: cstr) -> Int` | no |
| `nori_plugin_service` | `(key: cstr, arg: cstr) -> cstr` | if it declares services |
| `nori_plugin_surface` | `(id: cstr, device: Int, queue: Int, w: Int, h: Int) -> Int` | if it declares surfaces |

A plugin exporting no `nori_plugin_init` needs nothing from its host and is ready the moment it
loads; `ui::plugin::calls_back(p)` is what says whether an address will actually be handed over.

`render` returns `.ui` source, which the host parses, lays out, paints, and hit-tests. `action` is
the `on_click` id of the region the user activated.

## The manifest

`nori_plugin_manifest` returns JSON. `name` is required; every panel field except `id` has a default.

```json
{
  "name": "notes",
  "version": "0.1.0",
  "panels": [
    { "id": "notes.main", "name": "Notes", "icon": "", "position": 3,
      "default_w": 280.0, "default_h": 320.0, "closable": true, "shortcut": "" }
  ],
  "services": ["notes.count"],
  "surfaces": ["notes.preview"]
}
```

`position`: 0 left · 1 right · 2 top · 3 bottom · 4 center · 5 floating.

`services` names the keys this plugin **answers** through `nori_plugin_service`, and `surfaces` names
the holes it **fills** through `nori_plugin_surface`. Both are plain string arrays and both are
matched by name — see below.

**Declaring something you cannot deliver is a load error**, not a surprise at frame time. A manifest
with `panels` and no `nori_plugin_render`, with `services` and no `nori_plugin_service`, or with
`surfaces` and no `nori_plugin_surface`, comes back `ok = false` with the reason in `err`. All three
rules are unconditional.

## Only scalars and C strings cross

**This is the rule that matters.** A plugin `.so` carries its own copy of the runtime, so it has its
own heap and handle tables. A Nori `Str`, `Vec`, or any other handle is an index into a per-process
table — a handle minted inside the plugin means nothing to the host, and vice versa.

So every value crossing the boundary is an `Int`, a `Float`, or a pointer to a NUL-terminated UTF-8 C
string (also passed as `Int`). Convert at the edges:

- `os::os_cstr(s: Str) -> Int` — a Nori string out to a C string
- `os::os_from_cstr(p: Int) -> Str` — a C string in to a Nori string

This is why the signatures above take and return `Int` where they read as strings.

## Failure is reported, never fatal

`ui::plugin::load` returns a `LoadedPlugin` with `ok = false` and `err` set — it does not trap — when
the file will not open, is not a plugin, reports a different ABI version, has an unreadable manifest,
or declares something it has no entry point for. `ui::plugin::scan(dir)` returns the failures
alongside the successes, so a host can show what refused to load rather than silently dropping it.

A plugin that returns malformed `.ui` costs only its own panel: the parse error is reported and drawn
there.

## Hot reload

`changed(p)` compares the plugin file's size and mtime against the load — a `stat`, cheap enough to
ask every frame. `reload(p)` closes the library and loads the same path again; `reload_changed(ps)`
does it for a whole list and returns how many moved.

```nori
import "std/nori_ui" as ui

fn poll_reloads(inout ps: Vec<ui::plugin::LoadedPlugin>) {
    if ui::plugin::reload_changed(ps) > 0 { printl("plugins reloaded") }
}
```

All state inside the old library goes with it — the plugin owns its own heap. The new file must be a
*new* file: a writer that overwrites the `.so` in place, keeping its inode, can be handed straight
back by the dynamic loader. `noric` writes a replacement, which is what makes this work.

## Calling back into the host

Without an address the conversation is one-way: the host calls in, and a plugin that needs to ask the
host something has to be told everything up front. Two editor panels proved that is not enough — each
ended up building a private service registry inside its own `.so` and bridging over invented channels
(`render("svc:KEY:ARG")` out, `event(panel, "scene:[…]")` in).

The answer is **one address, handed over at init**:

```
nori_plugin_init(host_call: Int) -> Int
```

`host_call` is the address of a host function with the C signature `(key: char*, arg: char*) -> char*`
— the same JSON-in / JSON-out service call the host bus already speaks, so the whole bus rides on one
callback and nothing new has to be versioned when a service is added.

```nori
import "std/os" as os
import "std/dl" as dl

global g_host: Int = 0

pub fn nori_plugin_abi() -> Int { return 3 }
pub fn nori_plugin_init(host_call: Int) -> Int { g_host = host_call  return 0 }

pub fn nori_plugin_render(idp: Int, w: Float, h: Float) -> Int {
    let reply = dl::host_call(g_host, "scene.names", "{}")     // -> {"ok":true,"result":[…]}
    return os::os_cstr(("text { text: \"" + (reply + "\" }")))
}
```

`dl::host_call` is the whole plugin side. It copies the reply on the spot, and answers a parseable
`{"ok":false,"error":…}` for a null address rather than trapping — so a plugin never has to guard the
call, and one rendered before `init` ran degrades to a visible error instead of a crash.

The host side is `ui::plugin::host_new()`, which binds its own service bus to the callback, and
`ui::plugin::init(p)`, which hands the address over. Neither takes an argument for it:

```nori,excerpt
var h = ui::plugin::host_new()                                   // now IS the callback's bus
ui::plugin::host_register_service(h, "scene.names", |a| names_json())
var p = ui::plugin::load("plugins/hierarchy.so")
ui::plugin::init(p)                                              // hands over host_call_addr()
```

A `PluginHost`'s vectors are handles, so the binding is live: a service registered *after* the plugin
loaded is reachable over the callback with no rebinding. `host.services` is answered by the host
itself and returns the published keys, so a plugin can discover its host rather than assume it. An
unknown key answers exactly what `host_invoke_service` answers in-process:

```json
{"ok":false,"error":"no such service: no.such.key"}
```

### Where the address comes from

This is the reason the callback was deferred as long as it was. A whole-program `noric --build`
**inlines a top-level `pub fn` away and leaves no symbol** — `nm` on a built host finds `_start`,
`nori_rt0`, and the allocator, and nothing else, with or without `-rdynamic`. `--build-lib --shared`
does keep its exports, so the proposed route was to split every host into a library plus a thin entry
point.

**That split is not needed, because an address does not have to be a symbol — it only has to be
taken.** `cfn_addr(f)` is the address of a C-callable entry the compiler generates for the top-level
function `f` (see [Unsafe and FFI](../../language/17_unsafe_and_ffi.md)), and taking it keeps the body
alive. The entire mechanism is one line in `std/nori_ui/plugin/hostcall.nori`:

```nori,excerpt
pub fn hostcall_entry(kp: Int, ap: Int) -> Int { … }     // (key: cstr, arg: cstr) -> cstr
pub fn hostcall_addr() -> Int { var r = 0  unsafe { r = cfn_addr(hostcall_entry) }  return r }
```

An ordinary whole-program host keeps working, and the callback is still absent from the host's
*dynamic* symbol table — nothing is exported to get it. The same host builds with `noric --build
--native`: it is then a glibc-hosted executable (std/dl imports `dlopen` from `libc.so.6`) and loads
plugins built through LLVM, calling into them and being called back exactly as the LLVM host is.

Because a C-ABI function pointer has no context parameter, the bus is a module global: **one
plugin host per process**, which is the assumption the dock already makes. `host_new` opens it
empty, so nothing an earlier host published answers for a new one; `hostcall_unbind()` takes it
away, after which the callback answers `{"ok":false,"error":"the host has not bound a service bus"}`,
and `host_bind_hostcall(h)` opens it again.

## The host calling into the plugin

The callback gives the plugin an address to call its host with. The other direction exists for the
two things **only the plugin can produce**: an answer to a service key, and the pixels of a viewport.

```
nori_plugin_service(key: cstr, arg: cstr) -> cstr
nori_plugin_surface(id: cstr, device: Int, queue: Int, w: Int, h: Int) -> Int
```

Both are dispatched **by name**. The host walks the services a plugin declared and the surfaces a
document declared, matches strings, and calls through. It never learns what any of those names mean
— and that is the entire point: it is what lets one `nori_ui` binary be a game editor, an image
editor or a browser depending only on what is in its plugin directory. The last piece of domain
knowledge a shell was carrying was a hand-written bridge from `scene_3d.*` onto the bus; a plugin
now brings its own services with it.

### Answering a service

`services` is the list of keys the host will ask this plugin, so it needs an answer:

```nori
import "std/os" as os

global notes: Int = 3
global g_last: Int = 0

pub fn nori_plugin_abi() -> Int { return 3 }

pub fn nori_plugin_manifest() -> Int {
    return os::os_cstr("{\"name\":\"notes\",\"version\":\"0.3.0\",\"services\":[\"notes.count\",\"notes.add\"]}")
}

/// hand back a reply, freeing the PREVIOUS one — at most one is ever outstanding.
fn reply(s: Str) -> Int {
    let p = os::os_cstr(s)
    if g_last != 0 { unsafe { free(g_last) } }
    g_last = p
    return p
}

pub fn nori_plugin_service(kp: Int, ap: Int) -> Int {
    var key = ""
    if kp != 0 { key = os::os_from_cstr(kp) }
    var arg = "{}"
    if ap != 0 { arg = os::os_from_cstr(ap) }
    if key == "notes.count" { return reply(("{\"ok\":true,\"result\":" + (to_str(notes) + "}"))) }
    if key == "notes.add"   { notes = notes + 1  return reply("{\"ok\":true}") }
    return reply(("{\"ok\":false,\"error\":\"no such service: " + (key + "\"}")))
}
```

The host side is one call, and after it the plugin's keys are ordinary bus entries:

```nori,excerpt
var h = ui::plugin::host_new()
let i = ui::plugin::host_add(h, ui::plugin::load("plugins/notes.so"))

ui::plugin::host_has_service(h, "notes.count")               // true — host_add published it
ui::plugin::host_invoke_service(h, "notes.count", "{}")      // {"ok":true,"result":3}
```

`host_add` publishes a plugin's declared keys onto the bus, each routed into the library. A builtin
panel, another `.so` over the host callback, or the host itself then calls those keys with **no idea
a `.so` is behind them** — a caller cannot tell whether a key is answered by the host, a builtin, or a
plugin, which is the same JSON-in / JSON-out contract every other bus call speaks.

`ui::plugin::service(p, key, arg)` is the direct call, without a host. It normalises an empty argument
to `{}`, and a plugin that cannot answer at all — no `nori_plugin_service`, or a failed load —
answers the shared error shape rather than `""`, so a caller has exactly one thing to parse:

```json
{"ok":false,"error":"no such service: notes.count"}
```

A plugin that returns NULL from an entry point it does have answers in the same shape, naming
itself: `{"ok":false,"error":"notes returned nothing for notes.count"}`. `ui::plugin::serves(p)` is
true when the plugin exports the entry point at all.

### Asking for the pointer

A host sends a plugin `press:`, `dragto:`, `release:`, `key:` and `char:` events for the **widgets**
in its document — a `text_input`, a `text_area`, a `code_editor` — addressed by the widget's id and
carrying widget-local coordinates. A container can ask for the same treatment by naming itself:

```
column { on_click: "pointer:transcript" … }
```

The host reads the id out of that action exactly as it reads one out of a widget's, so the column
receives `press:transcript:<lx>:<ly>:<w>:<h>:<clicks>:<mods>`, the drags that follow, and the
keyboard while it holds focus. Nothing else changes: it is still an ordinary container, drawn and
laid out like one.

It is for the surfaces that are not a widget. The chat transcript is styled text with links in it
and a code block in the middle — there is no node kind for that — and selecting it means turning a
press into a line and a byte of the panel's own wrapped lines. A panel that wants a drag over
something it drew itself asks for it here rather than waiting for a widget to be invented.

### The reserved keys a shell asks about

Three service keys are asked of **every** plugin rather than routed by name, so that a shell can
collect what its plugins offer without knowing which plugins exist. A plugin that does not answer
them contributes nothing and is not an error.

| key | asked for | answered with |
|---|---|---|
| `shell.menu` | menu rows and the chords that run them | `{"result":[{"menu","label","action","arg","key"}]}` |
| `shell.settings` | the settings this plugin owns | `{"result":[{"key","label","kind","default","options","group","help"}]}` |
| `shell.pickers` | the picker scopes this plugin can answer | `{"result":[{"scope","note","service","key","row"}]}` |
| `shell.startup` | one thing to do the moment the dock exists | `{"result":"<action>"}` |

In each case the shell fills in which plugin answered; a declaration never names itself.

### Landing somewhere: `shell.startup`

A plugin cannot arrange what you see first. `nori_plugin_init` runs before there **is** a dock, and
`nori_plugin_render` runs only for panels that are already visible — so a panel that wants to be the
one you land on can neither ask early enough nor, sitting behind another tab, ask at all.

The reply is an action string in the vocabulary the menu bar speaks, and two of those belong to the
shell itself:

| action | means |
|---|---|
| `panel:<id>` | bring that panel to the front of whichever leaf holds it |
| `screen:<id>` | **give that panel the whole window**, and put the dock away |
| `""` | nothing, thank you — which is also what not answering the key says |

`screen:` exists because a welcome screen cannot be a panel. Shown as a tab it arrives with a file
tree of nothing beside it and a build panel with nothing to build underneath, and the question it is
asking — which project — is one ninth of the window. While a screen is up the dock is not laid out,
not drawn and not hit-tested; the panel is rendered at the full window rect and its hit regions are
the only ones in the frame. The dock's **state** is untouched throughout, so putting the screen away
gives back the arrangement that was there, saved layout and all.

`shell.screen` raises and dismisses one from the bus at any time — a panel id, or nothing to dismiss:

```nori,excerpt
let _r = dl::host_call(g_host, "shell.screen", "")          // put it away
let _s = dl::host_call(g_host, "shell.screen", "welcome.panel")
```

**An empty argument arrives as `{}`.** `host_call(h, key, "")` does not reach the other side as an
empty string: the bus substitutes an empty JSON object, which is the same "no argument" every
reserved key above is asked with. A service that distinguishes "nothing" from "a name" has to read
`{}` as nothing — the first version of `shell.screen` did not, took `{}` for a panel id, answered
`no such panel`, and left the launcher up over the project it had just opened.

A flag written by hand beats a screen, for the same reason it beats a `panel:` preference: `--tab`
names a tab, and a screen is drawn over every tab there is, so asking for one dismisses the screen.

### Providing a picker scope

The picker — the palette on Ctrl+P — is a **box the shell draws and a plugin fills**. The shell owns
the modality, the ranking, the prompt and the preview pane; it owns no scopes, knows no file
extensions and has no list of commands. A plugin declares what can be picked:

```json
// in the reply to `shell.pickers`
{"scope":"file","note":"every file in the project"}
{"scope":"font","note":"every font installed on this machine"}
{"scope":"all","row":"false","key":"ctrl+p"}
```

`note` is what the row offering the scope says beside it. `service` is the key the shell calls for
the rows, defaulting to `picker.rows`. `key` is a chord that opens the picker already narrowed to
this scope, spelled as `shell_key_name` spells one — **the chord comes with the scope**, so a shell
with no picker provider loaded has no Ctrl+P, which is correct: there would be nothing to open.
`row: "false"` keeps a scope out of the list that offers the scopes, which is what the pseudo-scope
`all` — the unscoped list itself — uses so that its chord has somewhere to be declared.

Then three services answer everything else:

| key | argument | answers |
|---|---|---|
| `picker.rows` | the scope (`file`, `preference::editor`) | `{"result":[{"label","detail","note","action"}]}` |
| `picker.preview` | the selected row's action | `{"result":{"kind":"lines" or "image","of","lang","path","lines"}}` |
| `picker.resolve` | `<scope-so-far>\t<word>` | `{"result":"<scope>"}` — "" when the word is not one |

A row is drawn as `detail/label` with `note` at the right, and its `action` is dispatched exactly
like a menu row's: `KEY\targ` onto the bus. So a row that opens a file is `code.open\t/path`, and a
row that narrows the list further is `picker.scope\tpreference::editor` — a scope is picked the same
way it is typed.

`picker.resolve` is what makes `prefs::` and `settings::` the same scope and `preference::editor::` a
narrowing: the shell asks the providers what a typed word means rather than holding a table of
synonyms it would have to be rebuilt to extend.

`picker.preview` is asked of every provider with the row's own action, because in the unscoped list a
file row, a command row and a setting row sit beside one another and only whoever produced a row can
describe it. A provider that does not recognise an action answers `{"result":{}}`.

Host-side this is `ui::shell::shell_picker_scopes(h)`, `shell_picker_rows`, `shell_picker_preview`,
`shell_picker_resolve` and `shell_picker_chord` — the same shape as `shell_menu_items`, and for the
same reason.

### Filling a surface

`surfaces` is the named holes this plugin offers to fill. A document declares a hole with
`surface { id: "scene.viewport" … }` (see [the UI DSL](ui_dsl.md)); a plugin claims one by
**declaring the same name in its manifest**, and nothing wires the two together.

```nori
import "std/os" as os

fn render_scene(device: Int, queue: Int, w: Int, h: Int) -> Int { return 0 }

pub fn nori_plugin_surface(idp: Int, device: Int, queue: Int, w: Int, h: Int) -> Int {
    if os::os_from_cstr(idp) == "scene.viewport" { return render_scene(device, queue, w, h) }
    return 0
}
```

The return is a **GPU texture view handle**, and `0` means "nothing to show this frame" — not an
error. The host then leaves the hole on `surface_placeholder_color()`, which is a visible colour on
purpose: an unfilled surface should look wrong rather than look like a background.

`device` and `queue` are **borrowed**. There is one `WGPUDevice` per process and `ui_gpu` owns it;
the plugin allocates its own targets on them and must release **neither**. Everything crossing here
is a scalar, so the C-string-only boundary is not widened — a GPU handle is an integer like any other,
and `w`/`h` are the hole's pixel size this frame.

`ui::plugin::surface(p, id, device, queue, w, h)` is the wrapper. It answers 0 without calling
through for a plugin that does not export the entry point and for a degenerate size (`w <= 0` or
`h <= 0`) — a zero-width hole cannot own a texture.

Host-side, three functions and one loop:

```nori,excerpt
ui::plugin::fills_surface(p, "scene.viewport")   // does this plugin claim that name?
ui::plugin::host_surface_owner(h, id)            // which library index claims it, or -1
ui::plugin::host_surface_ids(h)                  // every hole any plugin offers, deduplicated

ui::shell::shell_fill_surfaces(h, overlay, bindings, device, queue)
```

`host_surface_owner` answers **-1 when nothing claims the id**, which is how a hole keeps its
placeholder. First claim wins, and a second claimant is recorded as a host error rather than silently
overriding: two plugins fighting over one hole is a misconfiguration, and the frame that shows it
should say so. `host_surface_ids` is discovery, not policy — what the host *could* fill; what it
actually fills is whatever this frame's documents asked for.

`shell_fill_surfaces` is the whole generic path: it walks the holes in a laid-out document, matches
each id against what the loaded plugins declared, hands over the borrowed device and the hole's pixel
size, and binds whatever handle comes back. There is no branch in it for 3D, for an image canvas, or
for a web view. A hole is a name and a rect, and the plugin that claimed the name decides what
appears in it.

## Who owns the strings that cross

Two images, two heaps, two `malloc`s — **a block allocated by one side can never be freed by the
other.** One rule, applied in both directions, with the sides swapped:

**Plugin calling the host** (`host_call`):

- **arguments belong to the caller.** The plugin allocates `key` and `arg` with `os_cstr`, the host
  copies them in with `os_from_cstr` and keeps no pointer, and the plugin frees them once the call
  returns. `dl::host_call` does all three.
- **the reply belongs to the host, and the plugin must NOT free it.** The host frees the *previous*
  reply at the start of the next call, so at most one reply is ever outstanding — bounded, rather
  than a leak per call. The plugin therefore has to copy the reply before it calls again, which
  `dl::host_call` does immediately.

**Host calling the plugin** (`nori_plugin_service`, and `nori_plugin_manifest` / `nori_plugin_render`
before it):

- **the arguments belong to the HOST.** `key` and `arg` are allocated host-side and freed host-side
  once the call returns; the plugin must copy them and keep no pointer.
- **the reply belongs to the PLUGIN**, and the host must NOT free it. The host copies it immediately,
  on the same line, in `dl::call2_cstr`. The plugin therefore has to keep its reply alive until at
  least its next call; freeing the *previous* reply at the start of each call, as `reply()` above
  does, keeps at most one outstanding rather than leaking one per call.

`nori_plugin_surface` widens nothing: `id` is a host-owned C string on the same terms, and everything
else crossing is a scalar. `device` and `queue` are borrowed handles the plugin must not release, and
the texture view it returns is the plugin's — the host binds it for the frame and does not destroy it.

One consequence, and it is the only sharp edge: **a host service must not call back into the plugin
that is calling it.** A reentrant call would free the outer reply buffer before the outer caller had
read it. Services return data; they do not re-enter.

## Version checking

`ABI_VERSION()` is **1**, and a plugin is accepted only when `nori_plugin_abi()` returns exactly
that. Anything else is refused before the manifest is read, with the reason in `err`:

```
ABI 0, host speaks 1
ABI 2, host speaks 1
```

## The host side

`ui::plugin::host_*` is one registry for panels from both sources — a loaded `.so` and a **built-in**
compiled into the application. After registration nothing distinguishes them:

```nori,excerpt
var h = ui::plugin::host_new()

ui::plugin::host_register_service(h, "app.name", |a| "{\"ok\":true,\"name\":\"shell\"}")

// a panel written right here
ui::plugin::host_register_builtin(h,
    ui::plugin::panel("about.main", "About", ui::plugin::POS_RIGHT()),
    |id, w, hh| "text { text: \"hello\" }",
    |id, action| 0)

// every plugin in a directory
let n = ui::plugin::host_load_dir(h, "plugins")

// draw them all — no branch on where each came from
var ids = ui::plugin::host_panel_ids(h)
```

`host_render(h, id, w, h)` returns the panel's `.ui` source. `host_queue_event` / `host_drain_events`
carry actions back. `host_reload_changed(h)` reloads changed libraries and re-registers their panels.

**Services** take a JSON document and return one. An unknown key answers
`{"ok":false,"error":"no such service: …"}` rather than an empty string, so a caller can parse either
outcome. The bus is the process's one registry, in `hostcall.nori`: a built-in panel publishes
onto it through `host_register_service`, and **a loaded plugin reaches the SAME bus** over the
callback `host_new` opened and `init` handed it; `host_call_addr()` is the address itself, for a host
that wants to drive `nori_plugin_init` by hand. **And a serving plugin publishes ONTO that bus**:
`host_add` registers its declared keys as ordinary entries, each answered by the library itself, so
`host_invoke_service` reaches into a `.so` with nothing at the call site saying so; a reload
publishes them again against the new handle.

**Surfaces** are the same idea for pixels. `host_surface_owner(h, id)` is the library that claims a
hole or -1, `host_surface_ids(h)` is every hole any loaded plugin offers, and
`ui::shell::shell_fill_surfaces(h, overlay, bindings, device, queue)` is the loop that walks a
document's holes and fills the ones a plugin claimed. None of it names a domain.

**A panel that fails costs only itself.** One that produces nothing has the failure recorded against
it and returns ""; `host_set_panel_error` records a parse failure the caller found; `error_document`
builds a valid `.ui` card to draw in the hole.

## See also

- [`std/dl`](../dl.md) — the loading primitives underneath: `open`, `sym`, `close`, the `call*`
  functions, and `host_call`, the plugin side of the host callback.
- [the UI DSL](ui_dsl.md) — `surface { id: … }`, the hole in a document that a plugin fills.
- [`std/nori_ui/plugin`](plugin.md) — the host side: loading panels and the service bus.
