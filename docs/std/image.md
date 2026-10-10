# std/image

```nori
import "std/image" as image
```

### `fn ANIM_MAX_PX() -> Int`

the most pixels a canvas may have (64 Mpx: 256 MB of RGBA). A file asking for more is refused.

### `fn ANIM_NONE() -> Int`

the formats `anim_kind` names.

### `struct Anim`

an animation being played: the file's bytes, the canvas, and where the next frame starts.

### `fn anim_open(sink data: Str) -> Anim`

open an animated (or still) picture from its bytes: GIF, PNG/APNG, WebP, or anything `load` reads. The
canvas is made; no frame is decoded until `anim_next`. `anim_ok` says whether it opened, `anim_error` why not.

### `fn anim_ok(a: Anim) -> Bool`

did it open?

### `fn anim_error(a: Anim) -> Str`

why it did not open, or why the last frame did not decode ("" when all is well).

### `fn anim_kind(a: Anim) -> Int`

which format: ANIM_GIF, ANIM_PNG, ANIM_WEBP, or ANIM_STILL (a picture with one frame read by `load`).

### `fn anim_width(a: Anim) -> Int`

the canvas's width.

### `fn anim_height(a: Anim) -> Int`

the canvas's height.

### `fn anim_pixels(a: Anim) -> Int`

the canvas: w*h*4 bytes of straight RGBA8, the last decoded frame as it is shown.

### `fn anim_frames(a: Anim) -> Int`

how many frames the file has.

### `fn anim_loops(a: Anim) -> Int`

how many times the file asks to be played (0: for ever).

### `fn anim_delay_ms(a: Anim) -> Int`

the delay of the last decoded frame, in milliseconds, as the file says it (0 for a still picture).

### `fn anim_index(a: Anim) -> Int`

the index of the frame `anim_next` decodes next (0 after `anim_rewind`).

### `fn anim_moves(a: Anim) -> Bool`

does it move? (more than one frame)

### `fn anim_next(inout a: Anim) -> Bool`

decode the next frame onto the canvas. false at the end of the animation (the canvas keeps the last frame)
or when the frame does not decode (`anim_error` says why).

### `fn anim_rewind(inout a: Anim)`

back to the start: the canvas cleared, the next `anim_next` decodes the first frame.

### `fn anim_close(inout a: Anim)`

let go of the canvas, the tables and the bytes.

### `fn anim_image(a: Anim) -> Image`

the canvas as an `Image` of its own (a copy): the frame now shown, to keep or to save.






std/image: image decoding. Decodes a PNG to a straight RGBA8 pixel buffer (byte order
R,G,B,A) that a renderer can blit into a panel (see std/nori_ui/render::image / paint_cpu::image).
Reuses std/archive's DEFLATE inflate for the IDAT zlib stream: no libpng, no C.
  import "std/image" as image
  var im = image::load_png(read_file("logo.png"))
  if image::ok(im) { render::image(r, rect, im) }
  image::dispose(im)
Supported: 8-bit non-interlaced PNG, color types 0 (gray), 2 (RGB), 3 (palette+tRNS),
4 (gray+alpha), 6 (RGBA). Unsupported inputs return an image with ok()==false.
### `struct Image`

a decoded image: `w` x `h` straight RGBA8 at `pixels` (w*h*4 bytes, byte0=R..byte3=A).

### `fn ok(im: Image) -> Bool`

did decoding succeed?

### `fn width(im: Image) -> Int`

width in pixels.

### `fn height(im: Image) -> Int`

height in pixels.

### `fn pixels(im: Image) -> Int`

raw RGBA8 buffer pointer (w*h*4 bytes).

### `fn pixel_rgba(im: Image, x: Int, y: Int) -> Int`

one pixel packed as R<<24 | G<<16 | B<<8 | A (for tests / sampling).

### `fn dispose(inout im: Image)`

release the pixel buffer.

### `fn image_new(w: Int, h: Int) -> Image`

a fresh w*h transparent (all-zero RGBA8) image whose buffer a renderer (e.g. std/svg) fills in place.

### `fn resize(im: Image, nw: Int, nh: Int) -> Image`

resize `im` to `nw` x `nh` using box-average sampling (nice for downscaling thumbnails; upscaling
falls back to nearest). Returns a fresh RGBA8 image the caller must `dispose`; ok()==false if `im`
is bad or the target dims are non-positive.

### `fn load_png(data: Str) -> Image`

decode a PNG (from read_file bytes). Returns ok()==false on malformed / unsupported input.

### `fn load_jpeg(data: Str) -> Image`

decode a baseline JPEG (from read_file bytes). Returns ok()==false on malformed / progressive /
arithmetic / unsupported input.

### `fn load_gif(data: Str) -> Image`

decode a GIF (87a/89a) into RGBA8. Animated files give up their first frame, composed onto a
transparent canvas at its own left/top offset: what a browser paints before the timer starts.
Interlacing and the transparent index are honored; `ok(im)` reports whether it decoded.

### `fn load_webp(data: Str) -> Image`

decode a WebP (lossy, lossless, with alpha, or animated: its first frame) into RGBA8.

### `fn load(data: Str) -> Image`

decode an image by sniffing its magic bytes: JPEG (FF D8), GIF ("GIF8"), WebP ("RIFF"...."WEBP"), else PNG.
An animation gives its first frame (`anim_open` plays it).

### `fn png_gray() -> Int`

colour type 0: 8-bit greyscale. The red channel is written (an `Image` decoded from greyscale has R=G=B).

### `fn png_rgb() -> Int`

colour type 2: 8-bit truecolour RGB. Alpha is dropped.

### `fn png_palette() -> Int`

colour type 3: 8-bit palette indices. Only via `encode_png_paletted` / `save_png_paletted`.

### `fn png_gray_alpha() -> Int`

colour type 4: 8-bit greyscale + alpha.

### `fn png_rgba() -> Int`

colour type 6: 8-bit truecolour + alpha. Lossless for any `Image`.

### `fn encode_png(im: Image, ctype: Int) -> Str`

encode `im` as a PNG at colour type `ctype` (`png_gray`, `png_rgb`, `png_gray_alpha`, `png_rgba`).
Returns "" if `im` is bad or `ctype` is not one of those: palette output is `encode_png_paletted`.

`png_rgba` is the only lossless choice for an arbitrary `Image`: `png_rgb` and `png_gray` drop
alpha, and `png_gray`/`png_gray_alpha` write the red channel as the grey level.

### `fn encode_png_paletted(im: Image, pal: Vec<Int>) -> Str`

encode `im` as a colour-type-3 PNG against the caller's `pal` (R<<24|G<<16|B<<8|A entries, 1..256
of them; a tRNS chunk is written when any entry is not opaque).

There is no quantiser here: every pixel must already be one of the palette's colours exactly, and
a pixel that is not returns "". Pick the palette from the image you are about to write, or write
`png_rgba` instead.

### `fn save_png(im: Image, path: Str, ctype: Int) -> Bool`

write `im` to `path` as a PNG at colour type `ctype`. false if it could not be encoded.

### `fn save_png_paletted(im: Image, path: Str, pal: Vec<Int>) -> Bool`

write `im` to `path` as a colour-type-3 PNG against `pal`. false if it could not be encoded.

### `fn encode_png_readback(px: Int, w: Int, h: Int) -> Str`

encode a `w` x `h` framebuffer of little-endian 0x00RRGGBB words at `px` (the layout
`std/nori_ui/paint_gpu::gpu_readback` and `paint_cpu` produce), as a truecolour PNG.

This is the raw-pointer door into the same encoder. A readback buffer is not an `Image` and
copying three megabytes into one only to have the encoder read it back out again is work for
nothing, so the pixel walk is parameterised on the layout instead.

### `fn encode_png_readback_fast(px: Int, w: Int, h: Int) -> Str`

`encode_png_readback`, fast: every scanline unfiltered and the image data stored, not compressed.
The same pixels, a file several times larger, in a fraction of the time: for a screenshot that
is read once and deleted.

### `fn save_png_readback_fast(path: Str, px: Int, w: Int, h: Int) -> Bool`

`save_png_readback`, fast: see `encode_png_readback_fast`.

### `fn save_png_readback(path: Str, px: Int, w: Int, h: Int) -> Bool`

write a `w` x `h` readback buffer (little-endian 0x00RRGGBB words) to `path` as a truecolour PNG.








### `struct YuvImage`

a lossy WebP's picture as the codec decodes it: YUV 4:2:0 (BT.601, limited range), before any conversion to
RGB: for a renderer that converts on the GPU, and for checking the decoder against others bit for bit.

### `fn load_webp_yuv(data: Str) -> YuvImage`

decode a lossy WebP (simple or extended; an animation's first frame is not looked for) to its YUV planes.
`yuv_ok` says whether it decoded; `yuv_dispose` lets the planes go.

### `fn yuv_ok(im: YuvImage) -> Bool`

did the planes decode?

### `fn yuv_dispose(inout im: YuvImage)`

let the planes go.


