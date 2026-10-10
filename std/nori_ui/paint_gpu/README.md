# std/nori_ui/paint_gpu

A GPU (wgpu-native) renderer for `std/nori_ui` overlays. Consumes the same `ComputedOverlay` /
`PaintPrimitive` data as `std/nori_ui/paint_cpu`, but draws on the GPU:

- rounded-rect **fills** and **borders** via a signed-distance-field fragment shader (AA edges),
- **text** as textured quads sampling an R8 glyph atlas built by `std/font`.

Two targets: a **window** canvas presents to an X11 wgpu surface; an **offscreen** canvas renders to
a texture you read back with `gpu_readback` (headless, for tests and image snapshots).

## Usage

```nori
import "std/nori_ui" as ui
import "std/font" as font

var gc = ui::paint_gpu::gpu_new_window(disp, win, w, h, myFont)   // disp/win from std/window
ui::paint_gpu::use_font_metrics(myFont)                            // once, before layout
ui::paint_gpu::render_overlay(gc, overlay, ui::core::color_rgb(22, 26, 38))   // draws + presents
```

## Native dependency: wgpu-native

The renderer links **wgpu-native** (v29 C API). The headers (`webgpu.h`, `wgpu.h`) are vendored here.
The shared library `lib/libwgpu_native.so` is a build artifact and is **not** committed (git-ignored);
drop it in `lib/` yourself, e.g. from a
[wgpu-native release](https://github.com/gfx-rs/wgpu-native/releases). Builds pass the search path
with `--ldir std/nori_ui/paint_gpu/lib`.

Any Vulkan device works, including the `llvmpipe` software fallback, so it runs headless in CI.

## `--native` builds

`noric --build --native` links `__native_ui_gpu.nori` instead of `ui_gpu_glue.c` (see `std/native_seams`):
the same renderer in Nori, which opens `libwgpu_native.so` at run time (put `lib/` on `LD_LIBRARY_PATH`).
Offscreen canvases only; a windowed canvas reports no GPU. The GPU tests give identical output built either way.
