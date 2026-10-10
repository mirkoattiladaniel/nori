# std/svg

```nori
import "std/svg" as svg
```

std/svg: an SVG rasterizer. Parses an SVG document (XML) and rasterizes it to an
`image::Image` (straight RGBA8) at a caller-chosen pixel size, so the same `<img>` blit path used
for PNG/JPEG renders vector art too. Reuses std/font's signed-area path rasterizer (raster_*).

  import "std/svg" as svg
  var im = svg::rasterize(read_file("logo.svg"), 128, 128)          // shapes, no <text>
  var im = svg::rasterize_font(bytes, 128, 128, myFont)            // + <text> via std/font

Supports: <path> (M/L/H/V/C/S/Q/T/A + Z, abs+rel), rect (rx/ry), circle, ellipse, line, polygon,
polyline, <g>; presentation via attributes and style=""; fill (nonzero), stroke (width/caps/joins
approximated), opacity/fill-opacity/stroke-opacity; solid colors (#rgb/#rrggbb/rgb()/names/none);
linear & radial gradients (objectBoundingBox + userSpaceOnUse, href stop inheritance); transform
(matrix/translate/scale/rotate/skewX/skewY); viewBox + xMidYMid-meet scaling; <text>/<tspan>.
Out of scope: CSS selectors, filters, clipPath/mask, patterns, <use>, <image>, animation.
### `fn rasterize(src: Str, w: Int, h: Int) -> image::Image`

rasterize an SVG document to an RGBA8 image of `w`x`h` pixels (no <text>).

### `fn rasterize_current(src: Str, w: Int, h: Int, r: Int, g: Int, b: Int) -> image::Image`

rasterize an SVG document whose `currentColor`, and whatever it leaves unpainted, is (r,g,b).
An icon in a page is drawn this way: it carries no colour of its own and takes the text colour
around it, which is what makes one icon set work on a light page and on a dark one.

### `fn rasterize_font(src: Str, w: Int, h: Int, f: font::Font) -> image::Image`

rasterize an SVG document including <text> runs, laid out with `f`.

### `fn rasterize_font_current(src: Str, w: Int, h: Int, f: font::Font, r: Int, g: Int, b: Int) -> image::Image`

rasterize with <text> runs laid out with `f`, and `currentColor` (and unpainted shapes) at (r,g,b).


