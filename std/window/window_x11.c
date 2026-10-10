/* The X11 implementation of the window seam: the same 26 calls window_wayland.c answers, under the
 * nori_x11_ prefix so both can be linked into one binary and chosen at open() (window_dispatch.c).
 *
 * Event codes on the wire match the other backends, because the Nori side decodes them once
 * for all of them: 1 = closed, 2 = resized(w,h), 3 = key down(keysym,mods), 4 = key up, 5 = drop,
 * 6 = scroll(delta), 7 = mouse down(button, x, y), 8 = mouse up(button, x, y). Keysyms are X11
 * keysyms, which is what the Wayland backend resolves its xkb keymap into as well, so key_text above
 * this seam needs no per-backend branch.
 *
 * A button event ends the drain. If poll_event swallowed the whole X queue looking for something
 * to report, updating x->btn as it went, a press and its release in the same drain would net btn=0
 * and the click would never happen. So each press and release returns from the loop with its own
 * event, carrying the position X reported for it; whatever is left in the queue is read by the next
 * call, which is how a caller already drives this (poll until 0).
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define EVQ 64

typedef struct {
    Display* d;
    Window   win;
    Atom     wmdel, clipboard, utf8, targets, incr;
    int      w, h, closed;
    int      px, py, btn, mid;
    int      lx, ly;                 /* where button 1 was pressed (the latch's position) */
    int      tap_latch;              /* a press not yet observed by a polled read; see nori_x11_mouse_down */
    char*    clip_own;               /* text this window has put on the CLIPBOARD selection */
    /* Pointer lock. X11 has no lock, so this is the classic technique: grab the pointer, hide the
     * cursor, and warp it back to the centre after every read. The delta is then the distance it
     * moved away from the centre, and it can never reach an edge. Wayland forbids warping, which
     * is why that backend uses pointer-constraints instead (see window_wayland.c). */
    int      locked;
    int      warp_x, warp_y;         /* the centre we warp back to, in window coordinates */
    int      rel_dx, rel_dy;         /* accumulated since the last read */
    Cursor   blank;
    Cursor   shapes[6];              /* font cursors, created on first use and kept for the window's life */
    int      shape_cur;              /* the shape currently defined, so a per-frame request is free */
    int      pending_scroll;         /* wheel notches coalesced since the last poll */
} NoriX11;

/* Button4/Button5 are the wheel. X delivers a press and a release per notch; only the press counts,
 * or every scroll would be doubled. */
#define WHEEL_UP 4
#define WHEEL_DOWN 5

void* nori_x11_open(const char* title, int w, int h) {
    Display* d = XOpenDisplay(0);
    if (!d) return 0;
    int scr = DefaultScreen(d);
    Window root = DefaultRootWindow(d);
    Window win = XCreateSimpleWindow(d, root, 0, 0, w, h, 1, BlackPixel(d, scr), BlackPixel(d, scr));

    /* No background pixmap, and keep existing content on resize. Without this the server clears the
     * window on every resize step, which reads as a white flash under a tiling WM. */
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.background_pixmap = None;
    attrs.bit_gravity = NorthWestGravity;
    XChangeWindowAttributes(d, win, CWBackPixmap | CWBitGravity, &attrs);

    if (title) XStoreName(d, win, title);
    XSelectInput(d, win, KeyPressMask | KeyReleaseMask | StructureNotifyMask |
                         ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    NoriX11* x = (NoriX11*)calloc(1, sizeof(NoriX11));
    if (!x) { XDestroyWindow(d, win); XCloseDisplay(d); return 0; }
    x->d = d; x->win = win; x->w = w; x->h = h;
    x->px = -1; x->py = -1;
    x->wmdel    = XInternAtom(d, "WM_DELETE_WINDOW", False);
    x->clipboard= XInternAtom(d, "CLIPBOARD", False);
    x->utf8     = XInternAtom(d, "UTF8_STRING", False);
    x->targets  = XInternAtom(d, "TARGETS", False);
    x->incr     = XInternAtom(d, "INCR", False);
    XSetWMProtocols(d, win, &x->wmdel, 1);
    XMapWindow(d, win);
    XFlush(d);
    return x;
}

/* Another client is asking for the text we own on the CLIPBOARD. Answering this is what makes a
 * paste in some other application produce our text; an owner that never replies looks to
 * the rest of the desktop like an empty clipboard. */
static void x11_serve_selection(NoriX11* x, XSelectionRequestEvent* req) {
    XSelectionEvent resp;
    memset(&resp, 0, sizeof(resp));
    resp.type = SelectionNotify;
    resp.display = req->display;
    resp.requestor = req->requestor;
    resp.selection = req->selection;
    resp.target = req->target;
    resp.time = req->time;
    resp.property = None;

    if (x->clip_own) {
        if (req->target == x->targets) {
            Atom offer[2]; offer[0] = x->targets; offer[1] = x->utf8;
            XChangeProperty(x->d, req->requestor, req->property, XA_ATOM, 32,
                            PropModeReplace, (unsigned char*)offer, 2);
            resp.property = req->property;
        } else if (req->target == x->utf8 || req->target == XA_STRING) {
            XChangeProperty(x->d, req->requestor, req->property, req->target, 8,
                            PropModeReplace, (unsigned char*)x->clip_own,
                            (int)strlen(x->clip_own));
            resp.property = req->property;
        }
    }
    XSendEvent(x->d, req->requestor, False, 0, (XEvent*)&resp);
    XFlush(x->d);
}

/* 1 = left, 2 = middle, 3 = right: the same three ids every backend reports. */
static int x11_button_id(unsigned int b) { return b == Button1 ? 1 : (b == Button2 ? 2 : 3); }

/* `out` must have room for three ints: the mouse events fill (button, x, y). */
int nori_x11_poll_event(void* h, int* out) {
    NoriX11* x = (NoriX11*)h;
    if (!x) return 1;
    out[0] = 0; out[1] = 0; out[2] = 0;

    /* Wheel notches are coalesced: a fast flick delivers many presses between two polls, and
     * reporting them one per poll would lag the page behind the input. */
    if (x->pending_scroll) {
        out[0] = x->pending_scroll;
        x->pending_scroll = 0;
        return 6;
    }
    while (XPending(x->d)) {
        XEvent ev;
        XNextEvent(x->d, &ev);
        switch (ev.type) {
        case ClientMessage:
            if ((Atom)ev.xclient.data.l[0] == x->wmdel) { x->closed = 1; return 1; }
            break;
        case ConfigureNotify:
            if (ev.xconfigure.width != x->w || ev.xconfigure.height != x->h) {
                x->w = ev.xconfigure.width; x->h = ev.xconfigure.height;
                out[0] = x->w; out[1] = x->h;
                return 2;
            }
            break;
        case KeyPress:
        case KeyRelease: {
            KeySym ks = XLookupKeysym(&ev.xkey, (ev.xkey.state & ShiftMask) ? 1 : 0);
            int mods = 0;
            if (ev.xkey.state & ShiftMask)   mods |= 1;
            if (ev.xkey.state & ControlMask) mods |= 2;
            if (ev.xkey.state & Mod1Mask)    mods |= 4;
            if (ev.xkey.state & Mod4Mask)    mods |= 8;
            out[0] = (int)ks; out[1] = mods;
            return ev.type == KeyPress ? 3 : 4;
        }
        case ButtonPress:
            /* In logical pixels, like every other backend (see window.nori). A notch is three
             * lines of text, near enough; a touch screen has no notches and reports the distance
             * its finger actually travelled. */
            if (ev.xbutton.button == WHEEL_UP)        { x->pending_scroll -= 48; x->px = ev.xbutton.x; x->py = ev.xbutton.y; break; }
            else if (ev.xbutton.button == WHEEL_DOWN) { x->pending_scroll += 48; x->px = ev.xbutton.x; x->py = ev.xbutton.y; break; }
            x->px = ev.xbutton.x; x->py = ev.xbutton.y;
            if (ev.xbutton.button == Button1)      { x->btn = 1; x->lx = x->px; x->ly = x->py; x->tap_latch = 1; }
            else if (ev.xbutton.button == Button2) { x->mid = 1; }
            else if (ev.xbutton.button != Button3) break;              /* buttons 6/7 (tilt wheel) are not reported */
            out[0] = x11_button_id(ev.xbutton.button); out[1] = x->px; out[2] = x->py;
            return 7;
        case ButtonRelease:
            if (ev.xbutton.button == WHEEL_UP || ev.xbutton.button == WHEEL_DOWN) break;   /* X sends a release per notch; only the press counts */
            x->px = ev.xbutton.x; x->py = ev.xbutton.y;
            if (ev.xbutton.button == Button1)      x->btn = 0;
            else if (ev.xbutton.button == Button2) x->mid = 0;
            else if (ev.xbutton.button != Button3) break;
            out[0] = x11_button_id(ev.xbutton.button); out[1] = x->px; out[2] = x->py;
            return 8;
        case MotionNotify:
            x->px = ev.xmotion.x; x->py = ev.xmotion.y;
            break;
        case SelectionRequest:
            x11_serve_selection(x, &ev.xselectionrequest);
            break;
        case SelectionClear:
            free(x->clip_own); x->clip_own = 0;
            break;
        default: break;
        }
    }
    if (x->pending_scroll) { out[0] = x->pending_scroll; x->pending_scroll = 0; return 6; }
    return 0;
}

/* A click is shorter than a frame. A mouse button is down for something like 50ms and a real frame
   can take longer, so a caller that only reads the button state sees 0 before and 0 after, and
   the click never happens (as with `xdotool click`). So a press that no polled read has seen yet
   is latched, as on the Android backend: the first read after it reports the button down even
   though it is physically up again, and the read clears the latch, so the click is one
   press-release edge no matter how slowly the caller polls. A press still held reports down from
   the state itself, every frame.
   The latch collapses several clicks between two reads into one, and cannot say which button it
   was: that is what the click events (`next_click` in window.nori) are for, and a caller that
   wants every click reads those instead. */
static int x11_take_down(NoriX11* x) {
    if (x->btn) { x->tap_latch = 0; return 1; }
    if (x->tap_latch) { x->tap_latch = 0; return 1; }
    return 0;
}
/* While a completed click is still unobserved the position is the press position, not wherever the
   pointer drifted to afterwards: a click is where it landed. */
static int x11_px(NoriX11* x) { return x->tap_latch ? x->lx : x->px; }
static int x11_py(NoriX11* x) { return x->tap_latch ? x->ly : x->py; }

int nori_x11_mouse(void* h, int* out_xy) {
    NoriX11* x = (NoriX11*)h;
    if (!x) { out_xy[0] = -1; out_xy[1] = -1; return 0; }
    out_xy[0] = x11_px(x); out_xy[1] = x11_py(x);
    return x11_take_down(x);
}
int nori_x11_mouse_x(void* h)      { NoriX11* x = (NoriX11*)h; return x ? x11_px(x) : -1; }
int nori_x11_mouse_y(void* h)      { NoriX11* x = (NoriX11*)h; return x ? x11_py(x) : -1; }
int nori_x11_mouse_down(void* h)   { NoriX11* x = (NoriX11*)h; return x ? x11_take_down(x) : 0; }
/* On a desktop the button state IS the held state: a mouse reports press and release,
   so nothing has to be latched to notice a drag. */
int nori_x11_pointer_held(void* h)   { NoriX11* x = (NoriX11*)h; return x ? x->btn :  0; }
int nori_x11_mouse_middle(void* h) { NoriX11* x = (NoriX11*)h; return x ? x->mid :  0; }

/* 0x00RRGGBB pixels straight into the window. This is the same byte order a standard TrueColor
 * visual wants, so there is no per-pixel conversion — the buffer is handed to the server as-is. */
void nori_x11_shm_present(void* h, void* pixels, int ww, int hh) {
    NoriX11* x = (NoriX11*)h;
    if (!x || !pixels || ww <= 0 || hh <= 0) return;
    int scr = DefaultScreen(x->d);
    XImage* img = XCreateImage(x->d, DefaultVisual(x->d, scr), DefaultDepth(x->d, scr),
                               ZPixmap, 0, (char*)pixels, ww, hh, 32, 0);
    if (!img) return;
    XPutImage(x->d, x->win, DefaultGC(x->d, scr), img, 0, 0, 0, 0, ww, hh);
    img->data = 0;                 /* the framebuffer belongs to the caller; XDestroyImage must not free it */
    XDestroyImage(img);
    XFlush(x->d);
}

/* X has no frame callback, so there is nothing to wait for that would pace us to the display. The
 * honest equivalent is to flush what we have drawn and sleep the remainder of the caller's budget,
 * which keeps a render loop off a busy spin without pretending to be vsynced. */
void nori_x11_frame_wait(void* h, int timeout_ms) {
    NoriX11* x = (NoriX11*)h;
    if (!x) return;
    XSync(x->d, False);
    if (XPending(x->d) || timeout_ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = timeout_ms / 1000;
    ts.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
    nanosleep(&ts, 0);
}

/* Xft.dpi is where every desktop environment records the user's scaling choice; 96 is unscaled. */
int nori_x11_scale120(void* h) {
    NoriX11* x = (NoriX11*)h;
    if (!x) return 120;
    char* rms = XResourceManagerString(x->d);
    if (!rms) return 120;
    const char* p = strstr(rms, "Xft.dpi:");
    if (!p) return 120;
    p += 8;
    while (*p == ' ' || *p == '\t') p++;
    double dpi = atof(p);
    if (dpi <= 0.0) return 120;
    int s = (int)((dpi / 96.0) * 120.0 + 0.5);
    return s < 120 ? 120 : s;
}

int nori_x11_closed(void* h) { NoriX11* x = (NoriX11*)h; return x ? x->closed : 1; }
/* As with Wayland: an X server does not revoke a window while the program runs. */
int nori_x11_drawable(void* h) { NoriX11* x = (NoriX11*)h; return x ? !x->closed : 0; }
int nori_x11_w(void* h)      { NoriX11* x = (NoriX11*)h; return x ? x->w : 0; }
int nori_x11_h(void* h)      { NoriX11* x = (NoriX11*)h; return x ? x->h : 0; }

void* nori_x11_display(void* h) { NoriX11* x = (NoriX11*)h; return x ? (void*)x->d : 0; }
/* The X11 drawable, for a GPU surface built from display+window. Not a wl_surface: the caller asks
 * nori_win_backend which of the two it is holding. */
void* nori_x11_surface(void* h) { NoriX11* x = (NoriX11*)h; return x ? (void*)(unsigned long)x->win : 0; }

void nori_x11_clipboard_set(void* h, const char* text) {
    NoriX11* x = (NoriX11*)h;
    if (!x || !text) return;
    free(x->clip_own);
    x->clip_own = strdup(text);
    XSetSelectionOwner(x->d, x->clipboard, x->win, CurrentTime);
    XFlush(x->d);
}

/* Reading the selection is a round trip through its owner, so this blocks, briefly and with a
 * ceiling, because the owner may be gone or wedged and a paste must not hang the frame loop. */
char* nori_x11_clipboard_get(void* h) {
    NoriX11* x = (NoriX11*)h;
    if (!x) return 0;
    if (x->clip_own) return strdup(x->clip_own);      /* we own it; no round trip needed */
    if (XGetSelectionOwner(x->d, x->clipboard) == None) return 0;

    Atom prop = XInternAtom(x->d, "NORI_CLIP", False);
    XConvertSelection(x->d, x->clipboard, x->utf8, prop, x->win, CurrentTime);
    XFlush(x->d);

    for (int spin = 0; spin < 200; spin++) {          /* ~200ms ceiling */
        XEvent ev;
        if (XCheckTypedWindowEvent(x->d, x->win, SelectionNotify, &ev)) {
            if (ev.xselection.property == None) return 0;
            Atom type; int fmt; unsigned long n, rest; unsigned char* data = 0;
            if (XGetWindowProperty(x->d, x->win, prop, 0, (~0L), True, AnyPropertyType,
                                   &type, &fmt, &n, &rest, &data) != Success || !data) return 0;
            if (type == x->incr) { XFree(data); return 0; }   /* huge transfers are not worth the machinery */
            char* out = (char*)malloc(n + 1);
            if (out) { memcpy(out, data, n); out[n] = 0; }
            XFree(data);
            return out;
        }
        struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 1000000L;
        nanosleep(&ts, 0);
    }
    return 0;
}

/* A desktop window owns its whole rectangle, the pointer can hover, and a wheel notch is four
 * logical pixels — the conventions every desktop caller already assumes. These differ on a phone,
 * which is exactly why they are asked for rather than assumed. */
int  nori_x11_inset_top(void* h)        { (void)h; return 0; }
int  nori_x11_has_hover(void* h)        { (void)h; return 1; }
int  nori_x11_scroll_unit_1000(void* h) { (void)h; return 4000; }
/* No on-screen keyboard, and no viewport indirection: an X11 buffer is presented at its own size. */
void nori_x11_soft_keyboard(void* h, int show) { (void)h; (void)show; }
/* No system back gesture here; the Escape key is already an ordinary key event. */
int nori_x11_catch_back(void* h, int on) { (void)h; (void)on; return 0; }
void nori_x11_apply_viewport(void* h)          { (void)h; }

/* Drag-and-drop is XDND, a multi-round protocol between windows, and it is not implemented here.
 * These report "nothing was dragged, nothing was dropped" rather than approximating it, so a caller
 * that checks them behaves as it would on any platform where a drag never happens. */
void  nori_x11_start_drag(void* h, const char* text) { (void)h; (void)text; }
int   nori_x11_drag_finished(void* h)                { (void)h; return 0; }
char* nori_x11_drop_text(void* h)                    { (void)h; return 0; }

/* A 1x1 fully transparent cursor. XFixesHideCursor would need another library for one call. */
static Cursor x11_blank_cursor(Display* d, Window w) {
    static char zero[8] = {0,0,0,0,0,0,0,0};
    Pixmap pm = XCreateBitmapFromData(d, w, zero, 8, 8);
    XColor black; black.red = black.green = black.blue = 0; black.flags = DoRed|DoGreen|DoBlue;
    Cursor c = XCreatePixmapCursor(d, pm, pm, &black, &black, 0, 0);
    XFreePixmap(d, pm);
    return c;
}

/* Pointer shapes, from the standard X cursor font. XC_xterm and the two double arrows are what a
 * text widget and a divider want; XC_hand2 and XC_fleur cover the other two. A shape we cannot name
 * falls back to the arrow rather than leaving whatever was there. */
int nori_x11_set_cursor(void* h, int shape) {
    NoriX11* x = (NoriX11*)h;
    if (!x || !x->d) return 0;
    if (x->locked) return 0;                    /* a locked pointer is hidden; do not un-hide it */
    if (shape < 0 || shape > 5) shape = 0;
    if (x->shape_cur == shape) return 1;        /* already showing it: no round trip */
    if (!x->shapes[shape]) {
        unsigned int font_shape = XC_left_ptr;
        switch (shape) {
            case 1: font_shape = XC_xterm; break;
            case 2: font_shape = XC_hand2; break;
            case 3: font_shape = XC_sb_h_double_arrow; break;
            case 4: font_shape = XC_sb_v_double_arrow; break;
            case 5: font_shape = XC_fleur; break;
            default: font_shape = XC_left_ptr; break;
        }
        x->shapes[shape] = XCreateFontCursor(x->d, font_shape);
        if (!x->shapes[shape]) return 0;
    }
    XDefineCursor(x->d, x->win, x->shapes[shape]);
    x->shape_cur = shape;
    return 1;
}

int nori_x11_lock_pointer(void* h, int on) {
    NoriX11* x = (NoriX11*)h;
    if (!x || !x->d) return 0;
    if (on) {
        if (!x->blank) x->blank = x11_blank_cursor(x->d, x->win);
        XDefineCursor(x->d, x->win, x->blank);
        XGrabPointer(x->d, x->win, True,
                     ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, x->win, x->blank, CurrentTime);
        x->warp_x = x->w / 2; x->warp_y = x->h / 2;
        XWarpPointer(x->d, None, x->win, 0, 0, 0, 0, x->warp_x, x->warp_y);
        XFlush(x->d);
        x->locked = 1; x->rel_dx = 0; x->rel_dy = 0;
        return 1;
    }
    if (x->locked) {
        XUngrabPointer(x->d, CurrentTime);
        XUndefineCursor(x->d, x->win);
        XFlush(x->d);
        x->locked = 0;
        x->shape_cur = 0;   /* the window has no cursor defined now, which IS the default arrow */
    }
    return 1;
}
int nori_x11_pointer_locked(void* h) { NoriX11* x = (NoriX11*)h; return x && x->locked; }

/* The delta since the last call, then re-centre. Reading and warping together is deliberate: the
 * warp itself generates a motion event, and doing both here means the next read sees only real
 * movement rather than the correction. */
int nori_x11_pointer_delta(void* h, int* out_dxdy) {
    NoriX11* x = (NoriX11*)h;
    if (!x || !x->locked) { out_dxdy[0] = 0; out_dxdy[1] = 0; return 0; }
    out_dxdy[0] = (x->px - x->warp_x) * 256;
    out_dxdy[1] = (x->py - x->warp_y) * 256;
    x->warp_x = x->w / 2; x->warp_y = x->h / 2;
    XWarpPointer(x->d, None, x->win, 0, 0, 0, 0, x->warp_x, x->warp_y);
    XFlush(x->d);
    x->px = x->warp_x; x->py = x->warp_y;
    return 1;
}

void nori_x11_close(void* h) {
    NoriX11* x = (NoriX11*)h;
    if (!x) return;
    free(x->clip_own);
    if (x->d) { XDestroyWindow(x->d, x->win); XCloseDisplay(x->d); }
    free(x);
}
