/* window_android.c — the std/window seam on Android, against ANativeWindow.
 *
 * Same 22 `nori_sfc_*` entry points as window_wayland.c, so the Nori side
 * (std/window_wayland) is unchanged and a program picks a backend purely by which .c it compiles:
 *     --cfile std/window/window_wayland.c --clink wayland-client   (desktop)
 *     --cfile std/window/window_android.c                          (Android; libandroid, see below)
 *
 * The shape of the two platforms differs in one way that matters. On Wayland a program opens a
 * window: nori_sfc_open connects, creates a surface, and owns it. On Android it does not — the system
 * creates the surface and hands it to the app through activity callbacks, before or after the Nori
 * code is ready, and takes it away again when the app is backgrounded. So the window here is a
 * singleton owned by the platform glue, which pushes it in through the nori_android_* entry points
 * at the bottom of this file; nori_sfc_open just adopts whatever is current. That is why it can
 * return 0 (no surface yet) and why should_close reports true once the surface is gone: both are
 * normal, recurring states on Android rather than the end of the program.
 *
 * Link with -landroid (ANativeWindow_*) — the NDK ships it in the sysroot, no extra dependency.
 */
#include <android/native_window.h>
#include "window_android_blit.h"
#include <android/input.h>
#include <android/keycodes.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define EV_CLOSED  1
#define EV_RESIZED 2
#define EV_KEYDOWN 3
#define EV_KEYUP   4
#define EV_DROP    5
#define EV_SCROLL  6
#define EV_MOUSEDOWN 7
#define EV_MOUSEUP   8

#define QN 64
/* c is the third payload word: only the mouse events use it (a = button, b = x, c = y). */
typedef struct { int type, a, b, c; } AndEv;

/* How many finger positions to keep, and how far back to look when turning them into a speed.
   Android reports touches at 120-240Hz, so eight samples is roughly the window below. */
#define AND_VN 8
#define AND_VEL_WINDOW_MS 60

typedef struct {
    ANativeWindow* win;          /* NULL between surfaceDestroyed and the next surfaceCreated */
    int w, h;                    /* current surface size in pixels */
    int closed;                  /* the activity is finishing (not merely backgrounded) */
    int px, py, btn;             /* the pointer, as touch reports it */
    int tap_latch;               /* a completed tap not yet observed by a poll — see nori_android_touch */
    int held;                    /* a finger is on the glass right now — see nori_sfc_pointer_held */
    int catch_back;              /* the program wants the back gesture — see nori_sfc_catch_back */
    int t_last_y, t_start_y;     /* the drag in progress, in device pixels */
    int t_down_x, t_down_y;      /* where the finger landed, in device pixels (a tap's position) */
    int t_moved;                 /* has it passed the slop, i.e. is this a scroll and not a tap? */
    int t_rem;                   /* the fraction of a logical pixel not yet emitted — see and_scroll_by_locked */
    long long t_last_ms;         /* for the fling: when the last movement was seen, and how fast */
    int t_vel;                   /* device pixels per second, signed the way a wheel is */
    int fling_vel;               /* the throw still decaying after the finger left */
    long long fling_ms;
    int fling_rem;               /* the fling's sub-pixel remainder, in thousandths of a pixel */
    int v_y[AND_VN];             /* recent finger positions, for the throw's speed */
    long long v_t[AND_VN];
    int v_head, v_cnt;
    long long frame_due_ns;      /* when the next frame is due, for the pacing in frame_wait */
    int scale120;                /* display density in 120ths, 120 = 1.0 (mdpi) */
    AndEv q[QN];
    int qhead, qtail;
} NoriAnd;

/* One activity, one surface: unlike Wayland there is nothing to key a per-window struct on, and the
   glue that receives the callbacks has no Nori handle to pass back. */
static NoriAnd g_and;

/* This struct is written from two threads and that is not an accident of implementation, it is how
   Android works: the activity callbacks (surface created/destroyed/resized, touches, keys) arrive on
   the UI thread, while the render loop that reads them is the Nori program running on its own
   thread. The Wayland backend needs no lock because there everything happens on the thread that
   pumps the display. Here the queue indices and the window pointer would be a plain data race.
   The lock is only ever held for a few field assignments, never across a blit. */
static pthread_mutex_t g_lk = PTHREAD_MUTEX_INITIALIZER;
#define AND_LOCK()   pthread_mutex_lock(&g_lk)
#define AND_UNLOCK() pthread_mutex_unlock(&g_lk)

static void push_ev3_locked(int type, int a, int b, int c) {
    int n = (g_and.qtail + 1) % QN;
    if (n != g_and.qhead) { g_and.q[g_and.qtail].type = type; g_and.q[g_and.qtail].a = a; g_and.q[g_and.qtail].b = b; g_and.q[g_and.qtail].c = c; g_and.qtail = n; }
}
static void push_ev_locked(int type, int a, int b) { push_ev3_locked(type, a, b, 0); }
static void push_ev(int type, int a, int b) { AND_LOCK(); push_ev_locked(type, a, b); AND_UNLOCK(); }

/* Defined with the rest of the touch handling further down; used by the event poll above it. */
static void and_fling_step_locked(void);

/* ---- the seam ------------------------------------------------------------------------------ */

/* Adopt the current surface. `title` and the requested size are ignored: an Android activity does not
   choose either — the window manager does. Returns 0 when there is no surface yet, which the Nori
   side already treats as "could not open"; the caller retries. */
void* nori_sfc_open(const char* title, int w, int h) {
    (void)title; (void)w; (void)h;
    AND_LOCK();
    ANativeWindow* w0 = g_and.win;
    AND_UNLOCK();
    if (!w0) return 0;
    return &g_and;
}

int  nori_sfc_w(void* h) { (void)h; AND_LOCK(); int v = g_and.w; AND_UNLOCK(); return v; }
int  nori_sfc_h(void* h) { (void)h; AND_LOCK(); int v = g_and.h; AND_UNLOCK(); return v; }
/* Closed means the activity is finishing, which is what the field says and what a render loop
   asking `should_close` means by it. A missing surface does not count: Android takes the surface
   away every time the app is backgrounded, so a loop written the obvious way
   (`while !should_close(win)`) would end when the user switched apps. The program would return,
   the activity would stay, and coming back would show a black window and a transparent card in
   recents. Whether there is anything to draw on right now is a different question, and
   `drawable` is it. */
int  nori_sfc_closed(void* h) { (void)h; AND_LOCK(); int v = g_and.closed ? 1 : 0; AND_UNLOCK(); return v; }
/* Is there a surface to draw on at this instant? False while backgrounded, true again when Android
   hands a new one over. present() is already a no-op without one; this lets a loop skip the work
   as well, which on a phone is the difference between idling and rendering 60 unseen frames a
   second into a buffer nobody reads. */
/* Is a finger down right now? This is distinct from "was there a tap". The tap latch answers a
   completed gesture once and clears itself, which is exactly right for a button and useless for
   anything dragged: a scrubber needs to know the touch is still in progress, and where it is, on
   every frame. Reading this consumes nothing. */
int  nori_sfc_pointer_held(void* h) { (void)h; AND_LOCK(); int v = g_and.held; AND_UNLOCK(); return v; }

/* Take the back gesture, or leave it to the system. Off by default: a program that catches back and
   then ignores it has trapped the person inside it, and the system's own behaviour -- leave the
   activity -- is the right one for every program that has nothing to dismiss. A player that is
   fullscreen, or showing a video, has something to dismiss, so it asks for the key and takes
   responsibility for closing the window when it runs out of things to undo. */
int  nori_sfc_catch_back(void* h, int on) { (void)h; AND_LOCK(); g_and.catch_back = on ? 1 : 0; AND_UNLOCK(); return 0; }
__attribute__((used, visibility("default")))
int nori_android_catch_back(void) { AND_LOCK(); int v = g_and.catch_back; AND_UNLOCK(); return v; }

int  nori_sfc_drawable(void* h) { (void)h; AND_LOCK(); int v = (!g_and.closed && g_and.win) ? 1 : 0; AND_UNLOCK(); return v; }

/* There is no display/surface pointer a GPU backend could consume the way wgpu consumes a
   wl_display*: on Android it wants the ANativeWindow itself, so hand that back from both. */
void* nori_sfc_display(void* h) { (void)h; return g_and.win; }
void* nori_sfc_surface(void* h) { (void)h; return g_and.win; }

__attribute__((weak)) void nori_android_finish(void);

/* Dropping the last reference is the glue's job (it owns the callbacks); a Nori `close` only means
   "this program is done drawing", so it must not release a surface the system still owns. */
void nori_sfc_close(void* h) {
    (void)h;
    g_and.closed = 1;
    /* Weak, because this file is linked in builds that have no activity glue at all (host test
       builds compile it without the glue). Where the glue is present, this is what actually leaves the app. */
    if (nori_android_finish) nori_android_finish();
}

/* `out` must have room for three ints: the mouse events fill (button, x, y). */
int nori_sfc_poll_event(void* h, int* out) {
    (void)h;
    out[0] = 0; out[1] = 0; out[2] = 0;
    AND_LOCK();
    and_fling_step_locked();
    if (g_and.qhead == g_and.qtail) { AND_UNLOCK(); return 0; }
    AndEv e = g_and.q[g_and.qhead]; g_and.qhead = (g_and.qhead + 1) % QN;
    AND_UNLOCK();
    out[0] = e.a; out[1] = e.b; out[2] = e.c; return e.type;
}

/* A tap is shorter than a frame. A finger is down for something like 80ms; a CPU-rasterised frame on
   a phone can take longer than that, so a caller that polls the button state sees 0 both before and
   after and the tap never happened. The pointer position survives (it is not cleared on release),
   which is why a link would take its hover style and then do nothing at all.
   So a press that has not yet been observed is LATCHED: the first read after it reports the button
   down even if the finger has already lifted. The next read sees the real state, so a tap becomes
   exactly one press-release edge no matter how slowly the caller is polling. */
/* Touch arrives in device pixels; everything above this seam works in logical ones, because that is
   what Wayland reports and what the presented buffer is sized in. Converting here keeps the pointer
   in the same space as the pixels a caller drew, so one hit-test is correct on both platforms. */
static int and_logical(int v) {
    int s = g_and.scale120 > 0 ? g_and.scale120 : 120;
    return v * 120 / s;
}
static int and_take_down(void) {
    if (g_and.btn) return 1;
    if (g_and.tap_latch) { g_and.tap_latch = 0; return 1; }
    return 0;
}
int nori_sfc_mouse(void* h, int* out_xy) { (void)h; AND_LOCK(); out_xy[0] = and_logical(g_and.px); out_xy[1] = and_logical(g_and.py); int d = and_take_down(); AND_UNLOCK(); return d; }
/* Pointer lock is a mouse idea: a touch screen has no cursor to confine and no relative motion to
   deliver, so the request is simply never granted. */
int nori_sfc_lock_pointer(void* h, int on) { (void)h; (void)on; return 0; }
int nori_sfc_pointer_locked(void* h) { (void)h; return 0; }
int nori_sfc_pointer_delta(void* h, int* out) { (void)h; if (out) { out[0] = 0; out[1] = 0; } return 0; }
int nori_sfc_mouse_x(void* h) { (void)h; AND_LOCK(); int v = and_logical(g_and.px); AND_UNLOCK(); return v; }
int nori_sfc_mouse_y(void* h) { (void)h; AND_LOCK(); int v = and_logical(g_and.py); AND_UNLOCK(); return v; }
int nori_sfc_mouse_down(void* h) { (void)h; AND_LOCK(); int v = and_take_down(); AND_UNLOCK(); return v; }
/* No middle button exists on a touch screen. Reporting "not held" is the truthful answer, and the
   callers that ask (middle-click paste, middle-click close) simply never fire. */
int nori_sfc_mouse_middle(void* h) { (void)h; return 0; }

/* Raise or dismiss the on-screen keyboard; implemented in android_native_activity.c, which is where
   the activity lives. A desktop has a keyboard already, so the other backend does nothing. */
void nori_android_soft_keyboard(int show);
void nori_sfc_soft_keyboard(void* h, int show) { (void)h; nori_android_soft_keyboard(show); }

/* The unusable strip at the top of the screen, in logical pixels to match everything else above this
   seam. Queried each call rather than cached: it changes with rotation and with a folding screen. */
int nori_android_inset_top(void);
int nori_sfc_inset_top(void* h) { (void)h; return and_logical(nori_android_inset_top()); }

int nori_sfc_scale120(void* h) { (void)h; return g_and.scale120 > 0 ? g_and.scale120 : 120; }
/* A scroll unit is one device pixel here, which in the logical pixels a caller lays out in is
   1/scale of one — 381 thousandths on a 2.625x phone. */
/* A finger does not hover. It is only on the glass while it is pressing, and a scroll drags it
   across everything between where the gesture started and where it ended — so a caller that treats
   the pointer as hovering restyles its way down the page on every swipe. */
int nori_sfc_has_hover(void* h) { (void)h; return 0; }

/* …and for the same reason there is no pointer to give a shape to. The Wayland backend answers 0
   when the pointer has not entered the surface, which is exactly this situation permanently: the
   caller asked for a text caret or a resize arrow and nothing was drawn, which is what 0 says. An
   Android build that silently returned 1 would have callers believing they had changed a cursor
   that does not exist. */
int nori_sfc_set_cursor(void* h, int shape) { (void)h; (void)shape; return 0; }

int nori_sfc_scroll_unit_1000(void* h) {
    (void)h;
    int s = g_and.scale120 > 0 ? g_and.scale120 : 120;
    return 120000 / s;
}
/* Wayland's viewporter maps a physical buffer onto a logical window size. Android has no equivalent
   because there is no logical size to map onto — the buffer geometry set in present IS the mapping. */
void nori_sfc_apply_viewport(void* h) { (void)h; }

/* Blit a CPU framebuffer. The pixel conversion and the stride walk live in window_android_blit.h so
   they can be checked against known bytes on the host; they are the parts that go wrong silently,
   drawing a plausible but wrong image rather than failing (see the header). What stays here is the surface handshake around them.

   The clamp matters: the buffer the allocator hands back need not be the geometry just requested —
   setBuffersGeometry takes effect on a later dequeue, so the first frame after a resize can still be
   the old size. Writing hh rows into a shorter buffer would run off the end of the mapping. */
void nori_sfc_shm_present(void* h, void* pixels, int ww, int hh) {
    (void)h;
    if (!pixels || ww <= 0 || hh <= 0) return;
    /* Take a reference, not a peek. This runs on the Nori thread while the UI thread may be inside
       onNativeWindowDestroyed — reading g_and.win and then blitting through it races with the
       release, and the window would be freed underneath a memcpy. ANativeWindow is refcounted, so
       holding one across the blit is exactly what keeps it alive; the lock itself is released
       immediately, because a blit is far too long to hold it. */
    AND_LOCK();
    ANativeWindow* win = g_and.win;
    if (win) ANativeWindow_acquire(win);
    AND_UNLOCK();
    if (!win) return;
    ANativeWindow_setBuffersGeometry(win, ww, hh, WINDOW_FORMAT_RGBX_8888);
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(win, &buf, NULL) != 0) { ANativeWindow_release(win); return; }
    int rows = hh < buf.height ? hh : buf.height;
    int cols = ww < buf.width  ? ww : buf.width;
    for (int y = 0; y < rows; y++) {
        const uint32_t* s = (const uint32_t*)pixels + (size_t)y * (size_t)ww;
        uint32_t* d = (uint32_t*)buf.bits + (size_t)y * (size_t)buf.stride;
        for (int x = 0; x < cols; x++) d[x] = nori_and_px(s[x]);
    }
    ANativeWindow_unlockAndPost(win);
    ANativeWindow_release(win);
}

/* Pace the loop. unlockAndPost already blocks on the compositor when the swap chain is full, so this
   only has to avoid spinning when nothing was drawn. A Choreographer vsync callback would be the
   better source, but it needs a looper on this thread — Stage 4's app model decides that. */
/* Pace to a deadline, not a fixed sleep. Sleeping the whole interval after the frame's work adds
   the work to the interval — a 6ms paint plus a 16ms sleep is a 22ms cadence, and one that wobbles
   by however long each paint took. A glide shown on a cadence like that steps visibly even though
   every frame was cheap. Sleeping only what is left until the next deadline holds the interval that
   was asked for. */
void nori_sfc_frame_wait(void* h, int timeout_ms) {
    (void)h;
    if (timeout_ms <= 0) return;
    /* Paced even with input waiting.
     *
     * Skipping the wait whenever the queue is not empty sounds like responsiveness and is not: a
     * finger dragging a list keeps the queue permanently non-empty, so the loop would draw as fast as it
     * could: ninety frames a second into a display that shows sixty. The extra frames do not
     * arrive early, they arrive at whatever moment the swap chain frees a buffer, and a picture
     * updated at an irregular rate is exactly what "not quite smooth" looks like.
     *
     * A frame late is one refresh, sixteen milliseconds, and the queue is drained before the frame
     * is drawn either way, so nothing is missed; it is merely shown at the rate the glass has. */
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long long now_ns = (long long)now.tv_sec * 1000000000LL + now.tv_nsec;
    long long period = (long long)timeout_ms * 1000000LL;
    long long due = g_and.frame_due_ns;
    /* A first frame, or a stall long enough that catching up would mean not sleeping at all. */
    if (due <= 0 || now_ns - due > period * 4) due = now_ns;
    due += period;
    g_and.frame_due_ns = due;
    long long wait = due - now_ns;
    if (wait <= 0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)(wait / 1000000000LL);
    ts.tv_nsec = (long)(wait % 1000000000LL);
    nanosleep(&ts, NULL);
}

/* Clipboard and drag-and-drop are Java-side on Android: ClipboardManager is reachable only through
   JNI, and there is no NDK surface for it. Rather than pretend, these report "empty" / do nothing,
   which is what a program sees on a desktop with an empty selection — a path callers already handle.
   Wiring them to JNI belongs with the rest of the glue in Stage 4. */
void  nori_sfc_clipboard_set(void* h, const char* text) { (void)h; (void)text; }
char* nori_sfc_clipboard_get(void* h) { (void)h; return 0; }
char* nori_sfc_drop_text(void* h) { (void)h; return 0; }
void  nori_sfc_start_drag(void* h, const char* text) { (void)h; (void)text; }
int   nori_sfc_drag_finished(void* h) { (void)h; return 0; }

/* ---- entry points for the platform glue ---------------------------------------------------- */
/* These are what an activity (NativeActivity's callbacks, or a Java shim through JNI) calls. They are
   the only way a surface or an input event reaches this file.
 *
 * Nothing inside the build calls them, so --gc-sections and LTO drop a
 * function no reachable code references; without `used` these vanish from the binary and the app
 * links fine and then never receives a surface. `visibility("default")` keeps them in the dynamic
 * symbol table of the .so an APK loads, which is how JNI finds them at all. */
#define NORI_GLUE __attribute__((used, visibility("default")))

/* onNativeWindowCreated / onSurfaceCreated. Takes a reference so the surface survives until we drop
   it, which matters because the activity may destroy its own reference during a configuration change
   while a frame is still being blitted. */
NORI_GLUE void nori_android_surface_created(void* nativeWindow) {
    ANativeWindow* w = (ANativeWindow*)nativeWindow;
    if (!w) return;
    ANativeWindow_acquire(w);
    AND_LOCK();
    if (g_and.win) ANativeWindow_release(g_and.win);
    g_and.win = w;
    g_and.w = ANativeWindow_getWidth(w);
    g_and.h = ANativeWindow_getHeight(w);
    g_and.closed = 0;
    push_ev_locked(EV_RESIZED, g_and.w, g_and.h);
    AND_UNLOCK();
}

/* onNativeWindowResized, and after a rotation. */
NORI_GLUE void nori_android_surface_resized(int w, int h) {
    if (w <= 0 || h <= 0) return;
    AND_LOCK();
    if (w == g_and.w && h == g_and.h) { AND_UNLOCK(); return; }
    g_and.w = w; g_and.h = h;
    push_ev_locked(EV_RESIZED, w, h);
    AND_UNLOCK();
}

/* onNativeWindowDestroyed. Not the end of the program: Android destroys the surface whenever the app
   goes to the background and creates a new one on return, so this reports "cannot draw right now".
   should_close is true meanwhile, which a render loop already treats as "stop drawing". */
NORI_GLUE void nori_android_surface_destroyed(void) {
    AND_LOCK();
    ANativeWindow* w = g_and.win;
    g_and.win = 0;
    AND_UNLOCK();
    if (w) ANativeWindow_release(w);
}

/* The activity is actually finishing. */
NORI_GLUE void nori_android_finishing(void) { AND_LOCK(); g_and.closed = 1; push_ev_locked(EV_CLOSED, 0, 0); AND_UNLOCK(); }

/* Display density, so scale() reports something true. 160dpi (mdpi) is 1.0 => 120. */
NORI_GLUE void nori_android_set_density(int dpi) { if (dpi > 0) g_and.scale120 = dpi * 120 / 160; }

/* A touch, mapped onto the pointer the rest of the stack already speaks. `action` is the masked
   AMOTION_EVENT_ACTION_*. Only the first pointer is tracked: everything above this seam is a
   single-cursor model, so a second finger would otherwise yank the cursor across the screen. */
/* A finger is not a wheel, so a touch drag does not report in wheel notches. A notch is worth 4
   logical pixels to a caller, which at a phone's density is ten or more device pixels — and a drag
   quantised to that moves the page in visible steps, some frames jumping two notches and some none,
   which reads as stuttering however fast the frames are. So a touch reports ONE DEVICE PIXEL per
   unit and says so through nori_sfc_scroll_unit_1000; the caller multiplies by the unit and gets the
   same distance, arrived at smoothly. */
/* Below this the gesture is still a tap: fingers are not steady, and a few pixels of tremble must not
   turn a link press into a scroll. Device pixels, so it means the same physical distance anywhere. */
#define AND_TAP_SLOP 16

static long long and_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Logical pixels of content movement, which is what Event::Scroll carries — see window.nori.
   Device pixels would mean the same drag scrolls twice as far on a denser screen, and detents (what
   this used to emit, one per 24dp of travel) mean a finger drags the page in 24-pixel jumps: the
   list stutters up the screen in steps instead of following the finger, which reads as a slow
   interface rather than a coarse one.

   t_rem carries the fraction not yet worth a whole logical pixel — without it a slow drag on a dense
   screen would round to zero on every move and scroll nothing at all. The fling comes through here
   too and inherits both. */
static void and_scroll_by_locked(int dy_dev) {
    int s = g_and.scale120 > 0 ? g_and.scale120 : 120;
    g_and.t_rem += dy_dev * 120;                  /* in 120ths of a logical pixel */
    int n = g_and.t_rem / s;                      /* toward zero, so the remainder keeps the sign */
    if (n != 0) {
        g_and.t_rem -= n * s;
        push_ev_locked(EV_SCROLL, n, 0);
    }
}

/* The last two samples are not a speed. A finger decelerates as it lifts, and Android delivers
   moves a couple of milliseconds apart, so the closing pair is a pixel or two over a moment or two —
   noise, scaled by a thousand. Read as the throw's speed it produces flings anywhere from motionless
   to several screens a second from the same flick, which is what makes a page feel unpredictable
   rather than merely fast. Averaging over the whole recent window is what a throw actually was. */
static void and_vel_add_locked(int y, long long t) {
    g_and.v_y[g_and.v_head] = y; g_and.v_t[g_and.v_head] = t;
    g_and.v_head = (g_and.v_head + 1) % AND_VN;
    if (g_and.v_cnt < AND_VN) g_and.v_cnt++;
}
static int and_vel_locked(void) {
    if (g_and.v_cnt < 2) return 0;
    int newest = (g_and.v_head - 1 + AND_VN) % AND_VN;
    int oldest = newest;
    for (int i = 1; i < g_and.v_cnt; i++) {
        int k = (g_and.v_head - 1 - i + 2 * AND_VN) % AND_VN;
        if (g_and.v_t[newest] - g_and.v_t[k] > AND_VEL_WINDOW_MS) break;
        oldest = k;
    }
    long long dt = g_and.v_t[newest] - g_and.v_t[oldest];
    if (dt <= 0) return 0;
    long long v = (long long)(g_and.v_y[oldest] - g_and.v_y[newest]) * 1000 / dt;  /* up = content down */
    /* A ceiling in device pixels a second. Nothing a hand does exceeds this; anything that reads
       higher is a sampling artefact, and letting it through teleports the page. */
    if (v >  12000) v =  12000;
    if (v < -12000) v = -12000;
    return (int)v;
}

/* A touch screen has no wheel and no second button, so the gesture has to say which it is. A drag
   past the slop scrolls and never presses; a touch that lifts without travelling is a tap and presses
   exactly once. Deciding here rather than in the caller is what lets one program serve a mouse and a
   finger: above this seam there is still just a pointer and a wheel.
   Nothing presses on DOWN any more — a press cannot be taken back once reported, and whether this is
   a tap or a scroll is not known until the finger moves or lifts. */
NORI_GLUE void nori_android_touch(int action, int x, int y) {
    AND_LOCK();
    g_and.px = x; g_and.py = y;
    long long now = and_now_ms();
    if (action == AMOTION_EVENT_ACTION_DOWN) {
        g_and.held = 1;
        g_and.t_start_y = y; g_and.t_last_y = y;
        g_and.t_down_x = x; g_and.t_down_y = y;
        g_and.t_moved = 0; g_and.t_rem = 0;
        g_and.t_vel = 0; g_and.t_last_ms = now;
        g_and.v_head = 0; g_and.v_cnt = 0;
        and_vel_add_locked(y, now);
        g_and.fling_vel = 0;                 /* touching down catches a page still gliding */
        g_and.fling_rem = 0;
    } else if (action == AMOTION_EVENT_ACTION_MOVE) {
        int dy = g_and.t_last_y - y;         /* finger up = content moves down, as a wheel does */
        if (!g_and.t_moved) {
            int travel = g_and.t_start_y - y;
            if (travel < 0) travel = -travel;
            if (travel > AND_TAP_SLOP) g_and.t_moved = 1;
        }
        if (g_and.t_moved) {
            and_scroll_by_locked(dy);
            long long dt = now - g_and.t_last_ms;
            if (dt > 0 && dt < 200) g_and.t_vel = (int)((long long)dy * 1000 / dt);
        }
        and_vel_add_locked(y, now);
        g_and.t_last_y = y;
        g_and.t_last_ms = now;
    } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
        g_and.held = 0;
        if (!g_and.t_moved && action == AMOTION_EVENT_ACTION_UP) {
            g_and.tap_latch = 1;             /* a tap: press once, on whichever poll looks next */
            /* and the same tap as the discrete events the other backends report, at the position the
               finger LANDED — a touch screen has one button, so the id is always 1 (left). */
            push_ev3_locked(EV_MOUSEDOWN, 1, and_logical(g_and.t_down_x), and_logical(g_and.t_down_y));
            push_ev3_locked(EV_MOUSEUP,   1, and_logical(g_and.t_down_x), and_logical(g_and.t_down_y));
        } else if (now - g_and.t_last_ms < 80) {
            /* Thrown rather than placed: keep going and decay, which is what makes a list feel like a
               list. A finger that stopped before lifting has no velocity and nothing carries on. */
            g_and.fling_vel = and_vel_locked();
            g_and.fling_ms = now;
            g_and.fling_rem = 0;
        }
        g_and.btn = 0;
        g_and.t_moved = 0;
    }
    AND_UNLOCK();
}

/* Advance a fling. Called from the event poll, so it needs no timer thread: the caller is already
   asking for input every frame, and a throw that only moves while someone is watching is exactly the
   throw they can see. */
static void and_fling_step_locked(void) {
    if (g_and.fling_vel == 0) return;
    long long now = and_now_ms();
    long long dt = now - g_and.fling_ms;
    if (dt <= 0) return;
    if (dt > 100) dt = 100;                  /* a stall must not teleport the page */
    g_and.fling_ms = now;
    /* Thousandths, so nothing is lost to truncation: a 900px/s glide is under a pixel per
       millisecond, and rounding that away each step would stop the page dead while the speed on
       paper kept decaying. */
    g_and.fling_rem += (int)((long long)g_and.fling_vel * dt);
    int px = g_and.fling_rem / 1000;
    if (px != 0) { g_and.fling_rem -= px * 1000; and_scroll_by_locked(px); }
    /* Friction is per millisecond, not per call. This runs from the event poll, and a caller drains
       its queue by polling until it comes back empty — several times per millisecond. Taking a fixed
       percentage each time meant the throw lost most of its speed before the first frame was even
       drawn, which reads as a page that barely moves after the finger leaves.
       ~0.4%/ms is around 6% per 16ms frame: about a second of glide from a firm swipe. */
    long long v = g_and.fling_vel;
    v -= v * dt * 4 / 1000;
    g_and.fling_vel = (int)v;
    if (g_and.fling_vel < 60 && g_and.fling_vel > -60) g_and.fling_vel = 0;
}

/* A scroll gesture, in the wheel-detent units the Scroll event carries (+ve = down//away). */
NORI_GLUE void nori_android_scroll(int dy) { if (dy != 0) push_ev(EV_SCROLL, dy, 0); }

/* A key, already translated to the X11 keysym the Nori API speaks (see std/window_wayland's KEY_*).
   The translation is left to the caller on purpose: the glue is where a keycode can be resolved
   against the active layout, and where IME text arrives — neither is knowable from a bare keycode
   here. nori_android_keysym_of below covers the navigation keys a hardware keyboard sends. */
NORI_GLUE void nori_android_key(int down, int keysym, int mods) {
    push_ev(down ? EV_KEYDOWN : EV_KEYUP, keysym, mods);
}

/* AKEYCODE_* -> Unicode codepoint for the printable keys, honouring shift. The NDK exposes no key
   character map — KeyEvent.getUnicodeChar is JNI only — so this is a US-layout table, which is what
   a URL bar and a text field need. Returns 0 for anything that is not a character. */
NORI_GLUE int nori_android_unicode_of(int keycode, int meta) {
    int shift = (meta & AMETA_SHIFT_ON) != 0;
    if (keycode >= AKEYCODE_A && keycode <= AKEYCODE_Z)
        return (shift ? 'A' : 'a') + (keycode - AKEYCODE_A);
    if (keycode >= AKEYCODE_0 && keycode <= AKEYCODE_9) {
        static const char sh[] = ")!@#$%^&*(";
        return shift ? sh[keycode - AKEYCODE_0] : '0' + (keycode - AKEYCODE_0);
    }
    switch (keycode) {
        case AKEYCODE_SPACE:         return ' ';
        case AKEYCODE_COMMA:         return shift ? '<' : ',';
        case AKEYCODE_PERIOD:        return shift ? '>' : '.';
        case AKEYCODE_MINUS:         return shift ? '_' : '-';
        case AKEYCODE_EQUALS:        return shift ? '+' : '=';
        case AKEYCODE_SLASH:         return shift ? '?' : '/';
        case AKEYCODE_BACKSLASH:     return shift ? '|' : '\\';
        case AKEYCODE_SEMICOLON:     return shift ? ':' : ';';
        case AKEYCODE_APOSTROPHE:    return shift ? '"' : '\'';
        case AKEYCODE_LEFT_BRACKET:  return shift ? '{' : '[';
        case AKEYCODE_RIGHT_BRACKET: return shift ? '}' : ']';
        case AKEYCODE_GRAVE:         return shift ? '~' : '`';
        case AKEYCODE_AT:            return '@';
        case AKEYCODE_PLUS:          return '+';
        case AKEYCODE_POUND:         return '#';
        case AKEYCODE_STAR:          return '*';
    }
    return 0;
}

/* AKEYCODE_* -> X11 keysym for the named keys the window API exposes; 0 when there is no mapping
   (a printable key, which the glue should deliver as its Unicode codepoint instead). */
NORI_GLUE int nori_android_keysym_of(int keycode) {
    switch (keycode) {
        case AKEYCODE_ESCAPE:        return 65307;
        case AKEYCODE_DEL:           return 65288;   /* Android's DEL is Backspace */
        case AKEYCODE_FORWARD_DEL:   return 65535;
        case AKEYCODE_ENTER:         return 65293;
        case AKEYCODE_NUMPAD_ENTER:  return 65293;
        case AKEYCODE_TAB:           return 65289;
        case AKEYCODE_DPAD_LEFT:     return 65361;
        case AKEYCODE_DPAD_UP:       return 65362;
        case AKEYCODE_DPAD_RIGHT:    return 65363;
        case AKEYCODE_DPAD_DOWN:     return 65364;
        case AKEYCODE_MOVE_HOME:     return 65360;
        case AKEYCODE_MOVE_END:      return 65367;
        case AKEYCODE_PAGE_UP:       return 65365;
        case AKEYCODE_PAGE_DOWN:     return 65366;
        /* Back is the system back gesture, not a key: the closest honest mapping is Escape, which is
           what a desktop user presses to dismiss. The activity still decides whether to finish. */
        case AKEYCODE_BACK:          return 65307;
        default:                     return 0;
    }
}
