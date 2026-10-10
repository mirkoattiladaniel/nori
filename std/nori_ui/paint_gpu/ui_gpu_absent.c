/* The GPU backend, absent.
 *
 * std/nori_ui carries both renderers, so importing it declares the ui_gpu_* seam whether or not a
 * program wants the GPU. A CPU-only program (a headless page render, an image test, a software UI)
 * would otherwise have to link wgpu-native and ship libwgpu_native.so just to satisfy symbols it
 * never calls. Compiling this file instead of ui_gpu_glue.c satisfies them with nothing.
 *
 * ui_gpu_ok returns 0, the same answer the real backend gives on a machine with no Vulkan device.
 * ui::render::try_renderer reports it as an unavailable backend, so a program linking this behaves
 * like one running on a box without a GPU, through the same code path.
 *
 * Link one of these two, never both:
 *   cfile = std/nori_ui/paint_gpu/ui_gpu_glue.c     + clink wgpu_native   -> a real GPU renderer
 *   cfile = std/nori_ui/paint_gpu/ui_gpu_absent.c                         -> CPU only
 */
void* ui_gpu_new_offscreen(int w, int h) { (void)w; (void)h; return 0; }
void* ui_gpu_new_window(void* d, unsigned long long win, int w, int h) { (void)d; (void)win; (void)w; (void)h; return 0; }
void* ui_gpu_new_window_wl(void* d, void* s, int w, int h) { (void)d; (void)s; (void)w; (void)h; return 0; }
int   ui_gpu_ok(void* c) { (void)c; return 0; }
void  ui_gpu_resize(void* c, int w, int h) { (void)c; (void)w; (void)h; }
void  ui_gpu_begin(void* c, double r, double g, double b) { (void)c; (void)r; (void)g; (void)b; }
void  ui_gpu_rect(void* c, double a1, double a2, double a3, double a4, double a5,
                  double a6, double a7, double a8, double a9, double a10,
                  double a11, double a12, double a13, double a14,
                  double a15, double a16, double a17, double a18) {
    (void)c; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7; (void)a8; (void)a9; (void)a10;
    (void)a11; (void)a12; (void)a13; (void)a14; (void)a15; (void)a16; (void)a17; (void)a18;
}
void  ui_gpu_glyph(void* c, double a1, double a2, double a3, double a4, double a5, double a6,
                   double a7, double a8, double a9, double a10, double a11, double a12,
                   double a13, double a14, double a15, double a16,
                   double a17, double a18, double a19, double a20) {
    (void)c; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    (void)a7; (void)a8; (void)a9; (void)a10; (void)a11; (void)a12;
    (void)a13; (void)a14; (void)a15; (void)a16; (void)a17; (void)a18; (void)a19; (void)a20;
}
void  ui_gpu_atlas(void* c, void* px, int w, int h, int gen) { (void)c; (void)px; (void)w; (void)h; (void)gen; }
int   ui_gpu_atlas_gen(void* c) { (void)c; return 0; }
void  ui_gpu_atlas_color(void* c, void* px, int w, int h) { (void)c; (void)px; (void)w; (void)h; }
void  ui_gpu_image(void* c, void* px, int w, int h, double x0, double y0, double x1, double y1, double a) {
    (void)c; (void)px; (void)w; (void)h; (void)x0; (void)y0; (void)x1; (void)y1; (void)a;
}
/* No device to share and nothing to blit, but the symbols must exist or a CPU-only link fails. */
void* ui_gpu_device(void* c) { (void)c; return 0; }
void* ui_gpu_queue(void* c) { (void)c; return 0; }
int   ui_gpu_target_format(void* c) { (void)c; return 0; }
void* ui_gpu_adapter(void* c) { (void)c; return 0; }
void* ui_gpu_instance(void* c) { (void)c; return 0; }
void  ui_gpu_image_view(void* c, void* v, double x, double y, double w, double h, double a, int linear, int flip_v) {
    (void)c; (void)v; (void)x; (void)y; (void)w; (void)h; (void)a; (void)linear; (void)flip_v;
}
void  ui_gpu_clip(void* c, double x0, double y0, double x1, double y1) {
    (void)c; (void)x0; (void)y0; (void)x1; (void)y1;
}
void  ui_gpu_clip_none(void* c) { (void)c; }
void  ui_gpu_end(void* c) { (void)c; }
int   ui_gpu_readback(void* c, void* out) { (void)c; (void)out; return 0; }
/* Every extern in ui_gpu.nori needs an answer here, including the ones a program will never call.
   `std/nori_ui` declares the whole GPU seam whether or not a build wants it, so a symbol added to
   the real glue and not to this file makes `dlopen` fail for the entire library: the app dies at
   launch with "cannot locate symbol" before any of its code runs, and nothing in the failure points
   at the GPU. */
int   ui_gpu_readback_pipelined(void* c, void* out) { (void)c; (void)out; return 0; }
void  ui_gpu_free(void* c) { (void)c; }
