# std/nori_ui::render

```nori
import "std/nori_ui" as ui          // then ui::render::…
```

std/nori_ui/render: the front end that drives whichever renderer is live.

  import "std/nori_ui" as ui
  var r = ui::render::renderer_mode(ui::render::cpu(), win, myFont)
  while ... {
      ui::render::begin(r, bg)
      ui::render::dock(r, ui::layout::layout_dock_ui(...))
      ui::render::text(r, pos, "hello", 16.0, white)
      ui::render::end(r)
  }
  ui::render::release(r)

Choose the backend yourself: `renderer_mode(cpu(), ...)` always works, and `try_renderer(gpu(),
...)` returns Result so a program can ask for the GPU and decide for itself what to do when the
machine has no device. Nothing here falls back silently.
Either way every draw call below is the same, and HiDPI is handled: lay out in logical
coordinates (the window's logical size) and it renders at physical resolution.
### `fn gpu() -> Int`

The rendering mode. Choose one, once, when the program starts; every draw call afterwards is the
same code whichever it is.

### `fn try_renderer(mode: Int, inout win: window::Window, inout f: font::Font) -> Result<Renderer>`

Ask for a renderer on `mode`: `Ok` with it, or `Err` saying why that backend is not available.

The failed renderer is released before returning, so a caller cannot leak the one it was not
given. Nothing here substitutes the other backend. Which one is running is not an implementation
detail a library should decide quietly: a CPU renderer is a different program to write for than
a GPU one, with a different frame budget, a different sensible resolution and different things
worth drawing. A program that wants the usual "GPU, else software" asks for it in a few lines:

    var r = ui::render::renderer_mode(ui::render::cpu(), win, f)
    match ui::render::try_renderer(ui::render::gpu(), win, f) {
        Result::Ok(g)    => { ui::render::release(r)  r = g }
        Result::Err(why) => { printl(why + " — using software") }
    }

A GPU backend that failed to initialise accepts every call and draws nothing, so check the
`Result` (or `ok`) rather than assuming a device.

### `fn renderer_mode(mode: Int, inout win: window::Window, inout f: font::Font) -> Renderer`

A renderer on `mode`, unchecked; installs font metrics. Lay your UI out in logical coords
(win's logical size); the HiDPI scale is applied automatically. `cpu()` always works; for
`gpu()` this hands back a renderer that may have failed to initialise, so ask `ok()`, or use
`try_renderer`, which asks for you.

### `fn set_alpha(inout r: Renderer, on: Bool)`

A translucent window: what is drawn keeps its alpha, and the window is shown with it, so what is
behind shows through where nothing (or something translucent) is drawn: a dock with rounded
corners over the wallpaper, a panel at 88% opacity. Clear to a transparent colour
(`color_rgba(0, 0, 0, 0)`) for the parts that are not there. CPU: the canvas carries premultiplied
alpha (`paint_cpu::canvas_alpha`) and the window presents ARGB (`window::set_alpha`); a GPU surface
is not changed by this (its swapchain's alpha mode is the device's).

### `fn set_shadows(inout r: Renderer, on: Bool)`

whether `retained_frame` draws nori_ui's soft drop shadow under every opaque rounded rectangle
(on by default, as `overlay` always has). A desktop whose look draws its own depth turns it off.

### `fn ok(r: Renderer) -> Bool`

did the chosen backend initialize? (GPU can fail without a Vulkan device.)

### `fn backend(r: Renderer) -> Int`

which backend is active (render::gpu() or render::cpu()).

### `fn width(r: Renderer) -> Int`

the logical width / height.

### `fn resize(inout r: Renderer, w: Int, h: Int)`

update the logical size (call on a Resized event); the framebuffer/surface resizes on next begin.

### `fn begin(inout r: Renderer, bg: Color)`

start a frame cleared to `bg` (ensures the framebuffer matches the window's current physical size).

### `fn end(r: Renderer)`

finish the frame: draw + present.

### `fn dock(inout r: Renderer, res: DockLayoutResult)`

draw a dock's chrome (from layout_dock_ui); lay out in logical, scaling is automatic.

### `fn windows(inout r: Renderer, wr: WindowLayoutResult)`

draw floating-window chrome (from layout_windows).

### `fn overlay(inout r: Renderer, ov: ComputedOverlay)`

draw a widget overlay (from layout_overlay), with drop shadows.

### `fn retained_frame(inout r: Renderer, ov: ComputedOverlay, damage: Vec<Rect>, full: Bool, bg: Color)`

Draw a retained document's frame: begin, draw and present in one call, repainting only what
changed. `ov` is `layout::rdoc_overlay(doc)`, `damage`/`full` are the frame's (`rdoc_frame`).
Call it only when the frame `changed`: when nothing did, there is nothing to present and the
program can sleep until its next input.

CPU: the canvas holds the previous frame, so only the damaged rects are cleared to `bg` and
redrawn (`paint_cpu::render_overlay_damage`, pixel-identical to a full repaint); a `full` frame,
a resize or a HiDPI scale change repaints everything. The whole buffer is still handed to the
window: `window::present` damages the whole surface, so the compositor recomposes all of it.

GPU: the whole frame is drawn. The swapchain image a frame renders into does not hold the
previous frame (FIFO hands back whichever image is free), so a scissored repaint would first
need a persistent copy of the last frame and a full-surface copy out of it every frame, the
same fill a redraw costs, since a UI's primitives are a few hundred instanced quads with little
overdraw. What a retained document saves on the GPU is everything before the draw (no layout,
no primitives recorded) and every frame where nothing changed (none is drawn at all).

### `fn fill_rect(r: Renderer, rect: Rect, col: Color)`

fill a rect (logical coords) for host content.

### `fn fill_round(r: Renderer, rect: Rect, col: Color, radius: Float)`

fill a rect with rounded corners (logical coords), radius in logical pixels.

`radius <= 0` is the same call as fill_rect, so this can be the only one a caller uses.

### `fn text(inout r: Renderer, pos: Vec2, str: Str, size: Float, col: Color)`

draw text at `pos` (logical; pos.y = baseline) at `size` px logical, for host content.

### `fn draw_image(r: Renderer, rect: Rect, im: image::Image)`

draw a decoded image (std/image) into `rect` (logical coords), scaled to fit and alpha-blended, for
host content. Decode once with image::load_png and reuse the Image across frames (don't re-decode).

### `fn gpu_device_of(r: Renderer) -> Int`

the borrowed `WGPUDevice` behind this renderer, as an Int handle: 0 on the CPU backend or when
the GPU one did not come up. Hand it, with `gpu_queue_of`, to a second renderer that must draw
into textures this one will sample. Do not release it, and do not use it after `release`.

### `fn gpu_queue_of(r: Renderer) -> Int`

the borrowed `WGPUQueue`, on the same terms.

### `fn surface_blits(inout r: Renderer, ov: ComputedOverlay, b: SurfaceBindings)`

draw an overlay's surface holes from the host's binding table: texture-bound holes are blitted,
colour-bound and unbound ones are filled. Call it after `overlay` for the same overlay: the
panel's own background is a primitive and belongs under the scene, and a texture-bound hole
emitted no primitive of its own, so nothing is drawn twice.

### `fn release(r: Renderer)`

release resources. Not named `fill`/`free`: both are built-in names (and the C `free` an imported
module declares `extern` is never mangled), so a top-level function with one of those names
could not be called.


