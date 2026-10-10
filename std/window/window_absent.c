/* The window backend, absent.
 *
 * std/nori_ui reaches std/window through its renderer front end, so importing the UI declares the
 * nori_win_* seam whether or not a program ever opens a window. A headless program — a page rendered
 * to a PPM, a layout test, an image pipeline — would otherwise have to link a display server's client
 * libraries and compile three backends just to satisfy symbols it never calls. Compiling this file
 * instead of window_dispatch.c + the per-platform backends satisfies them with nothing.
 *
 * nori_win_open returns 0, which is not an error: it is the same answer the dispatch
 * layer gives when no display server accepts the connection, and it is the answer `window::opened`
 * already reports as false. Every other call takes a handle, so on a build that links this file they
 * are only ever reached with 0 — and each returns what the dispatch layer returns for a null
 * handle, so nothing above the seam can tell the two apart.
 *
 * Link one of these, never both:
 *   cfile = std/window/window_dispatch.c, window_wayland.c, window_x11.c   -> a real window
 *   cfile = std/window/window_absent.c                                     -> headless
 */

void* nori_win_open(const char* title, int w, int h) { (void)title; (void)w; (void)h; return 0; }
int   nori_win_backend(void* h) { (void)h; return 0; }

int   nori_win_poll_event(void* h, int* out) { (void)h; (void)out; return 0; }
int   nori_win_mouse(void* h, int* out_xy) { (void)h; (void)out_xy; return 0; }
/* No display server, so a pointer-lock request is simply never granted and the delta is always
   the one a locked pointer that has not moved would report. */
int   nori_win_lock_pointer(void* h, int on) { (void)h; (void)on; return 0; }
int   nori_win_inhibit_shortcuts(void* h, int on) { (void)h; (void)on; return 0; }
int   nori_win_pointer_locked(void* h) { (void)h; return 0; }
int   nori_win_pointer_delta(void* h, int* out) { (void)h; if (out) { out[0] = 0; out[1] = 0; } return 0; }
int   nori_win_mouse_x(void* h) { (void)h; return -1; }
int   nori_win_mouse_y(void* h) { (void)h; return -1; }
int   nori_win_pointer_x(void* h) { (void)h; return -1; }
int   nori_win_pointer_y(void* h) { (void)h; return -1; }
int   nori_win_mouse_down(void* h) { (void)h; return 0; }
/* The button state IS the held state here: a mouse reports press and release, so nothing
   has to be latched to notice a drag. */
int   nori_win_pointer_held(void* h) { (void)h; return 0; }
int   nori_win_mouse_middle(void* h) { (void)h; return 0; }
int   nori_win_scale120(void* h) { (void)h; return 120; }
int   nori_win_inset_top(void* h) { (void)h; return 0; }
int   nori_win_scroll_unit_1000(void* h) { (void)h; return 4000; }
int   nori_win_has_hover(void* h) { (void)h; return 1; }
int   nori_win_drag_finished(void* h) { (void)h; return 0; }
int   nori_win_closed(void* h) { (void)h; return 1; }
int   nori_win_drawable(void* h) { (void)h; return 0; }   /* no window, nothing to draw on */
int   nori_win_w(void* h) { (void)h; return 0; }
int   nori_win_h(void* h) { (void)h; return 0; }
char* nori_win_clipboard_get(void* h) { (void)h; return 0; }
char* nori_win_drop_text(void* h) { (void)h; return 0; }
void* nori_win_display(void* h) { (void)h; return 0; }
void* nori_win_surface(void* h) { (void)h; return 0; }

void  nori_win_shm_present(void* h, void* pixels, int ww, int hh) { (void)h; (void)pixels; (void)ww; (void)hh; }
void  nori_win_frame_wait(void* h, int timeout_ms) { (void)h; (void)timeout_ms; }
void  nori_win_soft_keyboard(void* h, int show) { (void)h; (void)show; }
/* No system back gesture here; the Escape key is already an ordinary key event. */
int nori_win_catch_back(void* h, int on) { (void)h; (void)on; return 0; }
int nori_win_set_cursor(void* h, int shape) { (void)h; (void)shape; return 0; }   /* no window, no pointer */
void  nori_win_apply_viewport(void* h) { (void)h; }
void  nori_win_clipboard_set(void* h, const char* text) { (void)h; (void)text; }
void  nori_win_start_drag(void* h, const char* text) { (void)h; (void)text; }
void  nori_win_close(void* h) { (void)h; }

/* The desktop shell's half (std/window/window_shell.nori) is served by the native build only
 * (__native_wayland_shell.nori): the C Wayland shim would need the layer-shell and foreign-toplevel
 * protocols' generated code, and a headless build has no compositor to ask. Every call answers "not available" — the answer a compositor
 * without the protocols gets — and nori_win_fd offers no descriptor: poll the window instead. */
int   nori_win_fd(void* h) { (void)h; return -1; }
int   nori_win_dispatch(void* h) { (void)h; return -1; }
void* nori_win_open_layer(const char* ns, const long* spec) { (void)ns; (void)spec; return 0; }
void  nori_win_layer_keyboard(void* h, int mode) { (void)h; (void)mode; }
void  nori_win_layer_size(void* h, int width, int height) { (void)h; (void)width; (void)height; }
void  nori_win_start_drag_files(void* h, const char* uris, const char* plain) { (void)h; (void)uris; (void)plain; }
void  nori_win_input_none(void* h, int none) { (void)h; (void)none; }
void* nori_win_tl_open(void) { return 0; }
int   nori_win_tl_caps(void* h) { (void)h; return 0; }
int   nori_win_tl_fd(void* h) { (void)h; return -1; }
int   nori_win_tl_dispatch(void* h) { (void)h; return -1; }
int   nori_win_tl_count(void* h) { (void)h; return 0; }
long  nori_win_tl_uid(void* h, int k) { (void)h; (void)k; return 0; }
int   nori_win_tl_state(void* h, int k) { (void)h; (void)k; return 0; }
char* nori_win_tl_str(void* h, int k, int which) { (void)h; (void)k; (void)which; return 0; }
int   nori_win_tl_act(void* h, long uid, int act) { (void)h; (void)uid; (void)act; return 0; }
void  nori_win_tl_close(void* h) { (void)h; }
int   nori_win_ws_count(void* h) { (void)h; return 0; }
long  nori_win_ws_uid(void* h, int k) { (void)h; (void)k; return 0; }
int   nori_win_ws_state(void* h, int k) { (void)h; (void)k; return 0; }
char* nori_win_ws_str(void* h, int k, int which) { (void)h; (void)k; (void)which; return 0; }
int   nori_win_ws_act(void* h, long uid, int act) { (void)h; (void)uid; (void)act; return 0; }
/* translucent presentation, frame pacing, keyboard focus and activation tokens: the native build's */
void  nori_win_set_alpha(void* h, int on) { (void)h; (void)on; }
int   nori_win_frame_done(void* h) { (void)h; return 1; }
int   nori_win_has_keyboard(void* h) { return h ? 1 : 0; }
int   nori_win_next_timeout_ms(void* h) { (void)h; return -1; }
/* window::keymap_parse's decoder is the native build's (__native_xkb.nori): none on this backend */
void* nori_win_keymap_parse(void* p, long n) { (void)p; (void)n; return 0; }
int   nori_win_keymap_keysym(void* km, int code, int mods) { (void)km; (void)code; (void)mods; return 0; }
int   nori_win_keymap_keysym_group(void* km, int code, int mods, int group) { (void)km; (void)code; (void)mods; (void)group; return 0; }
void  nori_win_keymap_free(void* km) { (void)km; }
char* nori_win_activation_token(void* h, const char* app_id) { (void)h; (void)app_id; return 0; }
/* window::set_title / set_app_id after open: the native build's (the C backends keep open's title) */
void  nori_win_set_title(void* h, const char* title) { (void)h; (void)title; }
void  nori_win_set_app_id(void* h, const char* id) { (void)h; (void)id; }
