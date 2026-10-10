# std/nori_ui

```nori
import "std/nori_ui" as ui
```

std/nori_ui: the one import for a Nori user interface.

## A window, and something in it

**nori_ui does not open windows.** `std/window` does; nori_ui draws into the window you give it.
Every program that shows something is these three steps, and nothing else is required:

1. `window::open` a native window (Wayland or X11 on Linux, Win32 on Windows; one call for all).
2. `ui::render::renderer_mode(…, win, font)` a renderer on it: `cpu()` for software, `gpu()` for wgpu.
3. Each frame: drain `window::poll`, then `ui::render::begin`, draw, `ui::render::end`.

```nori
import "std/nori_ui" as ui
import "std/window" as window
import "std/font" as font
import "std/os" as os

fn main() -> Int {
    var win = window::open("Hello", 640, 400)
    if !window::opened(win) { printl("could not open a window")  return 1 }
    var fr = font::font_load(read_file(os::env_or("NORI_HOME", ".") + "/std/font/assets/DejaVuSans.ttf"))
    if !fr.ok { printl("no font: " + fr.err)  return 1 }
    var f = inout fr.font                  // the renderer borrows it: `inout`, not a copy
    var r = ui::render::renderer_mode(ui::render::cpu(), win, f)
    var running = true
    while running {
        var pumping = true
        while pumping {                    // every pending event, then draw once
            match window::poll(win) {
                window::Event::NoEvent => { pumping = false }
                window::Event::Closed => { running = false  pumping = false }
                window::Event::Resized(w, h) => { if w > 0 && h > 0 { ui::render::resize(r, w, h) } }
                _ => { }
            }
        }
        if window::should_close(win) { running = false }
        ui::render::begin(r, ui::core::color_rgb(28, 32, 44))
        let card = ui::core::rect_min_size(ui::core::vec2(40.0, 40.0), ui::core::vec2(240.0, 90.0))
        ui::render::fill_round(r, card, ui::core::color_rgb(70, 110, 200), 10.0)
        ui::render::text(r, ui::core::vec2(60.0, 92.0), "Hello, nori_ui", 22.0, ui::core::color_rgb(240, 240, 250))
        ui::render::end(r)
    }
    ui::render::release(r)
    window::close(win)
    return 0
}
```

The window and the GPU glue are C, so the project's `nori.manifest` links them. The paths are
relative to `$NORI_HOME`, and a `[native.windows]` section replaces the base keys on Windows:

```toml
[native]
cfile = std/window/window_dispatch.c, std/window/window_wayland.c, std/window/window_x11.c, std/nori_ui/paint_gpu/ui_gpu_glue.c
clink = wayland-client, xkbcommon, X11, wgpu_native
ccflag = -DNORI_HAVE_X11
ldir = std/nori_ui/paint_gpu/lib

[native.windows]
cfile = std/window/window_win32.c, std/nori_ui/paint_gpu/ui_gpu_glue.c
clink = wgpu_native, ntdll, userenv
ldir = std/nori_ui/paint_gpu/lib-windows
```

Then `roll run`. `(x, y)` is from the top-left in logical pixels; a text position is its baseline,
not its top edge. Input beyond the event loop (the pointer, clicks, scrolling) is also
`std/window`'s (`window::mouse_pos`, `window::next_click`): see `ref::std::window`.

## A panel, a button, and clicks: a `.ui` document

Rectangles and text by hand stop scaling at the first row of buttons. A panel is a `.ui` document
(a string describing nested `panel`/`column`/`row`, `text`, `button` and friends), and the same
four steps turn it into pixels and back into actions:

1. `ui::layout::dsl_parse_document(src)`: strict, a mistake is `pr.err` as `"LINE:COL: message"`.
2. `ui::layout::doc_layout(pr.doc, rect)`: lays it out into a rectangle, giving a `ComputedOverlay`.
3. `ui::layout::overlay_hit_test(ov, point)`: the hit region under a press; its `on_click` is the
   string the document gave that button.
4. `ui::render::overlay(r, ov)`: draws it, between `begin` and `end`.

The document is rebuilt every frame from the state it shows, so there is nothing to keep in step.
Replacing the drawing in the loop of the window program above:

```nori
fn panel_src(clicks: Int) -> Str {
    var s = "panel { padding: 16 gap: 10 bg: \"#252a38\" radius: 10 "
    s = s + "text { text: \"Settings\" size_px: 20 } "
    s = s + ("text { text: \"Saved " + (to_str(clicks) + " times\" size_px: 14 } "))
    s = s + "button { label: \"Save\" on_click: \"save\" } }"
    return s
}
```

```nori,excerpt
// before the loop: `var clicks = 0`. In the loop, after draining window::poll:
        var pr = ui::layout::dsl_parse_document(panel_src(clicks))
        if !pr.ok { printl("ui: " + pr.err)  return 1 }
        let area = ui::core::rect_min_size(ui::core::vec2(40.0, 40.0), ui::core::vec2(260.0, 180.0))
        let ov = ui::layout::doc_layout(pr.doc, area)
        var more = true
        while more {                       // every press since the last frame
            let c = window::next_click(win)
            if c.kind == window::CLICK_NONE() { more = false }
            else if c.kind == window::CLICK_DOWN() {
                let hit = ui::layout::overlay_hit_test(ov, ui::core::vec2(to_float(c.x), to_float(c.y)))
                if hit.on_click == "save" { clicks = clicks + 1 }
            }
        }
        ui::render::begin(r, ui::core::color_rgb(28, 32, 44))
        ui::render::overlay(r, ov)
        ui::render::end(r)
```

Every element and its attributes are in [the `.ui` DSL](nori_ui/ui_dsl.md), and the stylesheets in
[the `.style` language](nori_ui/style_dsl.md).
`button` takes `label:` and not `text:`, and the parser says so rather than guessing.

## The seven parts

Sub-namespaces, each with its own page:

- `ui::render::`: the front end above, one set of draw calls whichever painter is live. **Start here.**
- `ui::core::`: geometry (`vec2`, `rect_min_size`), colours and theming.
- `ui::layout::`: the layout engine (the `.ui` DSL, documents, docking and widgets). For a UI
  bigger than a few rectangles, lay it out here and draw the result with `ui::render::overlay`.
- `ui::paint_cpu::` and `ui::paint_gpu::`: the two painters. Address one directly only when it
  must be that one; `ui::paint_cpu` also draws into an image with no window at all.
- `ui::plugin::`: loading panels from shared libraries, and the service bus between them; what a
  plugin itself must export is [the plugin ABI](nori_ui/plugin_abi.md).
- `ui::shell::`: what an editor built from plugins needs (discovery, the dock's saved layout).
  It is headless and opens nothing; a program with one window does not need it.

Most programs need `ui::render::` plus `ui::core::`, and `ui::layout::` once there is a layout.

Coordinates are Float (f64); colors are 0..=255 per channel stored as Int.

## Choosing a renderer

Nothing here chooses for you. `renderer_mode(cpu()/gpu(), …)` asks
for one; `try_renderer(mode, …)` asks and answers `Result`: `Ok` with it, or `Err` saying why
that backend is not available, having released what it could not give you.

Neither falls back to the other. Which backend is running is not an implementation detail: a
CPU renderer is a different program to write for than a GPU one, and a library that swaps them
quietly has decided something it cannot know. A program that wants "GPU, else software" says so
in four lines:

    var r = ui::render::renderer_mode(ui::render::cpu(), win, f)
    match ui::render::try_renderer(ui::render::gpu(), win, f) {
        Result::Ok(g)    => { ui::render::release(r)  r = g }
        Result::Err(why) => { printl(why + " — using software") }
    }

A GPU backend that failed to initialise accepts every call it is given and draws nothing, so
check the `Result` rather than assuming a device.

A program that does not want the GPU at all links `paint_gpu/ui_gpu_absent.c` instead of
`ui_gpu_glue.c` and needs no wgpu-native; `gpu_ok` then answers false, so `try_renderer(gpu())`
reports it and `renderer_mode(cpu(), …)` is the one to call.

## Why the entry points are in `ui::render::`

Inside a directory module a folder can
name a sibling's types through `use sib::*`, but only for a sibling that sorts before it, and a
qualified call (`layout::f()`) does not parse, while a root file's own scope shadows `use sub::*`
entirely. So dependency order has to equal alphabetical order (core, layout, paint_cpu,
paint_gpu, render), and `render`, sorting last, is the only place able to drive all four. That is
also why the renderers are `paint_*`: `cpu`/`gpu` would sort before `layout` and could not see it.


