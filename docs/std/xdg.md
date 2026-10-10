# std/xdg

```nori
import "std/xdg" as xdg
```

std/xdg icons, following the Icon Theme Specification: `index.theme` parsing, the lookup a launcher wants
(the theme, its parents via Inherits, `hicolor`, then `/usr/share/pixmaps`; png/svg/xpm with png
and svg preferred), and a cache of directory listings so a lookup never re-reads a directory.
Same module as `std/xdg`: import "std/xdg" and call `xdg::icon_lookup(...)`.

  import "std/xdg" as xdg
  xdg::icon_lookup("my-app", 48)              // full path, or "" when no icon matches
  xdg::icon_resolve(entries_icon_value, 48)   // an absolute path in Icon= is used as is

A theme is a directory `<basedir>/<theme>/index.theme` where the base directories are
`$HOME/.icons`, then `<datadir>/icons` for each entry of `xdg::data_search_paths` (the spec:
"apps should look in $HOME/.icons (for backwards compatibility), in $XDG_DATA_DIRS/icons ...").
The theme's configuration, the `Directories=` list, each directory's Size/Type/…, comes from
the first base directory whose `<theme>/index.theme` exists ("The first index.theme found while
searching the base directories in order is used"), but the icon files are searched in the
theme's subdirectories under every base directory, in base-directory order ("A theme can be
spread across several base directories ... This way users can extend and override system
themes"). The spec's lookup: "The lookup order ... is: first the theme itself, then each theme
listed in the Inherits key, ... then hicolor" and finally the pixmap directories; within a
theme the `Directories=` order is searched (the spec's LookupIcon: "for each subdir in $(theme
subdir list) { for each directory in $(basename list) { ... } }") and a directory with
`Type=Fixed|Scalable|Threshold` qualifies when the requested size fits it (the spec's
Directories() pseudocode).
### `struct IconDirCfg`

a directory config of an icon theme (`[48x48]` etc.).

### `struct IconTheme`

a parsed icon theme: its directories (in `Directories=` order) and the themes it inherits.

### `fn ICON_FIXED() -> Int`

directory type: fixed size.

### `fn ICON_SCALABLE() -> Int`

directory type: scalable between MinSize and MaxSize.

### `fn ICON_THRESHOLD() -> Int`

directory type: usable within Threshold of Size.

### `fn load_theme(name: Str) -> IconTheme`

the theme `name` from the first base directory that has it (`$HOME/.icons`, then each
`<datadir>/icons` in priority order), or an empty theme (no dirs, no inherits) when it is not
installed.
(Icon Theme Spec: "In at least one of the theme directories there must be a file called
index.theme that describes the theme. The first index.theme found while searching the base
directories in order is used.")

### `fn icon_lookup(name: Str, size: Int) -> Str`

lookup icon `name` at pixel `size`, scale 1, in the default theme "hicolor".

### `fn icon_lookup_scale(name: Str, size: Int, scale: Int) -> Str`

lookup icon `name` at pixel `size` on screen at `scale` (a hidpi display), default theme hicolor.

### `fn icon_lookup_in(theme: Str, name: Str, size: Int, scale: Int) -> Str`

lookup icon `name` in theme `theme` (then its Inherits, then hicolor, then /usr/share/pixmaps)
at pixel `size` and `scale`; "" when nothing matches.

The theme's directory list comes from the first base directory holding its index.theme; the
icon file itself is then searched in each matching subdirectory under every base directory, in
base-directory order ($HOME/.icons, then each data dir's icons/). This is the spec's LookupIcon:
"for each subdir in $(theme subdir list) { for each directory in $(basename list) { ... } }".
The spec's chain: "The theme is searched ... If that fails, the parent themes are checked ...
If no parent themes are specified ... 'hicolor' is used as the fallback", plus the pixmap
fallback ("as a last resort, ... /usr/share/pixmaps").

### `fn icon_resolve(icon: Str, size: Int) -> Str`

resolve an `Icon=` value: an absolute path is used as is (Desktop Entry Spec: "The value ...
may be given as an absolute path ... the file is used as is"); anything else is looked up as a
themed icon name at `size`. "" when there is no icon or none matches.

### `fn app_icon_path(a: App, locale: Str, size: Int) -> Str`

the full icon path for the app's Icon key (absolute values as-is, themed otherwise).


### `struct MimeGlob`

one glob of globs2: `weight:type:pattern[:flags]`; kind 0 a literal name, 1 `*SUFFIX`, 2 any other glob

### `struct MagicLet`

one line of a magic rule (a "matchlet"): at `off` (to `off + range - 1`), `value` under `mask`; `wsize` is the
word size whose bytes are swapped on a little-endian machine (1: none)

### `struct MagicRule`

a `[priority:type]` section of the magic file

### `struct MimeDb`

a loaded database: the globs indexed (literal names and `*SUFFIX` patterns by their lower-case text), the magic
rules by priority, and the type relations

### `fn mime_db_empty() -> MimeDb`

an empty database (every guess is the text/binary fallback)

### `fn mime_db_load_with(extra: Vec<Str>) -> MimeDb`

The database of this machine: `mime/` under $XDG_DATA_HOME and each of $XDG_DATA_DIRS (the spec's order: the
first the most important), plus `extra` data directories after them (another root's share, say).

### `fn mime_db_load() -> MimeDb`

mime_db_load_with, nothing extra

### `fn mime_db_load_dirs(dirs: Vec<Str>) -> MimeDb`

The database in these `mime` directories, the most important first. Directories that do not exist are
skipped; `ok` says whether any held a database (globs2 or magic).

### `fn magic_rule_matches(r: MagicRule, data: Str) -> Bool`

does the magic rule match the data?

### `fn mime_by_data(db: MimeDb, data: Str) -> Str`

The type the data's first bytes say by the magic rules (the highest priority that matches; "" when none does).
Hand it the file's first few kilobytes: 4096 bytes cover every rule shared-mime-info ships but a handful.

### `fn mime_glob_match(pat: Str, name: Str) -> Bool`

The glob `pat` against `name` (fnmatch's `*`, `?` and `[...]`, `[!...]`; no path separators involved).

### `fn mime_by_name(db: MimeDb, name: Str) -> Vec<Str>`

The types the name's globs give, after the spec's pruning: only the biggest weight, then only the longest
pattern; distinct types, in globs2's order. Empty when no glob matches.

### `fn mime_looks_text(data: Str) -> Bool`

Text or not, by the spec's advice: no ASCII control character in the first 128 bytes but the ones text has
(tab, newline, form feed, carriage return, escape); high bytes are text (UTF-8).

### `fn mime_unalias(db: MimeDb, t: Str) -> Str`

The canonical name of a type (aliases followed).

### `fn mime_is_a(db: MimeDb, t: Str, p: Str) -> Bool`

Is `t` the type `p`, or a subclass of it? Aliases followed; besides subclasses, every text/* type is a
text/plain and every type but inode/* an application/octet-stream (the spec's implicit parents).

### `fn mime_parents(db: MimeDb, t: Str) -> Vec<Str>`

A type's parents: its subclasses lines, then the implicit text/plain (for text/*) and application/octet-stream.

### `fn mime_guess(db: MimeDb, name: Str, data: Str) -> Str`

The type of a file by its name and its first bytes (`data`; "" when they are not at hand), by the spec's
recommended order: one glob type wins outright; otherwise the magic decides; a glob type that is the magic's
type or a subclass of it, else the magic's type when no glob matched, else the first glob type; with neither,
text/plain for text and application/octet-stream for the rest.

### `fn mime_icon_names(db: MimeDb, t: Str) -> Vec<Str>`

The icon names a theme is asked for, in order: the database's icon for the type, the type's own name
(`image/png` -> `image-png`), its generic icon (generic-icons, else `<media>-x-generic`).

### `fn mime_comment(db: MimeDb, t: Str, locale: Str) -> Str`

What the type is called, for people: the `<comment>` of `<dir>/<type>.xml` in the most important directory that
has one, in `locale` (`hu_HU.UTF-8`, `hu`: the language and then the plain one) when it says one; "" when no
directory describes the type.

### `struct MimeApps`

the associations of every mimeapps.list, merged: per type, the default applications in preference order, the
added associations in preference order, and what each file removed (applied to its own and less important
files' associations: `blocked`)

### `fn mimeapps_files(desktops: Vec<Str>) -> Vec<Str>`

The mimeapps.list files in the spec's order, the most important first, for the desktops named (lower-case,
`XDG_CURRENT_DESKTOP`'s): `$XDG_CONFIG_HOME/{desktop-,}mimeapps.list`, the same in each config dir, then the
(deprecated) `applications/` of the data home and data dirs.

### `fn current_desktops() -> Vec<Str>`

the desktops XDG_CURRENT_DESKTOP names, lower-case

### `fn mimeapps_load() -> MimeApps`

The associations of this machine (XDG_CURRENT_DESKTOP's desktops' files included).

### `fn mimeapps_load_files(paths: Vec<Str>) -> MimeApps`

The associations of these files, the most important first (files that do not exist are skipped).

### `fn apps_for_mime(ma: MimeApps, db: MimeDb, apps: Vec<App>, t: Str) -> Vec<Str>`

The applications (the ids of `apps`, the installed ones: App.id, without `.desktop`) that open type `t`, best first: the default
applications mimeapps.list names, its added associations, the entries whose MimeType= lists the type; then the
same for each parent type, nearest first. Removed associations are left out (not from the defaults).

### `fn default_app_for_mime(ma: MimeApps, db: MimeDb, apps: Vec<App>, t: Str) -> Str`

The application that opens type `t` by default: the first of apps_for_mime ("" when nothing opens it).

### `fn mimeapps_set_default(path: Str, t: Str, id0: Str) -> Bool`

Make `id` (a desktop file id; `.desktop` is added when it lacks it) the default application of type `t` in the
mimeapps.list at `path` (the user's is
`config_home() + "/mimeapps.list"`): its `[Default Applications]` line for the type replaced, or added, the rest
of the file as it was. False when the file could not be written.


std/xdg: freedesktop XDG base directories, `.desktop` desktop entries, the installed-app list and
Exec field codes; the layer a desktop shell's launcher and dock sit on. Icon lookup (the Icon Theme
Specification) lives in `icon.nori`, in the same module.

  import "std/xdg" as xdg

**XDG base directories** (Base Directory Specification): `data_home()`, `data_dirs()`,
`config_home()`, `config_dirs()`, `cache_home()`, `runtime_dir()` honour `$XDG_*` with the spec's
defaults (`~/.local/share`, `/usr/local/share:/usr/share`, `~/.config`, `/etc/xdg`, `~/.cache`,
nothing for the runtime dir which has no default).

**Desktop entries** (Desktop Entry Specification 1.5): `parse_desktop(text)` reads groups, keys,
localized keys (`Name[hu]`), escapes (`\s \n \t \r \\`), `;`-separated lists and booleans.
`entry_str(e, key, locale)` fetches a value through the spec's locale fallback
(lang_COUNTRY@MODIFIER → lang_COUNTRY → lang@MODIFIER → lang → plain); `entry_list`/`entry_bool`
apply the list and boolean rules; the `[Desktop Action X]` groups are reachable via
`entry_str_in(e, "Desktop Action …", …)` / `entry_actions(e)`.

**Applications** (Desktop Entry Specification's "Desktop File ID" + "Hidden"): `list_apps()` walks
every `applications/` directory under the data dirs: earlier directories win, subdirectories become
`-` in the id, and a `Hidden=true` entry removes an id. `shown_apps` applies NoDisplay and the
OnlyShowIn/NotShowIn and TryExec rules for a real launcher.

**MIME types and default applications** (Shared MIME-info Database, MIME Applications Associations):
`mime.nori`: `mime_db_load`, `mime_guess(db, name, head)`, `mime_is_a`, `mime_icon_names`, `mime_comment`,
`mimeapps_load`, `apps_for_mime`, `default_app_for_mime`, `mimeapps_set_default`.

**Exec field codes** (`%f %F %u %U %i %c %k %%`): `expand_exec(exec, name, icon, file_path, files,
urls)` splits the Exec value with the spec's quoting (double-quoted arguments, `""` `` `` `` `$$`
escapes) and expands the field codes into an argv; `app_argv(a, locale, files, urls)` does it for an
App.
### `fn data_home() -> Str`

$XDG_DATA_HOME, or `$HOME/.local/share` when unset/empty/relative.
(Base Directory Spec §2: "If $XDG_DATA_HOME is either not set or empty, a default equal to
$HOME/.local/share should be used.")

### `fn data_dirs() -> Vec<Str>`

$XDG_DATA_DIRS split on ':' (relative entries ignored), or [`/usr/local/share`, `/usr/share`].
(Base Directory Spec §2: "If $XDG_DATA_DIRS is either not set or empty, a value equal to
/usr/local/share/:/usr/share/ should be used.")

### `fn config_home() -> Str`

$XDG_CONFIG_HOME, or `$HOME/.config` when unset/empty/relative.
(Base Directory Spec §3: "If $XDG_CONFIG_HOME is either not set or empty, a default equal to
$HOME/.config should be used.")

### `fn config_dirs() -> Vec<Str>`

$XDG_CONFIG_DIRS split on ':' (relative entries ignored), or [`/etc/xdg`].
(Base Directory Spec §3: "If $XDG_CONFIG_DIRS is either not set or empty, a value equal to
/etc/xdg should be used.")

### `fn cache_home() -> Str`

$XDG_CACHE_HOME, or `$HOME/.cache` when unset/empty/relative.
(Base Directory Spec §4: "If $XDG_CACHE_HOME is either not set or empty, a default equal to
$HOME/.cache should be used.")

### `fn runtime_dir() -> Str`

$XDG_RUNTIME_DIR, or "" when unset.
(Base Directory Spec §6: "If $XDG_RUNTIME_DIR is not set, the program should fall back to a
place ... with a warning"; returning "" is our fallback.)

### `fn data_search_paths() -> Vec<Str>`

the data search path in priority order: data_home() first, then each data_dirs() entry.
(Base Directory Spec §2: "There is a single base directory $XDG_DATA_HOME ... and a set of
preference-ordered base directories relative to which data files should be searched.")

### `struct DescKey`

one key of a desktop entry: `key` is the name without a locale suffix; `locale` is the `[hu_HU]`
suffix ("" for a plain key); `value` has the `\s \n \t \r \\` escapes already applied.

### `struct DescGroup`

one `[Group]` of a desktop entry, with its keys in file order.

### `struct DesktopEntry`

a parsed desktop entry file: its groups in file order.

### `fn parse_desktop(text: Str) -> DesktopEntry`

parse the text of a `.desktop` file (UTF-8). Keys before the first `[Group]` header are dropped
(the spec: "The first section of every desktop file is the 'Desktop Entry' section"; a file with
no group describes nothing), as are comment lines ("Lines starting with a '#' are comments,
and are ignored") and malformed `key=value` lines without an '='.

### `fn entry_has_key_in(e: DesktopEntry, group: Str, key: Str) -> Bool`

does `group` contain `key` (with any locale)?

### `fn entry_has_key(e: DesktopEntry, key: Str) -> Bool`

does the `[Desktop Entry]` group contain `key` (with any locale)?

### `fn entry_str_in(e: DesktopEntry, group: Str, key: Str, locale: Str) -> Str`

the value of `key` in `group`, through the spec's locale fallback for `locale`
(lang_COUNTRY@MODIFIER → lang_COUNTRY → lang@MODIFIER → lang → plain key), or "" if none exists.
The locale "C" (or "POSIX") reads the plain key only.

### `fn entry_str(e: DesktopEntry, key: Str, locale: Str) -> Str`

the value of `key` in the `[Desktop Entry]` group, through the locale fallback (see entry_str_in).

### `fn entry_list_in(e: DesktopEntry, group: Str, key: Str) -> Vec<Str>`

`key` of `group` as a list ("" ignored; `;`-separated).

### `fn entry_list(e: DesktopEntry, key: Str) -> Vec<Str>`

`[Desktop Entry]` `key` as a list ("" ignored; `;`-separated).

### `fn entry_list_locale_in(e: DesktopEntry, group: Str, key: Str, locale: Str) -> Vec<Str>`

a localized list value: the fallback chain of entry_str_in, then split as a list.

### `fn entry_list_locale(e: DesktopEntry, key: Str, locale: Str) -> Vec<Str>`

a localized list value from the `[Desktop Entry]` group.

### `fn entry_bool_in(e: DesktopEntry, group: Str, key: Str) -> Bool`

`key` of `group` as a boolean. (Desktop Entry Spec: "Boolean values are ... 'true' or 'false'".
Accepted case-insensitively; anything else is false.)

### `fn entry_bool(e: DesktopEntry, key: Str) -> Bool`

`[Desktop Entry]` `key` as a boolean (see entry_bool_in).

### `fn entry_actions(e: DesktopEntry) -> Vec<Str>`

every group name starting with "Desktop Action ": the `[Desktop Action X]` groups the spec
defines for secondary actions ("the name of the action group ... must be prefixed by
'Desktop Action'").

### `struct App`

an installed application: its desktop file id, the file it was read from, and the full parse, so
localized values and the Exec line stay available after listing.

### `fn list_apps() -> Vec<App>`

every `.desktop` file under the `applications/` directory of each data dir, in data-dir priority
order (earlier directories win, with a `Hidden=true` file removing an id even from an earlier
directory). Desktop File IDs follow the spec: "The desktop file ID is the file name with the
.desktop extension removed, relative to the applications directory, with '/' replaced by '-'".

### `fn app_id(a: App) -> Str`

the desktop file id.

### `fn app_path(a: App) -> Str`

the path the app was read from.

### `fn app_entry(a: App) -> DesktopEntry`

the parsed entry: for localized keys and the raw keys a launcher wants verbatim.

### `fn app_name(a: App, locale: Str) -> Str`

the `Name` (localized through `locale`).

### `fn app_generic_name(a: App, locale: Str) -> Str`

the `GenericName` (localized).

### `fn app_comment(a: App, locale: Str) -> Str`

the `Comment` (localized).

### `fn app_icon(a: App, locale: Str) -> Str`

the `Icon` (a themed name or an absolute path; resolve it with icon_resolve / app_icon_path).

### `fn app_type(a: App) -> Str`

the `Type` ("Application", "Link", ...). Only "Application" entries are launchable apps.

### `fn app_exec(a: App) -> Str`

the `Exec` line.

### `fn app_try_exec(a: App) -> Str`

the `TryExec` line ("the executable ... to check if the program is actually installed"; if it
does not resolve, the entry "must be ignored").

### `fn app_work_path(a: App) -> Str`

the `Path` key (the working directory to launch in).

### `fn app_terminal(a: App) -> Bool`

the `Terminal` key ("Whether the program runs in a terminal window").

### `fn app_no_display(a: App) -> Bool`

the `NoDisplay` key ("'true' if the application should not be shown in the menus").

### `fn app_hidden(a: App) -> Bool`

the `Hidden` key (a hidden entry removes its id entirely: list_apps already drops it).

### `fn app_startup_wm_class(a: App) -> Str`

the `StartupWMClass` key (the WM class for startup tracking).

### `fn app_only_show_in(a: App) -> Vec<Str>`

the `OnlyShowIn` list ("the application should be shown only if there is a match with the
current desktop environment").

### `fn app_not_show_in(a: App) -> Vec<Str>`

the `NotShowIn` list ("... should not be shown if there is a match with the current desktop
environment").

### `fn app_categories(a: App) -> Vec<Str>`

the `Categories` list.

### `fn app_keywords(a: App, locale: Str) -> Vec<Str>`

the `Keywords` list (localized).

### `fn app_mime_type(a: App) -> Vec<Str>`

the `MimeType` list.

### `fn app_actions(a: App) -> Vec<Str>`

the `[Desktop Action X]` group names.

### `fn shown_apps(apps: Vec<App>, current_desktops: Vec<Str>) -> Vec<App>`

the apps a launcher should actually show: list_apps() minus:
 · NoDisplay=true ("If NoDisplay is true ... this entry should be skipped when searching for an
   application to present to users"),
 · entries whose OnlyShowIn/NotShowIn rules exclude `current_desktops` (both keys "are the list
   of desktop environments"),
 · entries whose TryExec does not resolve ("If this key is present, the program ... must exist,
   else the entry must be ignored").
`current_desktops` is the (colon-split) $XDG_CURRENT_DESKTOP; empty hides nothing on its own.

### `fn try_exec_ok(try_exec: Str) -> Bool`

does the TryExec value resolve? A value containing '/' is a path; otherwise it is searched on
$PATH. (Desktop Entry Spec, "TryExec": "may be used to test whether this program is actually
installed"; X_OK on the resolved path is our test.)

### `fn expand_exec(exec: Str, name: Str, icon: Str, file_path: Str, files: Vec<Str>, urls: Vec<Str>) -> Vec<Str>`

the argv of an Exec value. `name` feeds %c, `icon` %i, `file_path` %k, `files` %f/%F and `urls`
%u/%U. A word whose file/URL/icon field code has nothing to substitute is dropped.

### `fn app_argv(a: App, locale: Str, files: Vec<Str>, urls: Vec<Str>) -> Vec<Str>`

the argv for launching `a`: its Exec expanded with %c = the localized Name, %i = the Icon key,
%k = the desktop file path, and `files`/`urls` for %f %F %u %U.


