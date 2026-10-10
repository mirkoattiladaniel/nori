# std/nori_ui::paint_gpu

```nori
import "std/nori_ui" as ui          // then ui::paint_gpu::…
```

std/nori_ui/paint_gpu: a wgpu (GPU) renderer for std/nori_ui overlays.

  import "std/nori_ui" as ui
  var gc = ui::paint_gpu::gpu_new_window(disp, win, w, h, myFont)   // Xlib/Win32 wgpu surface
  ui::paint_gpu::render_overlay(gc, overlay, ui::core::color_rgb(22, 26, 38))

Consumes the same ComputedOverlay / PaintPrimitive data as std/nori_ui/paint_cpu, but draws on the GPU:
rounded-rect fills + borders via an SDF fragment shader, and text as textured quads sampling an
R8 glyph atlas (std/font). Three targets: `gpu_new_window` presents to an Xlib/Win32 wgpu surface,
`gpu_new_window_wl` to a Wayland one, and `gpu_new_offscreen` renders to a texture read back with
`gpu_readback` (headless/testable).
The heavy lifting lives in ui_gpu_glue.c (wgpu-native v29); Nori just walks primitives.

A program built with `noric --build --native` links `__native_ui_gpu.nori` in the glue's place: the
same pipelines in Nori, calling wgpu-native through `CFn` pointers into `libwgpu_native.so`, which it
opens when a canvas is made (found on `LD_LIBRARY_PATH`; no `--cfile`/`--clink`). Without the
library `gpu_ok` is false, as on a machine with no GPU. Offscreen canvases render and read back
exactly as in a default build; a windowed canvas reports no GPU, because std/window's native backends
hold no Xlib or libwayland objects for wgpu to make a surface from.
### `struct GpuCanvas`

a GPU render context + the font/atlas used for text. `ctx` is the opaque wgpu backend handle.

### `fn gpu_new_offscreen(w: Int, h: Int, inout f: font::Font) -> GpuCanvas`

an offscreen GPU canvas: renders to a texture; read it back with `gpu_readback`. For headless
tests and image snapshots (no window needed).

### `fn gpu_new_window(disp: Int, win: Int, w: Int, h: Int, inout f: font::Font) -> GpuCanvas`

a windowed GPU canvas presenting to an X11 surface (disp = Display*, win = Window XID from
std/window native_display/native_handle). Frames are shown by `gpu_end` / `render_overlay`.

### `fn gpu_new_window_wl(disp: Int, surf: Int, w: Int, h: Int, inout f: font::Font) -> GpuCanvas`

a windowed GPU canvas presenting to a native Wayland surface (disp = wl_display*, surf = wl_surface*
from std/window_wayland). Native Wayland resizes cleanly and scales correctly on HiDPI outputs.

### `fn gpu_ok(c: GpuCanvas) -> Bool`

did the GPU backend initialize (adapter + device + pipelines)? false means no GPU/driver.

### `fn gpu_free(c: GpuCanvas)`

release all GPU resources.

### `fn gpu_resize(c: GpuCanvas, w: Int, h: Int)`

resize the render target / surface to (w,h). Track w,h yourself for readback sizing.

### `fn use_font_metrics(inout f: font::Font)`

Install a font's per-character advances into nori_ui for text measurement, so layout matches the
glyphs drawn.

Call once after loading the font and before laying out (doc_layout / layout_overlay). Measurement
is layout's job, not the renderer's, but it needs a real font to answer, so each renderer that
has one installs it.

### `fn gpu_fill_grad(c: GpuCanvas, r: Rect, col: Color, radius: Float, gto: Color, gax: Float, gaxd: Float, ghoriz: Bool)`

fill `r` with a two-colour linear ramp from `col` to `gto` over [gax, gax + gaxd), along x when
`ghoriz`, else y. The GPU half of paint_cpu::fill_rect_grad_cl, with the same rule:
the ramp is evaluated per fragment from the screen position, so the two painters agree.

### `fn gpu_text_baseline_offset(c: GpuCanvas, size: Float) -> Float`

px from a text box's top edge down to its first baseline at `size`: the font's real vertical
metrics (hhea, scaled to px by std/font's font_v_metrics_px), centred in the line box the layout
reserved, by layout::text_baseline_in_box. It is not a `size * k` guess, and not the raw ascent
either: see that function for why a taller face would otherwise move every glyph in the window.
The GPU half of paint_cpu::cpu_text_baseline_offset, with the identical rule so the two
backends put a kind-2 primitive's glyphs on the same rows. Fontless canvas: a 0.82 em
ascent, put through the same centring, so a canvas with a font and one without still agree.

### `fn gpu_text_fx(inout c: GpuCanvas, pos: Vec2, text: Str, size: Float, col: Color, ocol: Color, ow: Float, gon: Bool, gto: Color, gax: Float, gaxd: Float, ghoriz: Bool)`

draw text with the PaintPrimitive text effects. The GPU half of paint_cpu::draw_text_fx_cl, with
the same construction: eight offset copies under the glyphs for the outline (not an
atlas dilate, which the CPU painter cannot reproduce), and the gradient as per-instance data the
fragment shader evaluates. Both stay inside the single glyph draw call.

### `fn gpu_text_fx_sp(inout c: GpuCanvas, pos: Vec2, text: Str, size: Float, col: Color, ocol: Color, ow: Float, gon: Bool, gto: Color, gax: Float, gaxd: Float, ghoriz: Bool, lsp: Float, wsp: Float)`

gpu_text_fx carrying the primitive's letter_spacing / word_spacing (px, already scaled). The
GPU half of paint_cpu::draw_text_fx_sp_cl.

### `fn gpu_render_overlay(inout c: GpuCanvas, ov: ComputedOverlay, bg: Color)`

draw a ComputedOverlay's paint primitives on the GPU over a `bg`-cleared frame. On a windowed
canvas this presents the frame; on an offscreen canvas, follow with `gpu_readback`.

### `fn gpu_render_overlay_opts(inout c: GpuCanvas, ov: ComputedOverlay, shadows: Bool, bg: Color)`

like render_overlay, with `shadows` = a soft-ish drop shadow behind rounded, opaque filled rects.

### `fn gpu_render_overlay_scaled(inout c: GpuCanvas, ov: ComputedOverlay, shadows: Bool, bg: Color, s: Float)`

render an overlay laid out in logical coordinates at `s`x physical resolution (all geometry + font
sizes multiplied by `s`; glyphs are rasterized at physical size for crispness). For HiDPI/fractional
scaling: configure the surface at logical*scale, lay out in logical, and let the compositor viewport
map the physical buffer back to logical size. `s = 1.0` is the ordinary path.

### `fn gpu_set_clip(c: GpuCanvas, r: Rect)`

clip every following primitive to `r` (physical/canvas coords) until `gpu_clear_clip`. The box is
snapped outward to whole pixels, floor(min) and ceil(max), which is ui_cpu's `clip_of_rect`,
so the two painters keep or drop the same pixels. It is frame state: `gpu_begin` resets it.

### `fn gpu_clear_clip(c: GpuCanvas)`

stop clipping: following primitives are drawn unclipped.

### `fn gpu_begin(c: GpuCanvas, bg: Color)`

begin a GPU frame cleared to `bg`. Follow with gpu_render_dock/gpu_render_windows/gpu_render_overlay_into/
gpu_fill/gpu_text, then `gpu_end` to draw + present.

### `fn gpu_end(c: GpuCanvas)`

finish the frame: upload the glyph atlas, draw, and present.

### `fn gpu_render_overlay_into(inout c: GpuCanvas, ov: ComputedOverlay, shadows: Bool, s: Float)`

draw an overlay's primitives into an already-begun frame (no begin/present), geometry scaled by `s`.

### `fn gpu_render_dock(inout c: GpuCanvas, res: DockLayoutResult, s: Float)`

draw a dock's chrome (from layout_dock_ui) into an already-begun frame, scaled by `s`.

### `fn gpu_render_windows(inout c: GpuCanvas, wr: WindowLayoutResult, s: Float)`

draw floating-window chrome (from layout_windows) into an already-begun frame, scaled by `s`.

### `fn gpu_fill(c: GpuCanvas, r: Rect, col: Color)`

fill a rect (in physical/canvas coords) for host content, between begin/end.

### `fn gpu_fill_round(c: GpuCanvas, r: Rect, col: Color, radius: Float)`

fill with rounded corners: the same instance, with a radius the shader turns into an SDF edge.

### `fn gpu_text(inout c: GpuCanvas, pos: Vec2, str: Str, size: Float, col: Color)`

draw text (physical coords, pos.y = baseline) for host content, between begin/end.

### `fn gpu_blit_rgba(c: GpuCanvas, r: Rect, src: Int, iw: Int, ih: Int)`

blit a straight RGBA8 image (`src` = iw*ih*4 bytes, byte0=R..byte3=A) into `r` (physical coords),
scaled to fit, alpha-blended. Mirrors paint_cpu::blit_rgba. See std/image + nori_ui_render.

### `fn gpu_device(c: GpuCanvas) -> Int`

the WGPUDevice this canvas owns, as an Int handle. Borrowed: do not release it, and do not use
it after `gpu_free`. 0 when the backend did not come up. Pass it, with `gpu_queue`, to a second
renderer that must draw into textures this canvas can sample.

### `fn gpu_queue(c: GpuCanvas) -> Int`

the WGPUQueue, on the same terms.

### `fn gpu_target_format(c: GpuCanvas) -> Int`

the colour format this canvas renders into, as a WGPUTextureFormat enum value.

### `fn gpu_adapter(c: GpuCanvas) -> Int`

the WGPUAdapter, borrowed, for capability queries without standing up a second instance.

### `fn gpu_instance(c: GpuCanvas) -> Int`

the WGPUInstance, borrowed.

### `fn gpu_blit_view(c: GpuCanvas, r: Rect, tview: Int, alpha: Float, linear: Bool, flip_v: Bool)`

draw an already-resident texture view into `r` (physical coords), alpha-modulated by `alpha`.
`tview` is a WGPUTextureView handle that must be TextureBinding-usable, sampled-float, 2D, and
alive until the frame is presented; it is never released here. `linear` filters (a scene blit),
`flip_v` samples with v inverted. The texture's own alpha is ignored: the blit is opaque.

### `fn gpu_blit_surfaces(c: GpuCanvas, blits: Vec<SurfaceBlit>, s: Float, flip_v: Bool)`

draw every `SurfaceBlit` a document's holes produced, each into its own already-clipped rect,
with `texture` read as a WGPUTextureView handle. `s` scales the rects the way every other draw
call in this file scales them (HiDPI); pass 1.0 for the ordinary path.

Call this after `gpu_render_overlay_into` for the same overlay: the panel's own background is a
prim and must be under the scene, and the hole itself emitted no prim to cover.

### `fn gpu_null(inout f: font::Font) -> GpuCanvas`

a null GpuCanvas (ctx = 0), a placeholder when the GPU backend is not the active one.

### `fn gpu_present(c: GpuCanvas)`

present the current frame (windowed canvas). render_overlay already presents; use this only for
custom draw sequences via ui_gpu_begin/rect/glyph.

### `fn gpu_readback(c: GpuCanvas) -> Int`

read the offscreen frame into a freshly malloc'd `w*h` 0x00RRGGBB buffer (caller frees). Returns
0 on failure (e.g. a windowed canvas). Matches std/nori_ui/paint_cpu's canvas pixel layout.

### `fn gpu_readback_pipelined(c: GpuCanvas, buf: Int) -> Bool`

read the offscreen frame without waiting for the GPU, for a frame path rather than a test.

Hands back the previous frame's pixels and starts this frame's copy, so the CPU never blocks on
a copy it just submitted. `gpu_readback` does block, which is right for a screenshot and costs
milliseconds a frame for anything continuous.

Two differences from `gpu_readback`:
  * it writes into a buffer the caller owns and keeps, because allocating and freeing `w*h*4`
    every frame is the other half of the cost this exists to remove. Size it `w*h*4` bytes.
  * the pixels are raw RGBA8 rows, not `0x00RRGGBB` words: the repack is a per-pixel loop, and
    an encoder or a texture upload wants the bytes as they are.

False when there is nothing to hand back yet, which is the first call: skip that frame.

### `fn readback_pixel(buf: Int, w: Int, h: Int, x: Int, y: Int) -> Int`

the 0x00RRGGBB pixel at (x,y) in a readback buffer of width `w` (0 if out of bounds).


