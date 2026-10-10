# std/nori_ui::paint_cpu

```nori
import "std/nori_ui" as ui          // then ui::paint_cpu::…
```

std/nori_ui/paint_cpu: a CPU (software) renderer for std/nori_ui overlays.

  import "std/nori_ui" as ui
  import "std/font" as font
  var cv = ui::paint_cpu::canvas_new(w, h, myFont)         // 32bpp framebuffer + a text font
  ui::paint_cpu::clear(cv, color_rgb(22, 26, 38))
  ui::paint_cpu::render_overlay(cv, overlay)               // walk ComputedOverlay -> pixels
  // cv.buf is a 0x00RRGGBB framebuffer: XPutImage it, or dump it with `to_ppm`.

It turns nori_ui's PaintPrimitive data (filled rects, borders, text) into pixels in a CPU
framebuffer, using std/font for glyphs. The wgpu renderer consumes the same ComputedOverlay
through the same primitive set; only the per-primitive draw differs. Deterministic and
headless-testable (dump to an image, diff), with no GPU.
### `fn canvas_new(w: Int, h: Int, inout f: font::Font) -> CpuCanvas`

a new canvas with a text font (glyphs drawn via std/font).

### `fn canvas_new_scaled(wCss: Int, hCss: Int, inout f: font::Font, s: Float) -> CpuCanvas`

a canvas whose buffer is `s` times larger in each axis than the CSS-pixel area it stands for.

Draw in CSS pixels exactly as at scale 1: the primitives multiply positions, sizes and radii by
`s`, so glyphs are rasterised at the device size rather than drawn small and stretched by the
display afterwards, which is the difference between a page that looks native and one that looks
like a screenshot blown up. `canvas_w`/`canvas_h` report the buffer size, which is what `present`,
`blit_slice` and `to_ppm` need.

### `fn canvas_scale(c: CpuCanvas) -> Float`

device pixels per CSS pixel: 1.0 unless the canvas came from `canvas_new_scaled`.

### `fn canvas_with_fallback(c: CpuCanvas, inout fb: font::Font) -> CpuCanvas`

a view of the same framebuffer with a fallback face for codepoints the primary face lacks (.notdef).
The fallback joins layout's text engine too, so a line measured is a line drawn.

### `fn canvas_with_font(c: CpuCanvas, inout f: font::Font) -> CpuCanvas`

a view of the same framebuffer with a different text face (shares `buf`), which lets a renderer draw
mixed bold/italic/mono runs onto one canvas. Pair with `use_font_metrics(f)` before drawing.

### `fn canvas_alpha(c: CpuCanvas, on: Bool) -> CpuCanvas`

the same framebuffer with its alpha channel on (premultiplied 0xAARRGGBB: a translucent surface's
pixels) or off (0x00RRGGBB, the top byte unused; every canvas's default). Clear it after turning
it on: the pixels already there are not converted.

### `fn canvas_has_alpha(c: CpuCanvas) -> Bool`

is the canvas's alpha channel on?

### `fn canvas_new_nofont(w: Int, h: Int) -> CpuCanvas`

a new canvas with no font (text primitives are skipped), for pure rect/border rendering.

### `fn canvas_free(c: CpuCanvas)`

free the framebuffer.

### `fn canvas_buf(c: CpuCanvas) -> Int`

the pixel buffer, as `w * h` little-endian `0x00RRGGBB` words.

This is the layout `image::save_png_readback` documents for a GPU readback, so a software render
can be written out the same way.

### `fn canvas_copy_region(c: CpuCanvas, x0: Int, y0: Int, w: Int, h: Int, inout f: font::Font) -> CpuCanvas`

a new w×h canvas holding a copy of `c`'s pixels starting at (x0,y0), a backdrop snapshot for
group-opacity/transform compositing. Out-of-bounds source pixels copy as 0.

### `fn blit_transformed(c: CpuCanvas, off: CpuCanvas, ox0: Int, oy0: Int, cx: Float, cy: Float, sx: Float, sy: Float, cosR: Float, sinR: Float, alpha: Int, sentinel: Int)`

affine-blit `off` (element rendered over a sentinel background) onto `c`, transforming around canvas-
space center (cx,cy) by scale (sx,sy) then rotation (cosR,sinR). `ox0,oy0` = off's canvas origin. Dest
pixels inverse-map to `off`; sentinel (drawn-nothing) samples are skipped. `alpha` (0..255) scales blend.

### `fn blit_transformed_clip(c: CpuCanvas, off: CpuCanvas, ox0: Int, oy0: Int, cx: Float, cy: Float, sx: Float, sy: Float, cosR: Float, sinR: Float, alpha: Int, sentinel: Int, kx0: Float, ky0: Float, kx1: Float, ky1: Float)`

…restricted to a clip box (device px). A transformed element is composited on its own, outside
the op stream an `overflow: hidden` ancestor's clip lives in, so the clip has to be handed to the
blit or the element paints straight through the box that should have cut it off.

### `fn blend_region(c: CpuCanvas, off: CpuCanvas, x0: Int, y0: Int, alpha: Int)`

blend `off` onto `c` at (x0,y0): c = c·(1-a) + off·a (a = alpha/255). Since `off` started as a copy of
c's backdrop, untouched pixels blend to themselves, giving correct group opacity without an alpha channel.

### `fn blit_region(dst: CpuCanvas, src: CpuCanvas, sx: Int, sy: Int, w: Int, h: Int, dx: Int, dy: Int)`

Opaque copy of the `w`x`h` sub-rect of `src` at (sx,sy) onto `dst` at (dx,dy). Rows and columns
falling outside either canvas are skipped.

The sub-rect matters: a tile painted to be copied back has anti-aliased edges against its own
boundary, which are not the pixels the same content has in the middle of a full page. Painting a
margin and copying only the inside leaves those edge pixels behind in the margin, where they
belong. Copying the whole tile would leave a hairline of slightly wrong colour down its edge.

### `fn blit_slice(dst: CpuCanvas, src: CpuCanvas, srcY: Int, dstX: Int, dstY: Int, h: Int)`

opaque copy of an `h`-row slice of `src` starting at source row `srcY`, onto `dst` at (dstX,dstY).
Rows/cols outside either canvas are skipped. Used for scroll compositing: paint the whole page once
to an offscreen canvas, then each scroll frame is a memcpy of the visible slice instead of a repaint.

### `fn canvas_ptr(c: CpuCanvas) -> Int`

the raw framebuffer pointer (0x00RRGGBB pixels), to hand to an XPutImage present shim.

### `fn corner_cov_at(px: Float, py: Float, x0: Float, y0: Float, x1: Float, y1: Float, radius: Float) -> Int`

canvas width / height in pixels.
The two halves of the rounded fill, exposed so a test can draw the same rectangle the slow way
and compare. fill_rect_cl skips the interior, which is only valid if the pixels are the
same ones, so that is worth being able to check.

### `fn use_font_metrics(inout f: font::Font)`

install real per-character metrics from `f` into nori_ui, so layout (button sizing, wrapping,
ellipsis) matches the advances std/font actually draws, instead of nori_ui's flat heuristic.
Call once after loading the font and before laying out (doc_layout / layout_overlay).

### `fn clear(c: CpuCanvas, col: Color)`

fill the whole canvas with an opaque color.

### `fn drop_shadow(c: CpuCanvas, r: Rect, dx: Float, dy: Float, blur: Float, col: Color, alpha: Int)`

draw a soft drop shadow for `r` (offset by dx,dy, softened over `blur` px) in `col` at peak
`alpha` (0..255). A distance falloff outside the rect, cheap but reads as a real shadow.

### `fn drop_shadow_r(c: CpuCanvas, r: Rect, dx: Float, dy: Float, blur: Float, col: Color, alpha: Int, radius: Float)`

same, for a box with rounded corners. The falloff has to follow the rounded shape: measured from
the square box, a `border-radius:50%` circle glows in four corner lobes, a visible X around it.

### `fn fill_gradient(c: CpuCanvas, r: Rect, top: Color, bottom: Color)`

fill `r` with a vertical gradient from `top` to `bottom` (per-row lerp), clipped to the canvas.

### `fn fill_linear_gradient_r(c: CpuCanvas, r: Rect, dirx: Float, diry: Float, poss: Vec<Float>, cols: Vec<Color>, radius: Float)`

linear gradient clipped to rounded corners of `radius` px (border-radius on gradient boxes).

### `fn fill_radial_gradient_at(c: CpuCanvas, r: Rect, poss: Vec<Float>, cols: Vec<Color>, fx: Float, fy: Float, radPx: Float)`

radial gradient centred at (fx, fy) fractions of `r`; radPx > 0 = explicit radius in px
(CSS `transparent 42rem`), else the corner distance.

### `fn canvas_clone_nofont(src: CpuCanvas) -> CpuCanvas`

a canvas of `src`'s size holding a copy of its pixels, with no font attached.

### `fn rgba_merge(k: CpuCanvas, w: CpuCanvas)`

fold `k` (the layer painted over black) and `w` (the same layer over white) into `k` as a
premultiplied layer, alpha in the top byte.

### `fn rgba_clip_rrect(c: CpuCanvas, x0: Float, y0: Float, x1: Float, y1: Float, radius: Float)`

cut a premultiplied layer down to the rounded rect (x0,y0)-(x1,y1), device pixels: an element's own
`overflow` clip, applied to what its descendants painted before the element's filter runs.

### `fn mask_layer(mc: CpuCanvas, first: Bool, mode: Int, kind: Int, bx: Float, by: Float, bw: Float, bh: Float, dirx: Float, diry: Float, fromT: Float, gpx: Float, gpy: Float, rad: Float, poss: Vec<Float>, alphas: Vec<Int>)`

composite one layer of a CSS mask into `mc`, which holds the mask so far in the low byte of each
pixel. The layer is a gradient over the box (bx,by,bw,bh) in `mc`'s pixels: kind 0 linear along
(dirx,diry), 1 radial centred at (gpx,gpy) of the box with radius `rad` (0 = to the farthest
corner), 2 conic starting `fromT` turns clockwise from the top, and transparent outside the box
(`mask-clip: border-box`). An empty stop list is a layer with no image. Layers go bottom-up: the
`first` is written as is, and each one above it composites by `mode` (`mask-composite`): 0 add,
1 intersect, 2 subtract, 3 exclude.

### `fn rgba_apply_mask(off: CpuCanvas, mc: CpuCanvas)`

multiply a premultiplied layer by the mask `mc` built with `mask_layer` (same size).

### `fn rgba_blur(c: CpuCanvas, radius: Int)`

gaussian blur of a premultiplied layer, standard deviation about `radius` device pixels: three
box passes each way, which is what a gaussian converges to. Transparency blurs with the colour,
so a hard edge spreads into a soft one rather than stopping at the shape.

### `fn rgba_blit(c: CpuCanvas, off: CpuCanvas, ox0: Int, oy0: Int, hasFilt: Bool, gray: Float, bright: Float, inv: Float, sep: Float, contrast: Float, sat: Float, alpha: Int)`

composite a premultiplied layer onto `c` at (ox0,oy0), `alpha` (0..255) scaling it, through the
colour filter when `hasFilt`.

### `fn rgba_blit_transformed(c: CpuCanvas, off: CpuCanvas, ox0: Int, oy0: Int, cx: Float, cy: Float, sx: Float, sy: Float, cosR: Float, sinR: Float, alpha: Int, kx0: Float, ky0: Float, kx1: Float, ky1: Float)`

composite a premultiplied layer placed at (ox0,oy0) onto `c`, scaled by (sx,sy) then rotated
(cosR,sinR) about (cx,cy), sampled bilinearly (a layer stretched several times over must not turn
to blocks) and clipped to (kx0,ky0)-(kx1,ky1).

### `fn cpu_text_baseline_offset(c: CpuCanvas, size: Float) -> Float`

px from a text box's top edge down to its first baseline at `size`: the font's real vertical
metrics (hhea, scaled to px by std/font's font_v_metrics_px), centred in the line box the layout
reserved, by layout::text_baseline_in_box. It is not a `size * k` guess, and not the raw ascent either.
This is the single place paint_cpu turns layout::pp_text's top-left `pos` into a baseline;
paint_gpu::gpu_text_baseline_offset is the same rule on the other backend, and the dock's
TextLabel.top goes through it too, so there is one rule and not two.
A fontless canvas cannot know an ascent and falls back to 0.82 em, which goes
through the same centring so the two cases agree.

### `fn draw_text_fx_cl(c: CpuCanvas, pos: Vec2, text: Str, size: Float, col: Color, cl: Clip, ocol: Color, ow: Float, gon: Bool, gto: Color, gax: Float, gaxd: Float, ghoriz: Bool)`

draw text with the PaintPrimitive text effects: an outline of `ocol` at `ow` px (0 = none) and
an optional linear gradient from `col` to `gto` along [gax, gax+gaxd) (x if `ghoriz`, else y).

The outline is eight offset copies of the run, drawn under the glyphs, rather than an
alpha-dilate of the glyph mask. The reason is parity: a dilate reads
neighbouring glyphs out of a shared atlas, which the CPU rasterizer (it has no atlas, only
one glyph bitmap at a time) cannot reproduce, so the two painters would disagree exactly where
glyphs are packed close. Offset copies are the same drawing on both backends and need no shader.

### `fn draw_text_fx_sp_cl(c: CpuCanvas, pos: Vec2, text: Str, size: Float, col: Color, cl: Clip, ocol: Color, ow: Float, gon: Bool, gto: Color, gax: Float, gaxd: Float, ghoriz: Bool, lsp: Float, wsp: Float)`

draw_text_fx_cl carrying the primitive's letter_spacing / word_spacing (px, already scaled).

### `fn fill_rect(c: CpuCanvas, r: Rect, col: Color, radius: Float)`

fill `r` with `col` (rounded corners of `radius` px) over the whole canvas.

### `fn stroke_round_clip(c: CpuCanvas, r: Rect, col: Color, width: Float, radius: Float, x0: Float, y0: Float, x1: Float, y1: Float)`

...restricted to a clip box (CSS px). A square fill can be clipped by shrinking the rectangle, so
callers do that; a rounded one cannot (trimming its box moves the corners), so `overflow: hidden`
over a rounded child needs the clip carried down to the per-pixel loop, which is here.

### `fn fill_rect_clip(c: CpuCanvas, r: Rect, col: Color, radius: Float, x0: Float, y0: Float, x1: Float, y1: Float)`

a rounded `fill_rect` restricted to a clip box (CSS px), the fill counterpart of the above.

### `fn stroke(c: CpuCanvas, r: Rect, col: Color, width: Float)`

stroke the outline of `r` with `col`, `width` px thick.

### `fn text(c: CpuCanvas, pos: Vec2, str: Str, size: Float, col: Color)`

draw `text` at (pos.x, baseline pos.y) at `size` px in `col`.

### `fn text_clip(c: CpuCanvas, pos: Vec2, str: Str, size: Float, col: Color, x0: Float, y0: Float, x1: Float, y1: Float)`

like `text`, but clipped to the rectangle (x0,y0)-(x1,y1), for text inside a scrolled/overflowing box.

### `fn blit_rgba(c: CpuCanvas, r: Rect, src: Int, iw: Int, ih: Int)`

blit a straight RGBA8 image (`src` = iw*ih*4 bytes, byte0=R..byte3=A) into `r` (physical px),
nearest-neighbor scaled and alpha-blended over the canvas. See std/image + render::image.

### `fn pixel_at(c: CpuCanvas, x: Int, y: Int) -> Int`

the 0x00RRGGBB pixel at (x,y) (0 if out of bounds).

### `fn render_overlay(c: CpuCanvas, ov: ComputedOverlay)`

walk a ComputedOverlay's paint primitives and draw them into the canvas (fills, borders, text),
honoring each primitive's clip rect.

### `fn render_overlay_opts(c: CpuCanvas, ov: ComputedOverlay, shadows: Bool)`

like render_overlay, but with `shadows` = draw a soft drop shadow behind each rounded, opaque
filled rect (panels/buttons) before it, a visible effect layered on the same primitive set.

### `fn render_overlay_scaled(c: CpuCanvas, ov: ComputedOverlay, shadows: Bool, s: Float)`

render an overlay laid out in logical coordinates into a buffer that is `s` times larger in each
axis; every geometry (rects, text, radius, borders) is multiplied by `s`. For HiDPI/fractional
scaling: lay out in logical units, render at physical resolution (crisp), and let the compositor's
viewport map the physical buffer back to logical size. `s = 1.0` is the ordinary path.

### `fn render_overlay_damage(c: CpuCanvas, ov: ComputedOverlay, damage: Vec<Rect>, shadows: Bool, s: Float, bg: Color)`

Repaint only what changed. `damage` is a list of logical rects (a retained document's
`rdoc_frame(...).damage`); each is cleared to `bg` and every primitive that reaches into it is
drawn again, clipped to it. Everything outside the damage is left as the previous frame drew it,
so the canvas must hold that frame: draw the first frame (and any `full` one) with
`clear` + `render_overlay_scaled`, and every later one with this.

The result is the same pixels a full repaint gives: every pixel inside a damaged rect is
recomputed from the background up, in paint order, with the same coverage; clipping only
decides which pixels are written, never how. Drop shadows are clipped to the damage and not to
their primitive's clip, as the full repaint draws them.

### `fn render_dock(c: CpuCanvas, res: DockLayoutResult, s: Float)`

draw a dock's chrome (panels, tab bars, splitters, buttons) from `layout_dock_ui`, geometry scaled
by `s` (1.0 = logical). Draw per-panel content yourself over it.

### `fn render_windows(c: CpuCanvas, wr: WindowLayoutResult, s: Float)`

draw floating-window chrome (title bars, borders, buttons) from `layout_windows`, scaled by `s`.
Draw per-window content yourself (iterate `layout_windows(...).windows` for each `content_rect`).


