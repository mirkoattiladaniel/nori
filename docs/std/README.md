# The Nori Standard Library

A reference for each `std/*` module: what it is, how to import it, and its public
API. A module needs no native dependency unless its overview notes one; import a module
with `import "std/NAME" as NAME` and call its items through the alias (`NAME::item`).

Each page is generated from its module's own doc comments, so it tracks the source exactly.

A library big enough to have **sub-namespaces** gets a page per part rather than one
page: `nori_ui` is imported whole and reached through its parts, so the pages follow the
same division the caller already sees.

## Text and parsing

[strutil](strutil.md) · [unicode](unicode.md) · [fmt](fmt.md) · [parse](parse.md) · [regex](regex.md) · [csv](csv.md) · [json](json.md) · [toml](toml.md) · [encoding](encoding.md) · [c](c.md) (C headers)

## Collections and data structures

[hashmap](hashmap.md) · [table](table.md) · [ordmap](ordmap.md) · [set](set.md) · [deque](deque.md) · [heap](heap.md) · [pvec](pvec.md) · [sort](sort.md) · [iter](iter.md) · [bigint](bigint.md)

## Math and numerics

[math](math.md) · [stats](stats.md) · [rand](rand.md)

## System, I/O, and networking

[os](os.md) · [io](io.md) · [path](path.md) · [time](time.md) · [sys](sys.md) · [net](net.md) · [http](http.md) · [dl](dl.md) · [notify](notify.md) · [background](background.md)

## Cryptography and identifiers

[crypto](crypto.md) · [tls](tls.md) · [uuid](uuid.md)

## Compression and archives

[archive](archive.md) · [erasure](erasure.md) (Reed–Solomon erasure coding)

## Graphics and media

[image](image.md) · [svg](svg.md) · [geometry](geometry.md) · [font](font.md) · [audio](audio.md) · [video](video.md)

## Windowing and terminal

[window](window.md) · [tui](tui.md) · [xdg](xdg.md) (desktop entries, icons, base directories)

## User interface

[nori_ui](nori_ui.md) is one import with seven sub-namespaces — `import "std/nori_ui" as ui`,
then `ui::layout::…`. **Start at [its overview](nori_ui.md)**: a window comes from
[window](window.md), and nori_ui draws into it — a complete program is there.

[core](nori_ui/core.md) — geometry, colour, the theme and the text styles ·
[layout](nori_ui/layout.md) — the `.ui` DSL, the document, the dock, the widgets ·
[render](nori_ui/render.md) — one renderer interface over both painters ·
[paint_cpu](nori_ui/paint_cpu.md) — the software painter ·
[paint_gpu](nori_ui/paint_gpu.md) — the wgpu painter ·
[plugin](nori_ui/plugin.md) — loading panels and the service bus ·
[shell](nori_ui/shell.md) — discovery, the dock's state and the layout on disk

And three pages of their own: [the `.ui` DSL](nori_ui/ui_dsl.md) — the elements, their attributes,
templates, events and binds — [the `.style` language](nori_ui/style_dsl.md) its stylesheets are written in,
and [the plugin ABI](nori_ui/plugin_abi.md), the contract between an application and the shared
libraries it loads.

## Core and prelude

[result](result.md) — the `Option` / `Result` / `Json` prelude

## Tooling and library support

[check](check.md) · [test](test.md) · [log](log.md) · [flag](flag.md)
