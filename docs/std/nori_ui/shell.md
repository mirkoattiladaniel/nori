# std/nori_ui::shell

```nori
import "std/nori_ui" as ui          // then ui::shell::…
```

The shell core: what an editor needs that is not a panel and not a renderer.

Discovery of plugins, seeding a dock from what they declared, remembering the layout, and
remembering which plugins the user turned off. All of it is headless (no window, no GPU), so an
application supplies only the frame loop.

**Looking for how to open a window? Not here.** A window is `std/window`, and drawing in it is
`ui::render`; `ref::std::nori_ui` has the whole program. A program with one window never needs
the shell.

**This file names no domain.** It has no idea whether the panels it places belong to a game
engine or an IDE; that is decided by which plugins are on disk. If anything here ever
needs to know, the separation has leaked.
### `struct ShellConfig`

what the shell was told to do: where to look for plugins, and which ones to leave alone.

### `fn config_new(dir: Str) -> ShellConfig`

a config with one plugin directory and nothing disabled.

### `fn config_is_disabled(c: ShellConfig, name: Str) -> Bool`

true when `name` has been turned off.

### `fn config_disable(inout c: ShellConfig, name: Str)`

turn a plugin off. Already-off is not an error.

### `fn config_enable(inout c: ShellConfig, name: Str)`

turn a plugin back on.

### `fn config_write(c: ShellConfig) -> Str`

render a config back to text.

### `fn str_lines(s: Str) -> Vec<Str>`

split on newlines, dropping a trailing empty.

### `fn config_parse(src: Str) -> ShellConfig`

read a config back. Unknown keys are ignored, so a newer file stays readable by an older shell.

### `fn config_load(path: Str) -> ShellConfig`

read the config at `path`. A missing file is an empty config, not a failure.

### `fn config_save(c: ShellConfig, path: Str)`

write the config to `path`.

### `fn shell_discover(inout h: PluginHost, c: ShellConfig) -> Int`

load every enabled plugin from every configured directory into `h`. Returns how many loaded.

A disabled plugin is opened far enough to read its name and then closed again, since the shell has to
know a plugin exists to offer turning it back on.

### `fn shell_available(c: ShellConfig) -> Vec<Str>`

every plugin name present in the configured directories, whether enabled or not; what a
preferences panel lists. Each is opened for its manifest and closed again.

### `fn seed_dock(h: PluginHost) -> DockState`

build a dock from what the loaded panels asked for: left down the sidebar, bottom along the
bottom, everything else in the middle. A panel that named no position lands in the middle.

### `fn dock_serialize(d: DockState) -> Str`

serialize a dock layout to text.

### `fn dock_deserialize(src: Str) -> DockState`

read a dock layout back. Anything unreadable yields an empty dock, which the caller replaces with
a fresh seed; a corrupt layout file must never stop the shell from starting.

### `fn restore_or_seed(h: PluginHost, c: ShellConfig) -> DockState`

the layout to start with: the remembered one when it still matches the panels that are actually
registered, otherwise a fresh seed. A layout naming panels that are gone is not restored, since the
user would get a dock full of dead tabs.

### `fn shell_fill_surfaces(inout h: PluginHost, overlay: ComputedOverlay, inout b: SurfaceBindings, device: Int, queue: Int) -> Int`

**the whole generic surface path.** Fill every hole in `overlay` that some plugin claims, on the
host's borrowed `device` and `queue`, and record the resulting texture views in `b`.

This is the function that lets the shell stop knowing what it is a shell of. It walks the holes a
document declared, matches each id against what the loaded plugins said they fill, hands over the
device and the hole's pixel size, and binds whatever handle comes back. There is no branch here
for 3D, for an image canvas, or for a web view: a hole is a name and a rect, and the plugin that
claimed the name decides what appears in it.

A hole nothing claims, or a plugin that returns 0, keeps `surface_placeholder_color()`, which is
a visible colour on purpose: an unfilled surface should look wrong rather than look like a
background.

### `struct MenuItem`

one command a plugin asked the shell to show: which top-level menu, what to call it, and the
service key to invoke when it is chosen.

`key` is a shortcut spelling ("ctrl+s", "ctrl+shift+b", "f5") or "" for a row with no binding.
It travels with the row rather than living in a table in the shell, because the shell does not
know what any of these do and should not have to know what any of them are worth binding to. A
panel that grows a command grows its shortcut in the same line, and the menu shows the key beside
the label because they came from the same place and therefore cannot disagree.

### `fn SHELL_MENU_KEY() -> Str`

the reserved service key a plugin answers to contribute menu commands.

### `fn shell_menu_items(h: PluginHost) -> Vec<MenuItem>`

ask every loaded plugin what it wants in the menus, in load order.

A plugin that does not serve, does not declare the key, or answers anything unparseable
contributes nothing: a malformed reply is not worth failing a frame over, and the shell has no
way to judge one anyway.

### `struct SettingDecl`

one setting a plugin declares: what it is called, what kind of value it holds, and what it is
worth when nobody has said otherwise.

`key` is namespaced by whoever owns it (`editor.font`, `viewer.theme`) because the settings
of every plugin land in one file and one panel; a bare `shell` from two plugins is two plugins
fighting over a line in a config nobody can read.

`kind` is one of "text", "bool", "int", "choice". A "choice" carries its options in `options`,
separated by `|` or by `,`: a Vec would be the better type, but a Str survives the JSON
reply without a second parse. Both separators are accepted because nothing about the field says
which one it wants, and choosing the other fails silently as one option wearing the whole list.
`plugin` and `plugin_label` are filled in by the shell, not by the plugin: the declaration was
answered by a `.so` that the host already knows the name of, and asking a panel author to repeat
it in every row would invite mistakes. `plugin_label` is the name of the plugin's
first panel, which is what the person calls it ("Editor", not `editor_panel`).
`service` is for `kind: "secret"` and empty for everything else: the name of a service on the
bus that owns the value, so the shell can route it without ever holding it.

A secret is not a setting with a mask on it. Everything else here is stored by the shell in
`<config>.prefs`, a plain file people paste into bug reports; an API token stored the same way
with a nicer widget in front of it is the same token in the same plain file. So a secret declares
where it lives instead: the shell asks that service for its state ("none", "saved", "env") to
draw a row, and hands it a new value to keep, and at no point has the value itself.
`picker` names a picker scope that can answer this setting: `font` for a face, and whatever
else grows one. A path is exact and is the wrong way to ask somebody what they want; a row that
offers the list as well is the difference between a setting and a puzzle.

### `fn SHELL_SETTINGS_KEY() -> Str`

the reserved key a shell asks every plugin for the settings it owns.

### `fn shell_settings_decls(h: PluginHost) -> Vec<SettingDecl>`

ask every loaded plugin what it can be configured with, in load order.

The same contract as `shell_menu_items`, and for the same reason: a plugin knows what it can be
told and the shell knows how to ask a person. Neither has to know the other's business. A panel
that declares nothing is simply not configurable, and one that declares a setting gets it stored,
edited and handed back without writing a line of UI.

### `struct PickerScope`

one picker scope a plugin provides: the word you type before `::`, what it is for, and who
answers when you enter it.

The picker has no content of its own. It is a box with a query in it and a list under that; which
lists exist, what is in them and what picking one does are the things a plugin knows and a shell
does not. This is the declaration that says "I can answer `file::`", the same shape as
`shell.menu` and `shell.settings`, for the same reason: the panel that owns a kind of thing is
the panel that should be asked about it.

`plugin` and `plugin_label` are filled in by the shell from the `.so` that answered, never by the
plugin. `service` is the key the shell calls to get the rows, defaulting to `picker.rows`; it is
called with the scope as its argument and answers `{"result":[{label,detail,note,action}, …]}`.
`key` is the chord that opens the picker already narrowed to this scope, spelled the way
`shell_key_name` spells one ("ctrl+e"); empty for a scope you reach by typing its name. `row` is
whether the scope is offered in the unscoped list: false for the pseudo-scope that names the
unscoped list itself, which exists so that its chord has somewhere to be declared.

### `fn SHELL_PICKERS_KEY() -> Str`

the reserved key a shell asks every plugin for the picker scopes it can answer.

### `fn PICKER_ROWS_KEY() -> Str`

the default service a scope is answered by, when a declaration does not name one.

### `fn PICKER_PREVIEW_KEY() -> Str`

the service asked for the preview of a selected row; answered by the same plugin.

### `fn PICKER_RESOLVE_KEY() -> Str`

the service asked whether a typed word names a scope; see `shell_picker_resolve`.

### `fn shell_picker_scopes(h: PluginHost) -> Vec<PickerScope>`

every scope every loaded plugin says it can answer, in load order.

### `fn shell_picker_owner(scopes: Vec<PickerScope>, scope: Str) -> PickerScope`

the declaration for `scope`, matched on its head: `preference::editor` is answered by whoever
declared `preference`. An empty `scope` field means nothing claimed it.

### `fn shell_picker_head(scope: Str) -> Str`

the part of a scope before the first `::`. `preference::editor::changed` is the `preference`
plugin's business all the way down; only the head decides who is asked.

### `fn shell_picker_chord(scopes: Vec<PickerScope>, name: Str) -> Str`

what chord `name` opens, as the string `shell.pick` takes, or "" when nothing is bound to it.

The chords are contributed, like every other key in this window. A scope that arrives from a
plugin brings the key that opens it, and a shell that has no picker of its own has no business
owning the chord for one.

### `fn shell_picker_rows(h: PluginHost, scopes: Vec<PickerScope>, scope: Str) -> Vec<PickerItem>`

the rows of `scope`, from whoever declared it.

### `struct PickerPreview`

the preview of a selected row: a strip of lines, or an image, or nothing.

`kind` is "lines", "image" or "" for nothing to show. `of` is what is being previewed, which the
picker uses to know whether the preview it holds is still the right one.

### `fn shell_picker_preview(h: PluginHost, scopes: Vec<PickerScope>, action: Str) -> PickerPreview`

ask the providers what to show beside `action`, stopping at the first that offers anything.

This goes by action, not by scope. In the unscoped list a file row, a command row and a setting
row sit beside one another, and the thing that can describe a row is whoever produced it, so the
question is asked of each provider with the row's own action, and the one that recognises it
answers. A provider that does not is expected to say nothing, which costs a service call and is
the price of not having the shell know what a `.nori` file is.

### `fn shell_picker_resolve(h: PluginHost, scopes: Vec<PickerScope>, have: Str, word: Str) -> Str`

what a plugin makes of a word typed before `::` while `have` is the scope already applied.

`preference::` narrows to `preference::editor` when you type `editor::`, and only the plugin that
owns `preference` knows that `editor` is one of the things it holds. Asked of every provider until
one claims it; "" means nobody did, and the word stays part of the query.

### `fn shell_menu_binding(items: Vec<MenuItem>, key: Str) -> MenuItem`

the item bound to `key`, or one with an empty action when nothing is.

The shell asks this on every keystroke it did not otherwise consume. Linear over a list that is
a few dozen rows at most, once per key press. The alternative is an index that has to be
rebuilt whenever a plugin is reloaded, for a saving nobody can measure.

### `fn shell_key_name(sym: Int, mods: Int) -> Str`

`mods`+`keysym` as the spelling a contribution uses: "ctrl+shift+s", "f5", "ctrl+p".

Lower case and in a fixed order, so "ctrl+shift+b" is the only way to write that chord and a
plugin cannot miss its own binding by spelling it "shift+ctrl+B".

### `fn SHELL_STARTUP_KEY() -> Str`

the reserved key a shell asks every plugin once, after the dock exists.

### `fn shell_startup_actions(h: PluginHost) -> Vec<Str>`

what each plugin wants done the moment the dock is up, in load order.

A plugin cannot arrange this for itself. `nori_plugin_init` runs before there is a dock, and
`nori_plugin_render` runs only for panels that are already visible, so a panel that wants to be
the one you land on can neither ask early enough nor, if it is behind another tab, ask at all.

The reply is an action string, the same vocabulary the menu bar speaks: a service key, or the
shell's own `panel:<id>`. An empty reply means "nothing, thank you", which is what every plugin
that has not thought about this returns by not answering the key at all.

### `fn shell_menu_items_for(items: Vec<MenuItem>, menu: Str) -> Vec<MenuItem>`

the contributed items for one top-level menu.

### `fn shell_menu_names(items: Vec<MenuItem>) -> Vec<Str>`

every top-level menu name any plugin contributed to, deduplicated and in contribution order.
A shell uses this to grow a menu it did not itself declare.


