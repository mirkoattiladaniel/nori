/* Wayland backend shim for std/window_wayland (compiled via roll/noric --cfile; link -lwayland-client).
   A real xdg-shell client: creates a toplevel, tracks resize/close, and binds wl_seat for pointer +
   keyboard input. Events are queued for a non-blocking poll; the wl_display/wl_surface are exposed so
   a GPU backend (wgpu WaylandSurface) can render into the surface. Also a wl_shm path: present a CPU
   framebuffer (double-buffered) and pace to the compositor via wl_surface.frame callbacks. One
   translation unit (the generated xdg-shell protocol is #included at the bottom). Bound via extern fn. */
#define _GNU_SOURCE
#include <wayland-client.h>
#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <time.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include "xdg-shell-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"

/* event queue entry types (mirrored in window_wayland.nori) */
#define EV_CLOSED  1
#define EV_RESIZED 2
#define EV_KEYDOWN 3
#define EV_KEYUP   4
#define EV_DROP    5
#define EV_SCROLL  6
#define EV_MOUSEDOWN 7
#define EV_MOUSEUP   8
#define QN 512

/* c is the third payload word: only the mouse events use it (a = button, b = x, c = y). */
typedef struct { int type, a, b, c; } WlEv;

typedef struct { struct wl_buffer* buf; void* mem; int busy; } ShmBuf;

typedef struct {
    struct wl_display* dpy; struct wl_compositor* comp; struct xdg_wm_base* wm;
    struct wl_surface* surf; struct xdg_surface* xsurf; struct xdg_toplevel* top;
    struct wl_seat* seat; struct wl_pointer* ptr; struct wl_keyboard* kbd;
    struct wl_shm* shm;
    struct wp_viewporter* vpr; struct wp_viewport* vp;
    struct wp_fractional_scale_manager_v1* fsmgr; struct wp_fractional_scale_v1* fscale;
    struct zxdg_decoration_manager_v1* decomgr; struct zxdg_toplevel_decoration_v1* deco;
    struct wl_data_device_manager* ddm; struct wl_data_device* ddev;
    struct wl_data_offer* sel_offer; struct wl_data_source* clip_src; char* clip_text;
    /* drag-and-drop (drop target) */
    struct wl_data_offer* pending_offer; int pending_uri, pending_text;   /* mimes of the newest offer */
    struct wl_data_offer* drag_offer; const char* drag_mime;               /* the in-flight hover offer */
    struct wl_data_offer* drop_offer; const char* drop_mime;               /* handed off at drop() */
    char* drop_text; int drop_pending;
    struct wl_data_source* drag_src; char* drag_src_text; int drag_done;   /* drag source (cross-window) */
    uint32_t serial;                 /* latest input event serial (needed for set_selection) */
    int scale120;                    /* preferred fractional scale in 120ths (120 = 1.0) */
    int closed, w, h;
    int px, py, btn, mid;            /* pointer state (surface-local px, left/middle button held) */
    int lx, ly;                      /* where the left button was pressed (the latch's position) */
    int tap_latch;                   /* a press not yet observed by a polled read; see nori_sfc_mouse_down */
    struct xkb_context* xkb_ctx; struct xkb_keymap* xkb_map; struct xkb_state* xkb_st;   /* keymap-based translation */
    int mods;                        /* current modifier bitmask: 1=shift 2=ctrl 4=alt 8=logo */
    /* key repeat (Wayland has no auto-repeat; the client synthesizes it from repeat_info) */
    int rep_active; uint32_t rep_sym, rep_key; long rep_next_ms; int rep_delay_ms, rep_rate_ms;
    WlEv q[QN]; int qhead, qtail;    /* event ring buffer */
    /* wl_shm double-buffered framebuffer path (CPU renderer) */
    struct wl_shm_pool* pool; int shm_fd; void* shm_map; size_t shm_size; int shm_w, shm_h;
    ShmBuf bufs[2];
    struct wl_callback* frame_cb; int frame_pending;
    /* Pointer lock. Wayland has no pointer warping, so a first-person camera needs two
     * extensions: pointer-constraints to pin the cursor and hide it, relative-pointer to keep
     * delivering motion once it can no longer move. `rel_dx/dy` accumulate between polls in the
     * same 1/256 wl_fixed units the rest of this file uses; the reader takes and clears them. */
    struct zwp_pointer_constraints_v1* pcon; struct zwp_relative_pointer_manager_v1* rpmgr;
    struct zwp_locked_pointer_v1* lock; struct zwp_relative_pointer_v1* relptr;
    /* Pointer shape, via cursor-shape-v1: the compositor draws the named shape itself, so there is
     * no cursor theme to load, no image to decode and no extra library to link, which is why this
     * is used instead of libwayland-cursor. A compositor without the protocol binds nothing here and every
     * request is answered with 0, leaving the pointer as it was. */
    struct wp_cursor_shape_manager_v1* shmgr; struct wp_cursor_shape_device_v1* shdev;
    uint32_t ptr_serial;             /* the pointer enter serial, which is the one set_shape wants */
    int shape_cur;                   /* shape currently requested, so a per-frame call is free */
    int locked;                      /* the caller's request, independent of whether the compositor obliged */
    double rel_dx, rel_dy;           /* accumulated relative motion since the last read */
} NoriWl;

static void push_ev3(NoriWl* w, int type, int a, int b, int c) {
    int n = (w->qtail + 1) % QN;
    if (n != w->qhead) { w->q[w->qtail].type = type; w->q[w->qtail].a = a; w->q[w->qtail].b = b; w->q[w->qtail].c = c; w->qtail = n; }
}
static void push_ev(NoriWl* w, int type, int a, int b) { push_ev3(w, type, a, b, 0); }

/* ---- xdg_wm_base ping ---- */
static void wm_ping(void* d, struct xdg_wm_base* wm, uint32_t s) { (void)d; xdg_wm_base_pong(wm, s); }
static const struct xdg_wm_base_listener wm_l = { wm_ping };

/* ---- pointer ---- */
static void ptr_enter(void* d, struct wl_pointer* p, uint32_t s, struct wl_surface* sf, wl_fixed_t x, wl_fixed_t y) {
    (void)p;(void)sf; NoriWl* w = (NoriWl*)d; w->serial = s; w->px = wl_fixed_to_int(x); w->py = wl_fixed_to_int(y);
    /* the pointer keeps no shape across an enter: whatever was set is forgotten, so record this
     * serial (set_shape needs the enter one) and forget what we think is showing. */
    w->ptr_serial = s; w->shape_cur = -1;
}
/* the pointer is elsewhere: no position here (pointer_at says -1, -1, as before it entered) */
static void ptr_leave(void* d, struct wl_pointer* p, uint32_t s, struct wl_surface* sf) {
    (void)p;(void)s; NoriWl* w = (NoriWl*)d; if (sf == w->surf) { w->px = -1; w->py = -1; }
}
static void ptr_motion(void* d, struct wl_pointer* p, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    (void)p;(void)t; NoriWl* w = (NoriWl*)d; w->px = wl_fixed_to_int(x); w->py = wl_fixed_to_int(y);
}
/* A click is shorter than a frame, so keeping only the button state would lose it: a press and
   its release that both arrive between two polls net btn=0 and the click never happens. Every
   press and release is queued as its own event, in order, with the pointer position at the time.
   Nothing is coalesced, so three clicks between two frames are three events and the button that
   produced each one is still known. The state fields stay for the polled API (with the same
   latch the Android backend has, below). */
static void ptr_button(void* d, struct wl_pointer* p, uint32_t s, uint32_t t, uint32_t button, uint32_t state) {
    (void)p;(void)t; NoriWl* w = (NoriWl*)d; w->serial = s;
    int pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? 1 : 0;
    int id = 0;
    if (button == BTN_LEFT) { w->btn = pressed; id = 1; if (pressed) { w->lx = w->px; w->ly = w->py; w->tap_latch = 1; } }
    else if (button == BTN_MIDDLE) { w->mid = pressed; id = 2; }
    else if (button == BTN_RIGHT) { id = 3; }
    if (id) push_ev3(w, pressed ? EV_MOUSEDOWN : EV_MOUSEUP, id, w->px, w->py);
}
static void ptr_axis(void* d, struct wl_pointer* p, uint32_t t, uint32_t axis, wl_fixed_t v) {
    (void)p;(void)t; NoriWl* w = (NoriWl*)d;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) { int dv = wl_fixed_to_int(v); if (dv != 0) push_ev(w, EV_SCROLL, dv, 0); }
}
static const struct wl_pointer_listener ptr_l = {
    .enter = ptr_enter, .leave = ptr_leave, .motion = ptr_motion, .button = ptr_button, .axis = ptr_axis
};

/* ---- keyboard: translate evdev keycodes through the compositor's xkb keymap so the user's real
   layout is honored. KEYDOWN/KEYUP carry (a = keysym, b = modmask). Text is derived from the keysym
   in Nori (layout-correct); named keys (Left, BackSpace, …) are matched by keysym constant. ---- */
static int xkb_modmask(NoriWl* w) {
    if (!w->xkb_st) return 0;
    int m = 0;
    if (xkb_state_mod_name_is_active(w->xkb_st, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) m |= 1;
    if (xkb_state_mod_name_is_active(w->xkb_st, XKB_MOD_NAME_CTRL,  XKB_STATE_MODS_EFFECTIVE) > 0) m |= 2;
    if (xkb_state_mod_name_is_active(w->xkb_st, XKB_MOD_NAME_ALT,   XKB_STATE_MODS_EFFECTIVE) > 0) m |= 4;
    if (xkb_state_mod_name_is_active(w->xkb_st, XKB_MOD_NAME_LOGO,  XKB_STATE_MODS_EFFECTIVE) > 0) m |= 8;
    return m;
}
static void kbd_keymap(void* d, struct wl_keyboard* k, uint32_t fmt, int32_t fd, uint32_t sz) {
    (void)k; NoriWl* w = (NoriWl*)d;
    if (fmt != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) { if (fd >= 0) close(fd); return; }
    char* map = mmap(0, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) { close(fd); return; }
    if (!w->xkb_ctx) w->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    struct xkb_keymap* km = xkb_keymap_new_from_string(w->xkb_ctx, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    munmap(map, sz); close(fd);
    if (!km) return;
    struct xkb_state* st = xkb_state_new(km);
    if (!st) { xkb_keymap_unref(km); return; }
    if (w->xkb_st) xkb_state_unref(w->xkb_st);
    if (w->xkb_map) xkb_keymap_unref(w->xkb_map);
    w->xkb_map = km; w->xkb_st = st;
}
static long mono_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }
static void kbd_enter(void* d, struct wl_keyboard* k, uint32_t s, struct wl_surface* sf, struct wl_array* keys) { (void)k;(void)sf;(void)keys; ((NoriWl*)d)->serial = s; }
static void kbd_leave(void* d, struct wl_keyboard* k, uint32_t s, struct wl_surface* sf) { (void)k;(void)s;(void)sf; ((NoriWl*)d)->rep_active = 0; }   /* focus lost -> stop repeating */
static void kbd_key(void* d, struct wl_keyboard* k, uint32_t s, uint32_t t, uint32_t key, uint32_t state) {
    (void)k;(void)t; NoriWl* w = (NoriWl*)d; w->serial = s;
    uint32_t sym = 0;
    if (w->xkb_st) sym = xkb_state_key_get_one_sym(w->xkb_st, key + 8);   /* xkb keycode = evdev + 8 */
    else sym = key + 8;                                                   /* no keymap yet: fall back to raw */
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        push_ev(w, EV_KEYDOWN, (int)sym, w->mods);
        if (w->rep_rate_ms > 0 && w->xkb_map && xkb_keymap_key_repeats(w->xkb_map, key + 8)) {   /* this key auto-repeats */
            w->rep_active = 1; w->rep_sym = sym; w->rep_key = key + 8; w->rep_next_ms = mono_ms() + w->rep_delay_ms;
        } else { w->rep_active = 0; }                                     /* a non-repeating key cancels any repeat */
    } else {
        push_ev(w, EV_KEYUP, (int)sym, w->mods);
        if (w->rep_active && w->rep_key == key + 8) w->rep_active = 0;    /* the held key was released */
    }
}
static void kbd_modifiers(void* d, struct wl_keyboard* k, uint32_t s, uint32_t md, uint32_t ml, uint32_t lk, uint32_t g) {
    (void)k;(void)s; NoriWl* w = (NoriWl*)d;
    if (w->xkb_st) { xkb_state_update_mask(w->xkb_st, md, ml, lk, 0, 0, g); w->mods = xkb_modmask(w); }
}
static void kbd_repeat_info(void* d, struct wl_keyboard* k, int32_t rate, int32_t delay) {
    (void)k; NoriWl* w = (NoriWl*)d;
    w->rep_delay_ms = delay;
    w->rep_rate_ms = (rate > 0) ? (1000 / rate) : 0;   /* rate 0 => repeat disabled by the user's config */
}
static const struct wl_keyboard_listener kbd_l = {
    .keymap = kbd_keymap, .enter = kbd_enter, .leave = kbd_leave, .key = kbd_key, .modifiers = kbd_modifiers, .repeat_info = kbd_repeat_info
};

/* ---- seat: grab pointer + keyboard when advertised ---- */
static void seat_caps(void* d, struct wl_seat* seat, uint32_t caps) {
    NoriWl* w = (NoriWl*)d;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !w->ptr) { w->ptr = wl_seat_get_pointer(seat); wl_pointer_add_listener(w->ptr, &ptr_l, w); }
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !w->kbd) { w->kbd = wl_seat_get_keyboard(seat); wl_keyboard_add_listener(w->kbd, &kbd_l, w); }
}
static void seat_name(void* d, struct wl_seat* seat, const char* name) { (void)d;(void)seat;(void)name; }
static const struct wl_seat_listener seat_l = { seat_caps, seat_name };

/* receive `mime` from `offer` into a malloc'd NUL-terminated string (caller frees). Shared by the
   clipboard (paste) and drag-and-drop (drop) paths. Bounded so a stuck source can't hang us. */
static char* offer_read(NoriWl* w, struct wl_data_offer* offer, const char* mime) {
    char* out = (char*)malloc(1); out[0] = 0;
    if (!offer || !mime) return out;
    int fds[2]; if (pipe(fds) != 0) return out;
    wl_data_offer_receive(offer, mime, fds[1]);
    wl_display_flush(w->dpy);
    close(fds[1]);
    size_t cap = 4096, len = 0; char* buf = (char*)malloc(cap);
    struct pollfd p[2]; p[0].fd = wl_display_get_fd(w->dpy); p[0].events = POLLIN; p[1].fd = fds[0]; p[1].events = POLLIN;
    int done = 0, idle = 0, budget = 200;
    while (!done && budget-- > 0) {
        wl_display_flush(w->dpy);
        p[0].revents = 0; p[1].revents = 0;
        int r = poll(p, 2, 50);
        if (r <= 0) { if (++idle > 3) break; continue; }
        idle = 0;
        if (p[0].revents & POLLIN) wl_display_dispatch(w->dpy);
        if (p[1].revents & POLLIN) {
            if (len + 4096 > cap) { cap *= 2; buf = (char*)realloc(buf, cap); }
            ssize_t n = read(fds[0], buf + len, cap - len);
            if (n > 0) len += (size_t)n; else done = 1;
        }
    }
    close(fds[0]); free(out);
    buf = (char*)realloc(buf, len + 1); buf[len] = 0;
    return buf;
}

/* ---- clipboard + drag-and-drop (wl_data_device) ---- */
/* offer.offer(mime): record which mime types the newest offer advertises (for DnD target matching) */
static void doffer_offer(void* d, struct wl_data_offer* o, const char* mime) {
    NoriWl* w = (NoriWl*)d;
    if (o == w->pending_offer) {
        if (!strcmp(mime, "text/uri-list")) w->pending_uri = 1;
        else if (!strncmp(mime, "text/plain", 10)) w->pending_text = 1;
    }
}
static void doffer_source_actions(void* d, struct wl_data_offer* o, uint32_t a) { (void)d;(void)o;(void)a; }
static void doffer_action(void* d, struct wl_data_offer* o, uint32_t a) { (void)d;(void)o;(void)a; }
static const struct wl_data_offer_listener doffer_l = { doffer_offer, doffer_source_actions, doffer_action };

static void dsrc_send(void* d, struct wl_data_source* s, const char* mime, int32_t fd) {
    (void)s;(void)mime; NoriWl* w = (NoriWl*)d;
    if (w->clip_text) { size_t n = strlen(w->clip_text), off = 0; while (off < n) { ssize_t k = write(fd, w->clip_text + off, n - off); if (k <= 0) break; off += (size_t)k; } }
    close(fd);
}
static void dsrc_cancelled(void* d, struct wl_data_source* s) { NoriWl* w = (NoriWl*)d; if (w->clip_src == s) { wl_data_source_destroy(s); w->clip_src = 0; } }
static void dsrc_noop3(void* d, struct wl_data_source* s) { (void)d;(void)s; }
static void dsrc_target(void* d, struct wl_data_source* s, const char* m) { (void)d;(void)s;(void)m; }
static void dsrc_action(void* d, struct wl_data_source* s, uint32_t a) { (void)d;(void)s;(void)a; }
static const struct wl_data_source_listener dsrc_l = { dsrc_target, dsrc_send, dsrc_cancelled, dsrc_noop3, dsrc_noop3, dsrc_action };

/* ---- drag source (start a DnD drag carrying text, e.g. a tab id, across OS windows) ---- */
static void dragsrc_send(void* d, struct wl_data_source* s, const char* mime, int32_t fd) {
    (void)s;(void)mime; NoriWl* w = (NoriWl*)d;
    if (w->drag_src_text) { size_t n = strlen(w->drag_src_text), off = 0; while (off < n) { ssize_t k = write(fd, w->drag_src_text + off, n - off); if (k <= 0) break; off += (size_t)k; } }
    close(fd);
}
static void dragsrc_cancelled(void* d, struct wl_data_source* s) { NoriWl* w = (NoriWl*)d; if (w->drag_src == s) { wl_data_source_destroy(s); w->drag_src = 0; } }
static void dragsrc_finished(void* d, struct wl_data_source* s) { NoriWl* w = (NoriWl*)d; w->drag_done = 1; if (w->drag_src == s) { wl_data_source_destroy(s); w->drag_src = 0; } }
static void dragsrc_noop3(void* d, struct wl_data_source* s) { (void)d;(void)s; }
static void dragsrc_target(void* d, struct wl_data_source* s, const char* m) { (void)d;(void)s;(void)m; }
static void dragsrc_action(void* d, struct wl_data_source* s, uint32_t a) { (void)d;(void)s;(void)a; }
/* target, send, cancelled, dnd_drop_performed, dnd_finished, action */
static const struct wl_data_source_listener dragsrc_l = { dragsrc_target, dragsrc_send, dragsrc_cancelled, dragsrc_noop3, dragsrc_finished, dragsrc_action };

static void ddev_data_offer(void* d, struct wl_data_device* dd, struct wl_data_offer* o) {
    (void)dd; NoriWl* w = (NoriWl*)d; w->pending_offer = o; w->pending_uri = 0; w->pending_text = 0;
    wl_data_offer_add_listener(o, &doffer_l, d);
}
static void ddev_selection(void* d, struct wl_data_device* dd, struct wl_data_offer* o) {
    (void)dd; NoriWl* w = (NoriWl*)d; if (w->sel_offer && w->sel_offer != o) wl_data_offer_destroy(w->sel_offer); w->sel_offer = o;
}
/* a drag entered our surface: pick a mime we understand (files as text/uri-list, else text), accept it */
static void ddev_enter(void* d, struct wl_data_device* dd, uint32_t s, struct wl_surface* sf, wl_fixed_t x, wl_fixed_t y, struct wl_data_offer* o) {
    (void)dd;(void)sf; NoriWl* w = (NoriWl*)d;
    w->drag_offer = o; w->px = wl_fixed_to_int(x); w->py = wl_fixed_to_int(y);
    const char* mime = 0;
    if (o == w->pending_offer) { if (w->pending_uri) mime = "text/uri-list"; else if (w->pending_text) mime = "text/plain;charset=utf-8"; }
    w->drag_mime = mime;
    if (o) {
        wl_data_offer_set_actions(o, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE | WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE);
        wl_data_offer_accept(o, s, mime);   /* mime==NULL rejects (we don't understand the payload) */
    }
}
static void ddev_leave(void* d, struct wl_data_device* dd) {
    (void)dd; NoriWl* w = (NoriWl*)d; if (w->drag_offer) { wl_data_offer_destroy(w->drag_offer); w->drag_offer = 0; } w->drag_mime = 0;
}
static void ddev_motion(void* d, struct wl_data_device* dd, uint32_t t, wl_fixed_t x, wl_fixed_t y) {
    (void)dd;(void)t; NoriWl* w = (NoriWl*)d; w->px = wl_fixed_to_int(x); w->py = wl_fixed_to_int(y);
}
/* The drop event fires inside a dispatch; reading the offer needs its own dispatch loop, and
   libwayland's dispatch is not reentrant (nesting it corrupts state and crashes, and blocks
   pings, so the app is reported as "not responding"). So just flag the drop here and do the read at top level (see nori_sfc_poll_event). */
static void ddev_drop(void* d, struct wl_data_device* dd) {
    (void)dd; NoriWl* w = (NoriWl*)d;
    if (w->drag_offer && w->drag_mime) {
        /* hand the offer off to the drop slot: a trailing leave() (KWin sends one after drop) must
           not clear it before drain_drop reads it at top level. */
        w->drop_offer = w->drag_offer; w->drop_mime = w->drag_mime;
        w->drag_offer = 0; w->drag_mime = 0;
        w->drop_pending = 1;
    } else if (w->drag_offer) { wl_data_offer_destroy(w->drag_offer); w->drag_offer = 0; }   /* rejected drop */
}
/* perform a pending drop's read + finish outside any dispatch (called from nori_sfc_poll_event) */
static void drain_drop(NoriWl* w) {
    if (!w->drop_pending) return;
    w->drop_pending = 0;
    if (w->drop_offer && w->drop_mime) {
        char* txt = offer_read(w, w->drop_offer, w->drop_mime);
        wl_data_offer_finish(w->drop_offer);
        free(w->drop_text); w->drop_text = txt;
        push_ev(w, EV_DROP, 0, 0);
    }
    if (w->drop_offer) { wl_data_offer_destroy(w->drop_offer); w->drop_offer = 0; }
    w->drop_mime = 0;
}
static const struct wl_data_device_listener ddev_l = { ddev_data_offer, ddev_enter, ddev_leave, ddev_motion, ddev_drop, ddev_selection };

/* ---- registry ---- */
static void reg_global(void* data, struct wl_registry* r, uint32_t name, const char* iface, uint32_t ver) {
    (void)ver; NoriWl* w = (NoriWl*)data;
    if (!strcmp(iface, "wl_compositor")) w->comp = (struct wl_compositor*)wl_registry_bind(r, name, &wl_compositor_interface, 4);
    else if (!strcmp(iface, "xdg_wm_base")) { w->wm = (struct xdg_wm_base*)wl_registry_bind(r, name, &xdg_wm_base_interface, 1); xdg_wm_base_add_listener(w->wm, &wm_l, w); }
    else if (!strcmp(iface, "wl_seat")) { uint32_t sv = ver < 4 ? ver : 4; w->seat = (struct wl_seat*)wl_registry_bind(r, name, &wl_seat_interface, sv); wl_seat_add_listener(w->seat, &seat_l, w); }   /* v4 => wl_keyboard.repeat_info */
    else if (!strcmp(iface, "wl_shm")) w->shm = (struct wl_shm*)wl_registry_bind(r, name, &wl_shm_interface, 1);
    else if (!strcmp(iface, "wp_viewporter")) w->vpr = (struct wp_viewporter*)wl_registry_bind(r, name, &wp_viewporter_interface, 1);
    else if (!strcmp(iface, "wp_fractional_scale_manager_v1")) w->fsmgr = (struct wp_fractional_scale_manager_v1*)wl_registry_bind(r, name, &wp_fractional_scale_manager_v1_interface, 1);
    else if (!strcmp(iface, "zxdg_decoration_manager_v1")) w->decomgr = (struct zxdg_decoration_manager_v1*)wl_registry_bind(r, name, &zxdg_decoration_manager_v1_interface, 1);
    else if (!strcmp(iface, "wl_data_device_manager")) w->ddm = (struct wl_data_device_manager*)wl_registry_bind(r, name, &wl_data_device_manager_interface, 3);
    else if (!strcmp(iface, "zwp_pointer_constraints_v1")) w->pcon = (struct zwp_pointer_constraints_v1*)wl_registry_bind(r, name, &zwp_pointer_constraints_v1_interface, 1);
    else if (!strcmp(iface, "wp_cursor_shape_manager_v1")) w->shmgr = (struct wp_cursor_shape_manager_v1*)wl_registry_bind(r, name, &wp_cursor_shape_manager_v1_interface, 1);
    else if (!strcmp(iface, "zwp_relative_pointer_manager_v1")) w->rpmgr = (struct zwp_relative_pointer_manager_v1*)wl_registry_bind(r, name, &zwp_relative_pointer_manager_v1_interface, 1);
}
static void reg_remove(void* d, struct wl_registry* r, uint32_t name) { (void)d;(void)r;(void)name; }
static const struct wl_registry_listener reg_l = { reg_global, reg_remove };

/* ---- xdg surface / toplevel ---- */
static void xsurf_configure(void* d, struct xdg_surface* s, uint32_t serial) { (void)d; xdg_surface_ack_configure(s, serial); }
static const struct xdg_surface_listener xsurf_l = { xsurf_configure };

static void top_configure(void* data, struct xdg_toplevel* t, int32_t w, int32_t h, struct wl_array* states) {
    (void)t;(void)states; NoriWl* nw = (NoriWl*)data;
    if (w > 0 && h > 0 && (w != nw->w || h != nw->h)) { nw->w = w; nw->h = h; push_ev(nw, EV_RESIZED, w, h); }
}
static void top_close(void* data, struct xdg_toplevel* t) { (void)t; NoriWl* nw = (NoriWl*)data; nw->closed = 1; push_ev(nw, EV_CLOSED, 0, 0); }
static const struct xdg_toplevel_listener top_l = { top_configure, top_close };

/* fractional scale: the compositor tells us the preferred scale (in 120ths, e.g. 180 = 1.5x). Render
   a buffer at physical size (logical * scale/120) and the viewport maps it back to logical -> crisp. */
static void fscale_preferred(void* data, struct wp_fractional_scale_v1* fs, uint32_t scale) {
    (void)fs; NoriWl* w = (NoriWl*)data; if (scale > 0) w->scale120 = (int)scale;
}
static const struct wp_fractional_scale_v1_listener fscale_l = { fscale_preferred };

void* nori_sfc_open(const char* title, int w, int h) {
    /* A broken pipe must not kill the process.
     *
     * Serving the clipboard means writing the selection into a pipe the compositor hands us
     * (dsrc_send / dragsrc_send). Whoever is reading may close it at any point: a clipboard
     * manager probing what is on offer takes a few bytes and goes away, and a paste target that
     * only wants a prefix does the same. The write then raises SIGPIPE, whose default action is to
     * terminate the process, with no error and no core, so it looks as if the copy itself crashed it.
     *
     * Ignored, so write() returns -1/EPIPE instead. Both senders already stop on a non-positive
     * result.
     *
     * Set here rather than in main() because it is this file's requirement: a program that never
     * opens a window has no selection to serve and no business having its signals changed. */
    signal(SIGPIPE, SIG_IGN);
    NoriWl* nw = (NoriWl*)calloc(1, sizeof(NoriWl));
    nw->w = w; nw->h = h; nw->px = -1; nw->py = -1;
    nw->rep_delay_ms = 400; nw->rep_rate_ms = 33;   /* sensible defaults until repeat_info arrives */
    nw->dpy = wl_display_connect(NULL);
    if (!nw->dpy) { free(nw); return 0; }
    struct wl_registry* reg = wl_display_get_registry(nw->dpy);
    wl_registry_add_listener(reg, &reg_l, nw);
    wl_display_roundtrip(nw->dpy);                 /* bind compositor/wm/seat */
    if (!nw->comp || !nw->wm) { wl_display_disconnect(nw->dpy); free(nw); return 0; }
    wl_display_roundtrip(nw->dpy);                 /* seat capabilities -> pointer/keyboard */
    if (nw->ddm && nw->seat) { nw->ddev = wl_data_device_manager_get_data_device(nw->ddm, nw->seat); wl_data_device_add_listener(nw->ddev, &ddev_l, nw); }
    nw->surf = wl_compositor_create_surface(nw->comp);
    nw->xsurf = xdg_wm_base_get_xdg_surface(nw->wm, nw->surf);
    xdg_surface_add_listener(nw->xsurf, &xsurf_l, nw);
    nw->top = xdg_surface_get_toplevel(nw->xsurf);
    xdg_toplevel_add_listener(nw->top, &top_l, nw);
    xdg_toplevel_set_title(nw->top, title);
    nw->scale120 = 120;
    if (nw->vpr) nw->vp = wp_viewporter_get_viewport(nw->vpr, nw->surf);           /* physical buffer -> logical size */
    if (nw->fsmgr) { nw->fscale = wp_fractional_scale_manager_v1_get_fractional_scale(nw->fsmgr, nw->surf); wp_fractional_scale_v1_add_listener(nw->fscale, &fscale_l, nw); }
    if (nw->decomgr) { nw->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(nw->decomgr, nw->top);  /* ask the compositor to draw the titlebar/border */
        zxdg_toplevel_decoration_v1_set_mode(nw->deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE); }
    wl_surface_commit(nw->surf);
    wl_display_roundtrip(nw->dpy);                 /* initial configure (+ preferred scale) */
    return nw;
}

/* read + dispatch pending events without blocking, filling the queue via listeners */
static void pump(NoriWl* w) {
    struct wl_display* d = w->dpy;
    while (wl_display_prepare_read(d) != 0) wl_display_dispatch_pending(d);
    wl_display_flush(d);
    struct pollfd pfd; pfd.fd = wl_display_get_fd(d); pfd.events = POLLIN; pfd.revents = 0;
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) wl_display_read_events(d);
    else wl_display_cancel_read(d);
    wl_display_dispatch_pending(d);
}

/* dequeue one event into out[0..2] = (a, b, c); returns the type (0 = none). Pumps when the queue
   drains. `out` must have room for three ints: the mouse events fill (button, x, y). */
int nori_sfc_poll_event(void* h, int* out) {
    NoriWl* w = (NoriWl*)h;
    out[0] = 0; out[1] = 0; out[2] = 0;
    if (w->qhead == w->qtail) pump(w);
    if (w->qhead == w->qtail && w->rep_active && w->rep_rate_ms > 0) {   /* synthesize held-key repeats */
        long now = mono_ms();
        if (now >= w->rep_next_ms) { push_ev(w, EV_KEYDOWN, (int)w->rep_sym, w->mods); w->rep_next_ms = now + w->rep_rate_ms; }
    }
    drain_drop(w);                       /* read a just-arrived drop at top level (never inside dispatch) */
    if (w->qhead == w->qtail) return 0;
    WlEv e = w->q[w->qhead]; w->qhead = (w->qhead + 1) % QN;
    out[0] = e.a; out[1] = e.b; out[2] = e.c; return e.type;
}
/* A press no polled read has seen yet is latched, as the Android backend latches a tap: the
   first read after it reports the button down even though it is physically up again, and clears the
   latch, so a click shorter than a frame is one press-release edge instead of nothing at all. A
   button still held reports down from the state itself, every frame. The latch collapses several
   clicks between two reads into one and cannot say which button it was; the click events
   (`next_click` in window.nori) keep all of that, and a caller that needs it reads those. */
static int wl_take_down(NoriWl* w) {
    if (w->btn) { w->tap_latch = 0; return 1; }
    if (w->tap_latch) { w->tap_latch = 0; return 1; }
    return 0;
}
/* While a completed click is still unobserved the position is the press position, not wherever the
   pointer drifted to afterwards: a click is where it landed. */
static int wl_px(NoriWl* w) { return w->tap_latch ? w->lx : w->px; }
static int wl_py(NoriWl* w) { return w->tap_latch ? w->ly : w->py; }

/* current pointer position into out_xy[0..1]; returns 1 if button 1 is held (or was clicked) */
int nori_sfc_mouse(void* h, int* out_xy) { NoriWl* w = (NoriWl*)h; out_xy[0] = wl_px(w); out_xy[1] = wl_py(w); return wl_take_down(w); }
/* signed-int accessors so Nori callers need no out-buffer / peek32 / sign-fix (px/py are already signed) */
int nori_sfc_mouse_x(void* h) { return wl_px((NoriWl*)h); }
int nori_sfc_mouse_y(void* h) { return wl_py((NoriWl*)h); }
int nori_sfc_mouse_down(void* h) { return wl_take_down((NoriWl*)h); }
/* On a desktop the button state IS the held state: a mouse reports press and release,
   so nothing has to be latched to notice a drag. */
int nori_sfc_pointer_held(void* h) { return ((NoriWl*)h)->btn; }
int nori_sfc_mouse_middle(void* h) { return ((NoriWl*)h)->mid; }

/* There is no on-screen keyboard to raise here: the seam exists so a program can ask for one without
   knowing which platform it is on, and asking costs nothing where a real keyboard is attached. */
void nori_sfc_soft_keyboard(void* h, int show) { (void)h; (void)show; }
/* No system back gesture here; the Escape key is already an ordinary key event. */
int nori_sfc_catch_back(void* h, int on) { (void)h; (void)on; return 0; }

/* A desktop window is given a rectangle it owns entirely; nothing is drawn over its top edge. */
int nori_sfc_inset_top(void* h) { (void)h; return 0; }
/* A wheel notch, the only scroll a desktop reports, is worth the conventional four logical pixels. */
int nori_sfc_scroll_unit_1000(void* h) { (void)h; return 4000; }
/* A mouse pointer rests on things without pressing them, which is what a hover style is for. */
int nori_sfc_has_hover(void* h) { (void)h; return 1; }

/* ---- wl_shm CPU-framebuffer path (double-buffered) + frame-callback pacing ---- */
static void buf_release(void* data, struct wl_buffer* b) {
    NoriWl* w = (NoriWl*)data;
    if (w->bufs[0].buf == b) w->bufs[0].busy = 0;
    else if (w->bufs[1].buf == b) w->bufs[1].busy = 0;
}
static const struct wl_buffer_listener buf_l = { buf_release };

static void frame_done(void* data, struct wl_callback* cb, uint32_t t) {
    (void)t; NoriWl* w = (NoriWl*)data;
    if (w->frame_cb == cb) w->frame_cb = 0;
    wl_callback_destroy(cb);
    w->frame_pending = 0;
}
static const struct wl_callback_listener frame_l = { frame_done };

static void shm_destroy(NoriWl* w) {
    for (int i = 0; i < 2; i++) { if (w->bufs[i].buf) { wl_buffer_destroy(w->bufs[i].buf); } w->bufs[i].buf = 0; w->bufs[i].mem = 0; w->bufs[i].busy = 0; }
    if (w->pool) { wl_shm_pool_destroy(w->pool); w->pool = 0; }
    if (w->shm_map) { munmap(w->shm_map, w->shm_size); w->shm_map = 0; }
    if (w->shm_fd > 0) { close(w->shm_fd); w->shm_fd = 0; }
}
static int shm_make(NoriWl* w, int ww, int hh) {
    if (!w->shm) return 0;
    size_t stride = (size_t)ww * 4, bsz = stride * hh, total = bsz * 2;   /* two buffers back-to-back */
    int fd = memfd_create("nori-wl-shm", 0);
    if (fd < 0) return 0;
    if (ftruncate(fd, total) < 0) { close(fd); return 0; }
    void* map = mmap(0, total, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) { close(fd); return 0; }
    w->pool = wl_shm_create_pool(w->shm, fd, total);
    w->shm_fd = fd; w->shm_map = map; w->shm_size = total; w->shm_w = ww; w->shm_h = hh;
    for (int i = 0; i < 2; i++) {
        w->bufs[i].buf = wl_shm_pool_create_buffer(w->pool, (int32_t)(i * bsz), ww, hh, (int32_t)stride, WL_SHM_FORMAT_XRGB8888);
        w->bufs[i].mem = (char*)map + i * bsz;
        w->bufs[i].busy = 0;
        wl_buffer_add_listener(w->bufs[i].buf, &buf_l, w);
    }
    return 1;
}
/* present a CPU framebuffer (`pixels` = ww*hh 0x00RRGGBB, matching std/nori_ui_cpu's canvas, which is
   byte-identical to WL_SHM_FORMAT_XRGB8888) and request a frame callback for pacing. */
void nori_sfc_shm_present(void* h, void* pixels, int ww, int hh) {
    NoriWl* w = (NoriWl*)h;
    if (!w->shm || ww <= 0 || hh <= 0) return;
    if (!w->pool || ww != w->shm_w || hh != w->shm_h) { shm_destroy(w); if (!shm_make(w, ww, hh)) return; }
    int idx = -1;
    for (int i = 0; i < 2; i++) if (!w->bufs[i].busy) { idx = i; break; }
    if (idx < 0) { wl_display_dispatch(w->dpy); for (int i = 0; i < 2; i++) if (!w->bufs[i].busy) { idx = i; break; } }
    if (idx < 0) idx = 0;
    memcpy(w->bufs[idx].mem, pixels, (size_t)ww * hh * 4);
    wl_surface_attach(w->surf, w->bufs[idx].buf, 0, 0);
    if (w->vp) wp_viewport_set_destination(w->vp, w->w, w->h);   /* map the physical buffer to logical size */
    wl_surface_damage_buffer(w->surf, 0, 0, ww, hh);
    if (w->frame_cb) wl_callback_destroy(w->frame_cb);
    w->frame_cb = wl_surface_frame(w->surf);
    wl_callback_add_listener(w->frame_cb, &frame_l, w);
    w->frame_pending = 1;
    wl_surface_commit(w->surf);
    wl_display_flush(w->dpy);
    w->bufs[idx].busy = 1;
}
/* block until the compositor is ready for the next frame (the frame callback fires) or `timeout_ms`
   elapses — vsync-style pacing that replaces a fixed sleep. */
void nori_sfc_frame_wait(void* h, int timeout_ms) {
    NoriWl* w = (NoriWl*)h;
    struct pollfd pfd; pfd.fd = wl_display_get_fd(w->dpy); pfd.events = POLLIN;
    if (!w->frame_pending) {
        /* idle (nothing presented this frame): block up to timeout for input instead of busy-spinning
           the caller's event loop — wakes immediately on any input, so latency stays low. */
        wl_display_flush(w->dpy);
        pfd.revents = 0;
        if (poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLIN)) wl_display_dispatch(w->dpy);
        return;
    }
    int budget = timeout_ms / 4 + 1;   /* bounded iterations: a busy display fd must not spin forever */
    while (w->frame_pending && budget-- > 0) {
        wl_display_flush(w->dpy);
        pfd.revents = 0;
        int r = poll(&pfd, 1, 4);
        if (r > 0 && (pfd.revents & POLLIN)) wl_display_dispatch(w->dpy);
    }
}

int nori_sfc_closed(void* h) { return ((NoriWl*)h)->closed; }
/* A compositor never takes the surface away underneath a running program the way Android does, so
   "can I draw" is simply "am I still open". The seam exists for the platform where it is not. */
int nori_sfc_drawable(void* h) { return h && !((NoriWl*)h)->closed; }
int nori_sfc_w(void* h) { return ((NoriWl*)h)->w; }
int nori_sfc_h(void* h) { return ((NoriWl*)h)->h; }
/* preferred fractional scale in 120ths (120 = 1.0, 180 = 1.5). Render at logical*scale/120 for crispness. */
int nori_sfc_scale120(void* h) { return ((NoriWl*)h)->scale120; }
/* map a physical buffer (logical_w*scale x logical_h*scale) to the current logical size (GPU path) */
void nori_sfc_apply_viewport(void* h) { NoriWl* w = (NoriWl*)h; if (w->vp) { wp_viewport_set_destination(w->vp, w->w, w->h); wl_surface_commit(w->surf); } }
void* nori_sfc_display(void* h) { return ((NoriWl*)h)->dpy; }
void* nori_sfc_surface(void* h) { return ((NoriWl*)h)->surf; }
/* start a DnD drag from this window carrying `text` as text/plain (e.g. "nori-tab:Editor"), so it can
   be dropped on another window's drop target. Needs a recent input serial (a title-bar press). */
void nori_sfc_start_drag(void* h, const char* text) {
    NoriWl* w = (NoriWl*)h;
    if (!w->ddm || !w->ddev || !w->serial) return;
    free(w->drag_src_text); w->drag_src_text = strdup(text ? text : "");
    w->drag_done = 0;
    w->drag_src = wl_data_device_manager_create_data_source(w->ddm);
    wl_data_source_add_listener(w->drag_src, &dragsrc_l, w);
    wl_data_source_offer(w->drag_src, "text/plain;charset=utf-8");
    wl_data_source_offer(w->drag_src, "text/plain");
    wl_data_source_set_actions(w->drag_src, WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE | WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
    wl_data_device_start_drag(w->ddev, w->drag_src, w->surf, 0, w->serial);
    wl_display_flush(w->dpy);
}
/* 1 if the last drag from this window completed with a drop elsewhere (the move succeeded) */
int nori_sfc_drag_finished(void* h) { return ((NoriWl*)h)->drag_done; }

/* set the clipboard (primary selection) to `text` (UTF-8); we serve it via a data source */
void nori_sfc_clipboard_set(void* h, const char* text) {
    NoriWl* w = (NoriWl*)h;
    if (!w->ddm || !w->ddev) return;
    free(w->clip_text); w->clip_text = strdup(text ? text : "");
    w->clip_src = wl_data_device_manager_create_data_source(w->ddm);
    wl_data_source_add_listener(w->clip_src, &dsrc_l, w);
    wl_data_source_offer(w->clip_src, "text/plain;charset=utf-8");
    wl_data_source_offer(w->clip_src, "text/plain");
    wl_data_device_set_selection(w->ddev, w->clip_src, w->serial);
    wl_display_flush(w->dpy);
}
/* get the clipboard text (UTF-8) as a malloc'd NUL-terminated string (caller frees), "" if none */
char* nori_sfc_clipboard_get(void* h) {
    NoriWl* w = (NoriWl*)h;
    if (!w->sel_offer) { char* out = (char*)malloc(1); out[0] = 0; return out; }
    return offer_read(w, w->sel_offer, "text/plain;charset=utf-8");
}
/* the text/URIs of the last drop (delivered with an EV_DROP event), transferring ownership (caller
   frees); "" if none. For a file drop this is a text/uri-list (newline-separated file:// URIs). */
char* nori_sfc_drop_text(void* h) {
    NoriWl* w = (NoriWl*)h;
    char* t = w->drop_text; w->drop_text = 0;
    if (!t) { t = (char*)malloc(1); t[0] = 0; }
    return t;
}

/* ---- pointer lock ------------------------------------------------------------------------------
 *
 * relative-pointer delivers motion in two flavours: `dx/dy` (accelerated, what a cursor would have
 * done) and `dx_unaccel/dy_unaccel` (raw device units). A camera wants the accelerated one — it is
 * what every other application on the desktop reacts to, so pointer speed and the user's
 * acceleration curve mean the same thing here as everywhere else. Raw is for tooling that must
 * bypass the desktop's settings, which a game camera is not.
 */
static void rel_motion(void* d, struct zwp_relative_pointer_v1* rp, uint32_t hi, uint32_t lo,
                       wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t udx, wl_fixed_t udy) {
    (void)rp; (void)hi; (void)lo; (void)udx; (void)udy;
    NoriWl* w = (NoriWl*)d;
    w->rel_dx += wl_fixed_to_double(dx);
    w->rel_dy += wl_fixed_to_double(dy);
}
static const struct zwp_relative_pointer_v1_listener rel_l = { rel_motion };

/* Lock the pointer to the window and hide it, or release it. Returns 1 if the request was made.
 *
 * ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT, not ONESHOT: a oneshot lock is destroyed by the
 * compositor the first time it is broken (an alt-tab, a workspace switch), and the camera would go
 * dead on the way back. Persistent re-locks when the surface is focused again, which is what a
 * first-person game wants and what the cursor_mode request means.
 *
 * The compositor may refuse (it is allowed to), and there is no error path to report. `locked`
 * records what the caller asked for so the state is readable either way; `nori_sfc_pointer_locked`
 * answers that, not a promise about the compositor. */
int nori_sfc_lock_pointer(void* h, int on) {
    NoriWl* w = (NoriWl*)h;
    if (!w || !w->ptr || !w->surf) return 0;
    if (on) {
        if (!w->pcon || !w->rpmgr) return 0;          /* compositor lacks the extensions */
        if (!w->lock) {
            w->lock = zwp_pointer_constraints_v1_lock_pointer(w->pcon, w->surf, w->ptr, NULL,
                        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
        }
        if (!w->relptr) {
            w->relptr = zwp_relative_pointer_manager_v1_get_relative_pointer(w->rpmgr, w->ptr);
            if (w->relptr) zwp_relative_pointer_v1_add_listener(w->relptr, &rel_l, w);
        }
        /* hide the cursor: a locked pointer that still draws one leaves it frozen mid-screen */
        wl_pointer_set_cursor(w->ptr, w->serial, NULL, 0, 0);
        w->locked = 1;
        w->rel_dx = 0; w->rel_dy = 0;
        return 1;
    }
    if (w->lock) { zwp_locked_pointer_v1_destroy(w->lock); w->lock = NULL; }
    if (w->relptr) { zwp_relative_pointer_v1_destroy(w->relptr); w->relptr = NULL; }
    w->locked = 0;
    w->shape_cur = -1;   /* the pointer was hidden; whatever shape we last asked for is gone */
    return 1;
}
int nori_sfc_pointer_locked(void* h) { NoriWl* w = (NoriWl*)h; return w && w->locked; }

/* Pointer shape. The compositor draws it, from a name, so the theme, the size and the HiDPI variant
 * are its concern. That is why this uses cursor-shape-v1 rather than libwayland-cursor and a
 * wl_surface we would have to paint and scale.
 *
 * Needs the enter serial: set_shape is only valid against the serial of the enter that put the
 * pointer on our surface, so `serial` (the latest of anything) will not do. If the pointer has never
 * entered, there is nothing to shape. */
int nori_sfc_set_cursor(void* h, int shape) {
    NoriWl* w = (NoriWl*)h;
    if (!w || !w->shmgr || !w->ptr || !w->ptr_serial) return 0;
    if (w->locked) return 0;                    /* hidden on purpose; do not put it back */
    if (shape < 0 || shape > 5) shape = 0;
    if (w->shape_cur == shape) return 1;
    if (!w->shdev) {
        w->shdev = wp_cursor_shape_manager_v1_get_pointer(w->shmgr, w->ptr);
        if (!w->shdev) return 0;
    }
    uint32_t named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    switch (shape) {
        case 1: named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT; break;
        case 2: named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER; break;
        case 3: named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE; break;
        case 4: named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE; break;
        case 5: named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING; break;
        default: named = WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT; break;
    }
    wp_cursor_shape_device_v1_set_shape(w->shdev, w->ptr_serial, named);
    wl_display_flush(w->dpy);
    w->shape_cur = shape;
    return 1;
}

/* Take the accumulated relative motion and clear it. Scaled by 256 and returned as ints, so the
 * dispatch layer stays integer-only like every other call on this surface; the Nori side divides. */
int nori_sfc_pointer_delta(void* h, int* out_dxdy) {
    NoriWl* w = (NoriWl*)h;
    if (!w) { out_dxdy[0] = 0; out_dxdy[1] = 0; return 0; }
    out_dxdy[0] = (int)(w->rel_dx * 256.0);
    out_dxdy[1] = (int)(w->rel_dy * 256.0);
    w->rel_dx = 0; w->rel_dy = 0;
    return w->locked;
}

void nori_sfc_close(void* h) { NoriWl* nw = (NoriWl*)h; free(nw->clip_text); free(nw->drop_text); free(nw->drag_src_text); shm_destroy(nw); if (nw->dpy) wl_display_disconnect(nw->dpy); free(nw); }

#include "xdg-shell-protocol.c"
#include "viewporter-protocol.c"
#include "fractional-scale-v1-protocol.c"
#include "xdg-decoration-unstable-v1-protocol.c"
#include "pointer-constraints-unstable-v1-protocol.c"
#include "relative-pointer-unstable-v1-protocol.c"
#include "cursor-shape-v1-protocol.c"
