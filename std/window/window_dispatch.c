/* The window backend seam, resolved at runtime.
 *
 * Three implementations answer the same 26 calls: window_wayland.c and window_android.c export
 * nori_sfc_* (a compositor surface; the two are never linked together, the manifest swaps them),
 * and window_x11.c exports nori_x11_*. A Linux desktop can be running either display server, and
 * which one is not knowable until the process starts — so both are linked and open() picks.
 *
 * Everything above this file calls nori_win_*, and a window carries the backend that opened it, so
 * a program that opens two windows under two servers still routes each call correctly.
 *
 * NORI_HAVE_X11 is what the X11 half is compiled against. Android has no X11 library to link, so
 * an Android build leaves it undefined and every call below goes straight to the surface backend.
 */
#include <stdlib.h>
#include <string.h>

#define BK_SFC 0
#define BK_X11 1

typedef struct { int backend; void* h; } NoriWin;

extern void* nori_sfc_open(const char* title, int w, int h);
extern int   nori_sfc_poll_event(void* h, int* out);
extern int   nori_sfc_mouse(void* h, int* out_xy);
extern int   nori_sfc_lock_pointer(void* h, int on);
extern int   nori_sfc_pointer_locked(void* h);
extern int   nori_sfc_pointer_delta(void* h, int* out);
extern int   nori_sfc_mouse_x(void* h);
extern int   nori_sfc_mouse_y(void* h);
extern int   nori_sfc_mouse_down(void* h);
extern int   nori_sfc_mouse_middle(void* h);
extern void  nori_sfc_shm_present(void* h, void* pixels, int ww, int hh);
extern void  nori_sfc_frame_wait(void* h, int timeout_ms);
extern int   nori_sfc_scale120(void* h);
extern void  nori_sfc_soft_keyboard(void* h, int show);
extern int   nori_sfc_catch_back(void* h, int on);
extern int   nori_sfc_set_cursor(void* h, int shape);
extern int   nori_sfc_inset_top(void* h);
extern int   nori_sfc_scroll_unit_1000(void* h);
extern int   nori_sfc_has_hover(void* h);
extern void  nori_sfc_apply_viewport(void* h);
extern void  nori_sfc_clipboard_set(void* h, const char* text);
extern char* nori_sfc_clipboard_get(void* h);
extern void  nori_sfc_start_drag(void* h, const char* text);
extern int   nori_sfc_drag_finished(void* h);
extern char* nori_sfc_drop_text(void* h);
extern int   nori_sfc_closed(void* h);
extern int   nori_sfc_drawable(void* h);
extern int   nori_sfc_pointer_held(void* h);
extern int   nori_sfc_w(void* h);
extern int   nori_sfc_h(void* h);
extern void* nori_sfc_display(void* h);
extern void* nori_sfc_surface(void* h);
extern void  nori_sfc_close(void* h);

#ifdef NORI_HAVE_X11
extern void* nori_x11_open(const char* title, int w, int h);
extern int   nori_x11_poll_event(void* h, int* out);
extern int   nori_x11_mouse(void* h, int* out_xy);
extern int   nori_x11_lock_pointer(void* h, int on);
extern int   nori_x11_pointer_locked(void* h);
extern int   nori_x11_pointer_delta(void* h, int* out);
extern int   nori_x11_mouse_x(void* h);
extern int   nori_x11_mouse_y(void* h);
extern int   nori_x11_mouse_down(void* h);
extern int   nori_x11_mouse_middle(void* h);
extern void  nori_x11_shm_present(void* h, void* pixels, int ww, int hh);
extern void  nori_x11_frame_wait(void* h, int timeout_ms);
extern int   nori_x11_scale120(void* h);
extern void  nori_x11_soft_keyboard(void* h, int show);
extern int   nori_x11_catch_back(void* h, int on);
extern int   nori_x11_set_cursor(void* h, int shape);
extern int   nori_x11_inset_top(void* h);
extern int   nori_x11_scroll_unit_1000(void* h);
extern int   nori_x11_has_hover(void* h);
extern void  nori_x11_apply_viewport(void* h);
extern void  nori_x11_clipboard_set(void* h, const char* text);
extern char* nori_x11_clipboard_get(void* h);
extern void  nori_x11_start_drag(void* h, const char* text);
extern int   nori_x11_drag_finished(void* h);
extern char* nori_x11_drop_text(void* h);
extern int   nori_x11_closed(void* h);
extern int   nori_x11_drawable(void* h);
extern int   nori_x11_pointer_held(void* h);
extern int   nori_x11_w(void* h);
extern int   nori_x11_h(void* h);
extern void* nori_x11_display(void* h);
extern void* nori_x11_surface(void* h);
extern void  nori_x11_close(void* h);
#endif

/* Which server to try first. WAYLAND_DISPLAY is set by a Wayland session and absent under a plain
 * X server, which is the same test a toolkit makes. NORI_WINDOW_BACKEND overrides it, so a machine
 * running one server can still be used to exercise the other path. */
static int nori_win_preferred(void) {
#ifdef NORI_HAVE_X11
    const char* forced = getenv("NORI_WINDOW_BACKEND");
    if (forced && *forced) {
        if (!strcmp(forced, "x11")) return BK_X11;
        if (!strcmp(forced, "wayland") || !strcmp(forced, "surface")) return BK_SFC;
    }
    const char* wl = getenv("WAYLAND_DISPLAY");
    if (wl && *wl) return BK_SFC;
    return BK_X11;
#else
    return BK_SFC;
#endif
}

static void* nori_win_try(int backend, const char* title, int w, int h) {
    if (backend == BK_SFC) return nori_sfc_open(title, w, h);
#ifdef NORI_HAVE_X11
    if (backend == BK_X11) return nori_x11_open(title, w, h);
#endif
    return 0;
}

/* A backend that cannot connect returns 0 rather than failing loudly, so the other one is tried
 * before giving up: a session can have WAYLAND_DISPLAY set with a compositor that refuses the
 * connection, and an X server reachable the whole time. */
void* nori_win_open(const char* title, int w, int h) {
    int want = nori_win_preferred();
    void* inner = nori_win_try(want, title, w, h);
    int got = want;
    if (!inner) {
        got = (want == BK_SFC) ? BK_X11 : BK_SFC;
        inner = nori_win_try(got, title, w, h);
    }
    if (!inner) return 0;
    NoriWin* nw = (NoriWin*)malloc(sizeof(NoriWin));
    if (!nw) return 0;
    nw->backend = got;
    nw->h = inner;
    return nw;
}

/* 0 = compositor surface (Wayland or Android), 1 = X11. A GPU backend needs this to know whether to
 * build its surface from display+surface or from display+window. */
int nori_win_backend(void* h) { return h ? ((NoriWin*)h)->backend : BK_SFC; }

#ifdef NORI_HAVE_X11
#define NORI_FWD(rt, name, params, args, dflt)                 \
    rt nori_win_##name params {                                \
        if (!h) return dflt;                                   \
        NoriWin* nw = (NoriWin*)h;                             \
        if (nw->backend == BK_X11) return nori_x11_##name args;\
        return nori_sfc_##name args;                           \
    }
#define NORI_FWD_V(name, params, args)                         \
    void nori_win_##name params {                              \
        if (!h) return;                                        \
        NoriWin* nw = (NoriWin*)h;                             \
        if (nw->backend == BK_X11) { nori_x11_##name args; return; } \
        nori_sfc_##name args;                                  \
    }
#else
#define NORI_FWD(rt, name, params, args, dflt)                 \
    rt nori_win_##name params {                                \
        if (!h) return dflt;                                   \
        NoriWin* nw = (NoriWin*)h;                             \
        return nori_sfc_##name args;                           \
    }
#define NORI_FWD_V(name, params, args)                         \
    void nori_win_##name params {                              \
        if (!h) return;                                        \
        NoriWin* nw = (NoriWin*)h;                             \
        nori_sfc_##name args;                                  \
    }
#endif

NORI_FWD(int,   poll_event,       (void* h, int* out),                    (nw->h, out),          0)
NORI_FWD(int,   mouse,            (void* h, int* out_xy),                 (nw->h, out_xy),       0)
NORI_FWD(int,   mouse_x,          (void* h),                              (nw->h),              -1)
NORI_FWD(int,   mouse_y,          (void* h),                              (nw->h),              -1)
/* pointer_at's position: the C backends say it as mouse_x/y do */
int nori_win_pointer_x(void* h) { return nori_win_mouse_x(h); }
int nori_win_pointer_y(void* h) { return nori_win_mouse_y(h); }
NORI_FWD(int,   mouse_down,       (void* h),                              (nw->h),               0)
NORI_FWD(int,   mouse_middle,     (void* h),                              (nw->h),               0)
NORI_FWD(int,   scale120,         (void* h),                              (nw->h),             120)
NORI_FWD(int,   inset_top,        (void* h),                              (nw->h),               0)
NORI_FWD(int,   scroll_unit_1000, (void* h),                              (nw->h),            4000)
NORI_FWD(int,   has_hover,        (void* h),                              (nw->h),               1)
NORI_FWD(int,   drag_finished,    (void* h),                              (nw->h),               0)
NORI_FWD(int,   closed,           (void* h),                              (nw->h),               1)
NORI_FWD(int,   drawable,         (void* h),                              (nw->h),               0)
NORI_FWD(int,   pointer_held,     (void* h),                              (nw->h),               0)
NORI_FWD(int,   w,                (void* h),                              (nw->h),               0)
NORI_FWD(int,   h,                (void* h),                              (nw->h),               0)
NORI_FWD(char*, clipboard_get,    (void* h),                              (nw->h),               0)
NORI_FWD(char*, drop_text,        (void* h),                              (nw->h),               0)
NORI_FWD(void*, display,          (void* h),                              (nw->h),               0)
NORI_FWD(void*, surface,          (void* h),                              (nw->h),               0)

NORI_FWD_V(shm_present,    (void* h, void* pixels, int ww, int hh), (nw->h, pixels, ww, hh))
NORI_FWD_V(frame_wait,     (void* h, int timeout_ms),               (nw->h, timeout_ms))
NORI_FWD_V(soft_keyboard,  (void* h, int show),                     (nw->h, show))
NORI_FWD(int, catch_back,  (void* h, int on),  (nw->h, on),  0)
/* pointer shape: 0 default, 1 text, 2 pointer, 3 resize-EW, 4 resize-NS, 5 grabbing.
 * 0 back means the platform could not apply it; the caller leaves the pointer as it was. */
NORI_FWD(int, set_cursor,  (void* h, int shape), (nw->h, shape), 0)
NORI_FWD_V(apply_viewport, (void* h),                               (nw->h))
NORI_FWD_V(clipboard_set,  (void* h, const char* text),             (nw->h, text))
NORI_FWD_V(start_drag,     (void* h, const char* text),             (nw->h, text))
/* pointer lock: the two backends reach it completely differently (Wayland constrains, X11 grabs
 * and warps; see each file) and answer the same three calls. */
NORI_FWD(int, lock_pointer,    (void* h, int on),      (nw->h, on),      0)
NORI_FWD(int, pointer_locked,  (void* h),              (nw->h),          0)
NORI_FWD(int, pointer_delta,   (void* h, int* out),    (nw->h, out),     0)

/* close frees the wrapper too, so the handle above this seam is dead after it, exactly as before. */
void nori_win_close(void* h) {
    if (!h) return;
    NoriWin* nw = (NoriWin*)h;
#ifdef NORI_HAVE_X11
    if (nw->backend == BK_X11) nori_x11_close(nw->h);
    else nori_sfc_close(nw->h);
#else
    nori_sfc_close(nw->h);
#endif
    free(nw);
}

/* The desktop shell's half (std/window/window_shell.nori) is served by the native build only
 * (__native_wayland_shell.nori): the C Wayland shim would need the layer-shell and foreign-toplevel
 * protocols' generated code, and X11 and Android have no such protocols. Every call answers "not available" — the answer a compositor
 * without the protocols gets — and nori_win_fd offers no descriptor: poll the window instead. */
int   nori_win_fd(void* h) { (void)h; return -1; }
int   nori_win_dispatch(void* h) { return h ? 1 : -1; }   /* no descriptor to sleep on: always "poll it" */
void* nori_win_open_layer(const char* ns, const long* spec) { (void)ns; (void)spec; return 0; }
void  nori_win_layer_keyboard(void* h, int mode) { (void)h; (void)mode; }
void  nori_win_layer_size(void* h, int width, int height) { (void)h; (void)width; (void)height; }
void  nori_win_start_drag_files(void* h, const char* uris, const char* plain) { (void)h; (void)uris; (void)plain; }
void  nori_win_input_none(void* h, int none) { (void)h; (void)none; }
/* keyboard-shortcuts-inhibit is the native Wayland backend's alone; the C shims refuse, as win32 and absent do */
int   nori_win_inhibit_shortcuts(void* h, int on) { (void)h; (void)on; return 0; }
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
