# std/nori_ui::plugin

```nori
import "std/nori_ui" as ui          // then ui::plugin::…
```

The host callback: the address a loaded plugin uses to call back into its host.

Without it the conversation is one-way (the host calls `nori_plugin_render` / `nori_plugin_event`
and that is all), so a plugin that needs to ask its host something has to be told everything up
front. The callback gives the plugin an address to call instead of a private service registry
and ad-hoc channels such as `render("svc:KEY:ARG")` or `event(panel, "scene:[…]")`.

##### Why the address is taken, not exported

Handing a plugin a callback means handing it the address of a host function, and a whole-program
`noric --build` keeps no symbol for one: a top-level `pub fn` is inlined away and its name is gone
from the executable entirely (`nm` on a built host finds `_start`, `nori_rt0`, and the
allocator, nothing else). `--build-lib --shared` does keep its exports, but splitting the host
into a library plus a thin entry point is not needed.

**An address does not have to be a symbol, it only has to be taken.**
`cfn_addr(hostcall_entry)` is the address of a C-callable entry for the Nori callback, generated
beside it; taking it keeps the body alive, so an ordinary whole-program host keeps working, built
with the default backend or with `--native`.

##### The shape

One address, handed over at init, invoked as `(key: cstr, arg: cstr) -> cstr`. That is the same
JSON-in / JSON-out service call the host bus already speaks, so the whole bus rides on one
callback and nothing new has to be versioned when a service is added.

##### Who owns the strings

Two images, two heaps, two `malloc`s: a block from one cannot be returned to the other. So:

  * **arguments belong to the caller.** The plugin allocates `key` and `arg`, the host copies them
    with `os_from_cstr` and keeps no pointer, and the plugin frees them when the call returns.
  * **the reply belongs to the host**, and the plugin must not free it. The host frees the
    previous reply at the start of the next call, so at most one reply is ever outstanding
    (bounded, rather than a leak per call). The plugin therefore has to copy the reply before it
    calls again, which `dl::host_call` does immediately, on the same line.

One consequence, and it is the only sharp edge: **a host service must not call back into the
plugin that is calling it.** A reentrant call would free the outer reply buffer before the outer
caller had read it. Services return data; they do not re-enter.
### `fn hostcall_bind()`

open the bus for the callback. Idempotent; what is already published stays.

### `fn hostcall_reset()`

empty the bus and open it, which is what a new host does, so nothing an earlier host published answers
for it.

### `fn hostcall_unbind()`

take the bus away again. A callback invoked afterwards answers an error rather than a service
that belonged to a host that is gone.

### `fn hostcall_bound() -> Bool`

true once a host has bound its bus.

### `fn hostcall_register(key: Str, sink f: Fn(Str) -> Str)`

publish a service on the bus. Re-registering a key replaces its answer.

### `fn hostcall_has(key: Str) -> Bool`

true when the key is published.

### `fn hostcall_count() -> Int`

how many keys are published.

### `fn hostcall_key_at(i: Int) -> view Str`

the published key at `i`, in registration order.

### `fn hostcall_no_service(key: Str) -> Str`

the reply for a key that is not published: the one error shape both sides already speak.

### `fn hostcall_route(key: Str, arg: Str) -> Str`

route one `(key, arg)` to the bus: the reply of the service published under `key`, or the shared
error shape when nothing is.

### `fn hostcall_entry(kp: Int, ap: Int) -> Int`

**the callback itself**, what a plugin's `nori_plugin_init` is handed the address of.

C-ABI: two `char*` in, one `char*` out, both sides `Int`. Nothing here traps. A null pointer, an
empty key, an unbound bus and an unknown key all come back as a parseable JSON error, because the
caller is across a `.so` boundary and a trap there takes the whole process with it.

### `fn hostcall_addr() -> Int`

the address to hand a plugin: what `nori_plugin_init` receives. Never 0.

### `fn hostcall_self_test(key: Str, arg: Str) -> Str`

invoke the callback the way a plugin would, without a plugin. Useful in tests, and for anyone
who wants to check a service answers before shipping it behind a `.so`.


The plugin host: one registry of panels, whatever they were written as.

A panel arrives from one of two places, a **loaded** `.so` (see `loader.nori`) or a **built-in**
compiled into the application, and after registration the host cannot tell them apart. Both are
a `PanelDesc` plus a way to produce `.ui` source; the host lays that source out.

Nothing here knows what a plugin was written in; if this file ever needs to name a tier, the
design has gone wrong.
### `fn SRC_BUILTIN() -> Int`

a panel compiled into the application.

### `fn SRC_LOADED() -> Int`

a panel from a loaded `.so`.

### `fn POS_LEFT() -> Int`

where the dock should place a panel by default.

### `struct PanelEvent`

a panel-scoped action on its way back to whoever owns the panel.

### `fn panel(id: Str, name: Str, position: Int) -> PanelDesc`

build a `PanelDesc` with the usual defaults, the built-in equivalent of a manifest entry.

### `struct PluginHost`

everything registered: the panels and how to reach each one, the loaded libraries behind some of
them, the pending events, and what has gone wrong. The service bus lives in `hostcall.nori`,
one per process, and the host publishes into it.

The per-panel arrays are parallel and indexed together. A panel's renderer is its own closure, not
a dispatcher shared by everything from the same source: two built-ins are as independent as two
separate libraries.

### `fn host_new() -> PluginHost`

an empty host.

### `fn host_bind_hostcall(h: PluginHost)`

open the host callback's bus again after `hostcall_unbind`. `host_new` already opens it; the
bus is the process's, so what this host published and what the callback answers are one list.
One process, one bound host: the callback is a C-ABI function pointer and has no context
parameter to carry a host in.

### `fn host_call_addr() -> Int`

the address a plugin's `nori_plugin_init` is handed. `ui::plugin::init(p)` passes it for you.

### `fn host_register_builtin(inout h: PluginHost, sink d: PanelDesc, sink rf: Fn(Str, Float, Float) -> Str, sink ef: Fn(Str, Str) -> Int)`

register a panel implemented in this application. `rf` returns `.ui` source for the panel; `ef`
receives an action id and returns 0 when it handled it.

### `fn host_add(inout h: PluginHost, p: LoadedPlugin) -> Int`

take a plugin the loader opened and register every panel it declares. A plugin that failed to
load contributes nothing but its error. Returns the library's index, or -1 when it was refused.

### `fn host_surface_owner(inout h: PluginHost, id: Str) -> Int`

which loaded library fills the hole named `id`, or -1 when nothing claims it.

First claim wins, and a second claimant is an error the host reports rather than a silent
override: two plugins fighting over one hole is a misconfiguration, and the frame that shows it
should say so.

### `fn host_surface_ids(h: PluginHost) -> Vec<Str>`

every hole any loaded plugin offers to fill, in load order and without duplicates. A host uses
this to know what it could fill; what it actually fills is whatever this frame's documents asked
for. Discovery, not policy.

### `fn host_load_dir(inout h: PluginHost, dir: Str) -> Int`

load every plugin in `dir` and register them. Returns how many loaded; the rest are in the error
list, with the path that refused.

### `fn host_panel_count(h: PluginHost) -> Int`

how many panels are registered.

### `fn host_panel_at(h: PluginHost, i: Int) -> view PanelDesc`

the panel at `i`, in registration order.

### `fn host_panel_source(h: PluginHost, i: Int) -> Int`

where a panel came from: `SRC_BUILTIN` or `SRC_LOADED`.

### `fn host_panel_index(h: PluginHost, id: Str) -> Int`

index of the panel with this id, or -1.

### `fn panel_base_id(id: Str) -> Str`

`term.panel#2` -> `term.panel`; anything without an instance marker is returned unchanged.

`#` because it cannot appear in a panel id a manifest declares (those are dotted names), so an
instance can never be mistaken for a panel somebody meant to declare.

### `fn panel_instance(id: Str) -> Int`

the instance number in `term.panel#2`, or 1 for a plain panel id.

### `fn host_panel_service(h: PluginHost, id: Str, key: Str, arg: Str) -> Str`

ask one panel's plugin, rather than whoever holds the key on the bus.

The bus is a flat map from key to function and a later registration replaces an earlier one, so
a key two plugins both offer belongs to whichever loaded last. That is right for a key with one
answer (`shell.theme`, `project.root`) and wrong for a key that is inherently per panel.

`panel.cursor_at` is the case: the shell asks the panel under the pointer what cursor to show,
every panel with links wants to answer, and only one of them could. The others were not shadowed
loudly; they simply never ran.

A panel whose plugin does not offer the key gets the same shape an unknown key gets, so the
caller cannot tell "no such panel" from "that panel says nothing", which is the same thing to
everyone above this line.

### `fn host_panel_serves(h: PluginHost, id: Str, key: Str) -> Bool`

does the plugin that owns this panel declare this service key?

`host_has_service` is a question about the bus (is anyone, anywhere, answering this), and a
shell that asks it and then calls `host_panel_service` on whichever panel is under the pointer
sends the key into a plugin that never claimed it. Every plugin answers an unknown key
correctly, so nothing breaks visibly; what it costs is a service dispatch per panel per frame,
and some plugins do real work on the way in (a debugger pumping its session, a terminal
draining its pty). Neither should happen because a pointer crossed a button.

Reads the plugin's own `services` array from its manifest, which is where a plugin says what it
answers, so a panel that has not claimed the key is never asked.

### `fn host_has_panel(h: PluginHost, id: Str) -> Bool`

true when a panel with this id is registered.

### `fn host_panel_ids(h: PluginHost) -> Vec<Str>`

every registered panel id, in registration order; what seeds the dock's tabs.

### `fn host_panel_ids_at(h: PluginHost, position: Int) -> Vec<Str>`

the ids a given position bucket should hold, for seeding a dock layout.

### `fn host_render(inout h: PluginHost, id: Str, w: Float, hgt: Float) -> Str`

`.ui` source for a panel this frame, from whichever side owns it.

A panel that yields nothing has its error recorded against itself and returns ""; every other
panel still draws. The caller decides what to show in the hole; `error_document` builds one.

### `fn host_set_panel_error(inout h: PluginHost, id: Str, msg: Str)`

record that a panel's document could not be used, such as a parse failure the caller found. It is kept here so
the failure travels with the panel rather than with whoever happened to notice.

### `fn host_panel_error(h: PluginHost, id: Str) -> Str`

the last thing that went wrong with this panel, or "".

### `fn error_document(title: Str, msg: Str) -> Str`

`.ui` source for a card describing a failure: what to draw where a panel could not.

### `fn ui_escape(s: Str) -> Str`

quote a string for a `.ui` attribute: backslash and double-quote are the two that matter.
Byte-wise, so a multi-byte character passes through as its own bytes.

### `fn host_queue_event(inout h: PluginHost, panel: Str, action: Str)`

queue an action for the panel that owns it; called from a `HitRegion`'s `on_click`.

### `fn host_drain_events(inout h: PluginHost) -> Int`

deliver every queued action to its panel, then clear the queue. Returns how many were delivered.

### `fn host_register_service(inout h: PluginHost, key: Str, sink f: Fn(Str) -> Str)`

publish a service. Re-registering a key replaces it.

### `fn host_has_service(h: PluginHost, key: Str) -> Bool`

true when the key is published.

### `fn host_invoke_service(h: PluginHost, key: Str, arg: Str) -> Str`

call a service with a JSON argument, returning its JSON reply.

An unknown key returns a JSON error rather than trapping: asking for a service this build does
not have is normal, and the caller gets something it can parse either way.

### `fn host_service_count(h: PluginHost) -> Int`

how many keys are published. A shell prints this to say how much of its bus came from plugins,
which, for a shell that publishes nothing of its own, is all of it.

### `fn host_service_list(h: PluginHost) -> Str`

the keys a panel may discover, as a JSON array.

### `fn host_lib_count(h: PluginHost) -> Int`

how many `.so` plugins are loaded.

### `fn host_lib_at(h: PluginHost, i: Int) -> view LoadedPlugin`

the loaded plugin at `i`.

### `fn host_reload_changed(inout h: PluginHost) -> Int`

reload every loaded plugin whose file changed, re-registering its panels. Returns how many moved.

A panel the new build no longer declares stays in the registry pointing at its library; ask
`host_render` for it and it yields "" with the error recorded, like any other empty panel.

### `fn host_unload_all(inout h: PluginHost)`

close every loaded library. The host must not be rendered afterwards.

### `fn host_note_error(inout h: PluginHost, msg: Str)`

record a host-level failure, one that belongs to no single panel.

### `fn host_error_count(h: PluginHost) -> Int`

how many host-level failures have been recorded.

### `fn host_error_at(h: PluginHost, i: Int) -> view Str`

the host-level failure at `i`.


Loading a plugin `.so` and reading what it provides.

A plugin is a shared library exporting the `nori_plugin_*` entry points below. It is built
with `noric --build-lib IN OUT.so --shared --runtime runtime.nori`, which links the runtime
inside the library and exports nothing else, so the plugin has its own heap and handle tables,
and every value crossing the boundary is a scalar or a C string.

Nothing here traps. A library that will not open, is missing a symbol, reports the wrong ABI, or
returns an unreadable manifest yields a `LoadedPlugin` with `ok = false` and `err` set, and the
caller carries on with the plugins that did load.

##### One version

This host speaks plugin ABI 1, and a plugin reporting any other version is refused.

Beyond drawing panels and taking their events, ABI 1 is, in one line each:

  * `nori_plugin_init`: the plugin is handed the address of the host's service callback, so it
    can call the host.
  * `nori_plugin_service`: the plugin answers service keys, rather than only calling them. The
    keys it answers are the `services` array its manifest declares; declaring a key you cannot
    answer is a load error.
  * `nori_plugin_surface`: the plugin fills a named hole with a GPU texture, given the host's
    borrowed device and queue. The holes it fills are the manifest's `surfaces` array.

The last two are dispatched by name: a host walks the surfaces a document
declared and the services a plugin declared, and matches strings. It never learns what any of them
mean. That is what lets one `nori_ui` binary be a game editor, an image editor or a browser
depending only on what is in its plugin directory.

A plugin reporting another ABI is refused by version, with the reason in `err`.
### `fn ABI_VERSION() -> Int`

the plugin ABI this host speaks, the one version it loads.

### `fn SYM_ABI() -> Str`

`fn nori_plugin_abi() -> Int`: the ABI version the plugin was built against.

### `fn SYM_MANIFEST() -> Str`

`fn nori_plugin_manifest() -> cstr`: JSON describing the plugin and its panels.

### `fn SYM_INIT() -> Str`

`fn nori_plugin_init(host_call: Int) -> Int`: optional; called once after load, 0 = ready.

The argument is the address of the host's `(key: cstr, arg: cstr) -> cstr` callback, which is how
a plugin calls its host.

### `fn SYM_RENDER() -> Str`

`fn nori_plugin_render(id: cstr, w: Float, h: Float) -> cstr`: the panel's `.ui` source.

### `fn SYM_EVENT() -> Str`

`fn nori_plugin_event(id: cstr, action: cstr) -> Int`: a click or other action on a panel.

### `fn SYM_SERVICE() -> Str`

`fn nori_plugin_service(key: cstr, arg: cstr) -> cstr`: the plugin answering one of the
service keys its manifest declared. Same JSON-in / JSON-out contract as every other bus call, so
a caller cannot tell whether a key is answered by the host, a builtin, or a `.so`.

The reply belongs to the plugin and is copied on this side immediately; a block from another
image's malloc cannot be freed by ours. The plugin must therefore keep its reply alive until at
least its next call, the mirror of the rule `hostcall.nori` puts on the host.

### `fn SYM_SURFACE() -> Str`

`fn nori_plugin_surface(id: cstr, device: Int, queue: Int, w: Int, h: Int) -> Int`: the
plugin rendering into a named hole and returning a GPU texture view handle, or 0 when it has
nothing to show this frame.

`device` and `queue` are borrowed: one process, one `WGPUDevice`, owned by `ui_gpu`. The plugin
allocates its own targets on them and must release neither. Everything crossing here is a scalar,
so the C-string-only boundary is not widened: a handle is an integer like any other.

### `struct PanelDesc`

a panel a plugin provides: what the dock needs to place and label a tab.

### `struct LoadedPlugin`

one opened plugin library, its declared panels, and the entry points already resolved.
`stamp` is the file's size+mtime as of the load, so `changed` can tell when it has been rebuilt.

### `fn stamp_of(path: Str) -> Str`

a plugin file's fingerprint: size and mtime, cheap enough to poll every frame. "" if it is gone.

### `fn load(path: Str) -> LoadedPlugin`

open `path`, check its ABI, read its manifest, and resolve its entry points.

### `fn init(p: LoadedPlugin) -> Bool`

run the plugin's one-time setup, handing over the host callback when the plugin asked for one.

The plugin is given `hostcall_addr()` and can call the host bus from that moment on. A plugin
exporting no `nori_plugin_init` needs nothing from its host and is already ready.

### `fn calls_back(p: LoadedPlugin) -> Bool`

true when this plugin has an init to run, and so will be handed the host callback. A plugin with
no `nori_plugin_init` needs nothing from its host and is ready the moment it loads.

### `fn render(p: LoadedPlugin, id: Str, w: Float, h: Float) -> Str`

a panel's `.ui` source for this frame. "" when the plugin cannot draw it.

### `fn event(p: LoadedPlugin, id: Str, action: Str) -> Int`

deliver an action (a `HitRegion.on_click` id) to the panel that owns it.

### `fn serves(p: LoadedPlugin) -> Bool`

true when this plugin answers service keys itself.

### `fn service(p: LoadedPlugin, key: Str, arg: Str) -> Str`

ask the plugin to answer one of the keys its manifest declared. A plugin that cannot answer at
all replies in the shared error shape rather than "", so a caller has one thing to parse.

### `fn fills_surface(p: LoadedPlugin, id: Str) -> Bool`

true when this plugin declares `id` as a hole it fills.

### `fn surface(p: LoadedPlugin, id: Str, device: Int, queue: Int, w: Int, h: Int) -> Int`

render the named hole at `w` x `h` on the host's borrowed device and queue, and hand back a GPU
texture view handle. 0 means "nothing to show this frame" and is not an error: the host then
leaves the hole on its placeholder colour, which is visible on purpose.

### `fn unload(p: LoadedPlugin) -> Bool`

close the library. The `LoadedPlugin` must not be used afterwards.

### `fn changed(p: LoadedPlugin) -> Bool`

true when the plugin file on disk is not the one that was loaded: it has been rebuilt, replaced,
or removed. Cheap enough to ask every frame; the answer is a `stat`, not a read.

### `fn reload(p: LoadedPlugin) -> LoadedPlugin`

close and load the same path again, returning the fresh plugin. All state inside the old library
is gone: the plugin owns its own heap, and it goes with the library.

The new file must be a new file. A build that writes the `.so` in place, keeping its inode, can be
handed straight back by the dynamic loader; every writer worth using (including `noric`) creates a
replacement and renames it, which is what makes this work.

### `fn reload_changed(inout ps: Vec<LoadedPlugin>) -> Int`

reload every plugin whose file has changed, leaving the rest untouched. Returns the new list and
how many were reloaded, so a caller can log it.

### `fn plugin_ext() -> Str`

the extension a loadable plugin has on this platform: `so`, `dll`, `dylib`.

Re-exported so a shell can ask without importing `std/dl` itself.

### `fn scan(dir: Str) -> Vec<LoadedPlugin>`

load every plugin in `dir`, in directory order. Failures are returned too, with `ok = false`,
so a caller can show what refused to load instead of silently dropping it.


A panel's document from a `.ui` file and a `.style` file, the way a web page is HTML and CSS.

A plugin returns its whole document from `nori_plugin_render` as a string, so a panel written
that way builds the string in code: the shape of the panel, its colours and its data all
concatenated together in one function. That is fine for a status line and awkward for anything a
person might want to restyle. The template renderer, the standalone `.style` parser and the
stylesheet cascade exist for this, but a plugin needs a way to find a file of its own.

`plugin.dir` answers that, and this turns the answer into a document:

```nori,excerpt
ui::plugin::panel_ui_sync(g_doc, ui::plugin::plugin_dir(g_host, "build_panel"), "build")
return os::os_cstr(ui::plugin::panel_ui_doc(g_doc, env))
```

The files are read at run time and re-read when their size or mtime moves, so editing
`build.ui` restyles the panel in the next frame with no rebuild, much like working on a web
page. A program that must ship sealed wants `include_str` instead: same text, compiled in, no
files to lose and nothing to edit.

The data stays in code. A `.ui` file says what the panel looks like and `{{#foreach row in
rows}}` says it repeats; the plugin builds the JSON that fills it. That is the split: markup and
style in files a designer can edit, data and behaviour in Nori.
### `struct PanelUi`

a panel's two files, and what they were when last read.

### `fn plugin_dir(host: Int, name: Str) -> Str`

ask the shell where a plugin's library was loaded from. "" when it cannot say.

### `fn panel_ui_path(dir: Str, base: Str, ext: Str) -> Str`

where a panel's file is: in the source tree it was built from, or beside the library.

`plugin.dir` names the directory the `.so` was loaded from: `…/build/dev` in a checkout, and the
place a shipped plugin keeps its files. A plugin under development has them in `…/src`, two levels
up, and that is the copy someone is editing.

The source copy wins. `roll` copies `[assets]` next to the library at build time, so after the
first build both exist, and looking beside the library first would ignore every edit to
`src/panel.ui` until the next rebuild. A `src/` exactly two levels above a `build/<profile>/`
directory is this plugin's own source rather than a stray tree, and a shipped plugin has nothing
there at all.

### `fn panel_ui_sync(inout p: PanelUi, dir: Str, base: Str) -> Bool`

read `dir/base.ui` and `dir/base.style` if either has changed since the last look.

This costs two stats a frame (`stamp_of`), which is also what the plugin loader itself pays to
watch for a rebuilt `.so`. Returns true when something was re-read, so a caller can throw away
anything it derived from the old text.

A missing `.style` is not an error (a panel may have only structure), but a missing `.ui` is:
there is no document without it, and saying so is better than drawing nothing.

### `fn panel_ui_doc(p: PanelUi, env: json::Json) -> Str`

the panel's document: the stylesheet, then the template filled in from `env`.

The `.style` file is the document's `stylesheet { }` block. A `.style` source is a sequence of
`class X { … }` / `tag Y { … }` rules (the same rules that block holds), so wrapping rather than
parsing and re-emitting keeps one spelling of a stylesheet in the language, and a `.style` file
can be pasted into a document and a document's block saved out as a file.

An error is a document too. A panel whose template is broken draws the reason, with the line and
column the renderer reported, because the alternative is an empty rectangle and no idea why.

### `fn panel_ui_sheet_rules(src: Str) -> Str`

the rules inside a `stylesheet { … }` block, or the string unchanged when it is already rules.

A document may have only one stylesheet block (a second is "duplicate stylesheet block"), and a
real panel has two sources of style: the shell's theme, which arrives from `shell.theme` as a
whole block, and its own `.style` file. They have to become one block, so the outer braces come
off whichever already has them.

### `fn panel_ui_doc_themed(p: PanelUi, env: json::Json, theme: Str) -> Str`

the panel's document with another sheet's rules in front of its own: the shell's theme, whose
classes every panel draws with. Rules later in a block win, so the panel's `.style` overrides.

### `fn panel_ui_error_doc(msg: Str) -> Str`

what a panel draws when its own files are wrong.


