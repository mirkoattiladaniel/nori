/* The Win32 implementation of the window seam.
 *
 * Windows has exactly one windowing system, so there is nothing to choose between at runtime and no
 * dispatcher here: this file exports nori_win_* directly, the same names window_dispatch.c exports
 * on Linux. Everything above the seam is identical on both.
 *
 * Keys are reported as X11 keysyms, not as virtual-key codes. That is the seam's contract: the
 * Wayland backend resolves its xkb keymap into X11 keysyms and the X11 backend reports them
 * natively — so key_text above this seam decodes one alphabet on every platform.
 */
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <windowsx.h>
#include <stdlib.h>
#include <string.h>

#define BK_WIN32 2
#define EVQ 128

/* c is the third payload word: only the mouse events use it (a = button, b = x, c = y). */
typedef struct { int type, a, b, c; } WEv;

typedef struct {
    HWND  hwnd;
    int   w, h, closed;
    int   px, py, btn, mid;
    int   lx, ly;               /* where the left button was pressed (the latch's position) */
    int   tap_latch;            /* a press not yet observed by a polled read; see nori_win_mouse_down */
    WEv   q[EVQ];
    int   qhead, qtail;
    char* drop;                 /* newline-separated file:// URIs from the last WM_DROPFILES */
    int   scroll_accum;         /* WHEEL_DELTA remainder, so a high-resolution wheel is not lost */
    int   locked;               /* pointer confined + hidden, motion read as a delta */
    int   warp_x, warp_y;       /* client-space point the cursor is returned to after every read */
    int   shape_cur;            /* pointer shape last asked for; re-applied on every WM_SETCURSOR */
    int   dpi;                  /* this window's DPI; 96 is 100%. See w32_lg. */
    int   altgr;                /* right Alt went down AS AltGr, so its release is reported as AltGr too */
    DWORD  tid;                 /* the thread that owns the window, and the only one with its messages */
    HANDLE thread;              /* it runs w32_win_thread below */
    HANDLE ready;               /* signalled once hwnd is created (or creation failed) */
    HANDLE wake;                /* signalled when an event is queued, so frame_wait can sleep on it */
    CRITICAL_SECTION qlock;     /* the queue is written by the window thread and read by the caller */
    int    startw, starth;      /* what nori_win_open was asked for; the window thread builds it */
    WCHAR  title[512];
} NoriW32;

/* Physical pixels in, logical pixels out.
 *
 * Everything above this seam works in logical units: nori_ui lays out at 1.0 and the renderer
 * multiplies by the scale from nori_win_scale120 to reach real pixels, which is how a Wayland
 * compositor hands its coordinates over too. Win32 speaks physical pixels in every message, and
 * for a DPI-unaware process the two are the same number. Once the process declares awareness they
 * differ by the display scale: at 125% a click at logical x=400 arrives as 500. */
static int w32_lg(NoriW32* s, int v) {
    int dpi = (s && s->dpi > 0) ? s->dpi : 96;
    if (dpi == 96) return v;
    return (int)((double)v * 96.0 / (double)dpi + 0.5);
}

/* The stock system cursors for our six shapes. IDC_SIZEWE/IDC_SIZENS are the divider arrows. */
static HCURSOR win32_cursor_for(int shape) {
    switch (shape) {
        case 1:  return LoadCursorW(NULL, IDC_IBEAM);
        case 2:  return LoadCursorW(NULL, IDC_HAND);
        case 3:  return LoadCursorW(NULL, IDC_SIZEWE);
        case 4:  return LoadCursorW(NULL, IDC_SIZENS);
        case 5:  return LoadCursorW(NULL, IDC_SIZEALL);
        default: return LoadCursorW(NULL, IDC_ARROW);
    }
}

/* Written on the window thread, read by whichever thread is running the render loop; see
   w32_win_thread for why those are not the same thread. */
static void w32_push3(NoriW32* s, int type, int a, int b, int c) {
    EnterCriticalSection(&s->qlock);
    int n = (s->qtail + 1) % EVQ;
    if (n == s->qhead) { LeaveCriticalSection(&s->qlock); return; }   /* full: drop the newest rather than overwrite unread */
    s->q[s->qtail].type = type; s->q[s->qtail].a = a; s->q[s->qtail].b = b; s->q[s->qtail].c = c;
    s->qtail = n;
    LeaveCriticalSection(&s->qlock);
    SetEvent(s->wake);
}
static void w32_push(NoriW32* s, int type, int a, int b) { w32_push3(s, type, a, b, 0); }

/* Virtual-key -> X11 keysym for the keys that are named rather than typed. Printable keys are
 * resolved through ToUnicode instead, because only that applies the user's layout. */
static int w32_named_keysym(WPARAM vk) {
    switch (vk) {
    case VK_ESCAPE: return 0xff1b;  case VK_BACK:   return 0xff08;
    case VK_DELETE: return 0xffff;  case VK_RETURN: return 0xff0d;
    case VK_TAB:    return 0xff09;  case VK_LEFT:   return 0xff51;
    case VK_UP:     return 0xff52;  case VK_RIGHT:  return 0xff53;
    case VK_DOWN:   return 0xff54;  case VK_HOME:   return 0xff50;
    case VK_END:    return 0xff57;  case VK_PRIOR:  return 0xff55;
    case VK_NEXT:   return 0xff56;  case VK_INSERT: return 0xff63;
    /* The modifiers are keys too. X11 and Wayland both deliver a press for Shift, Ctrl and Alt, and
     * everything above this seam tracks held modifiers by watching those presses, so a backend that
     * reported only the mask would leave the shell believing nothing was held. Windows sends the
     * generic VK_ by default and the sided ones only when asked; both map here. */
    case VK_SHIFT: case VK_LSHIFT:     return 0xffe1;   /* Shift_L */
    case VK_RSHIFT:                    return 0xffe2;
    case VK_CONTROL: case VK_LCONTROL: return 0xffe3;   /* Control_L */
    case VK_RCONTROL:                  return 0xffe4;
    case VK_MENU: case VK_LMENU:       return 0xffe9;   /* Alt_L */
    case VK_RMENU:                     return 0xffea;
    case VK_LWIN:                      return 0xffeb;   /* Super_L */
    case VK_RWIN:                      return 0xffec;
    default: return 0;
    }
}

/* AltGr is not Ctrl+Alt, whatever Windows says. On a layout with an AltGr key (Hungarian, German,
 * Polish, French…) Windows reports right Alt as left Ctrl plus right Alt: a fake Ctrl press arrives
 * first, and GetKeyState then says Ctrl is held for as long as AltGr is. Taken at its word, AltGr+y
 * (`>` on a Hungarian keyboard) would be Ctrl+Y and AltGr+f (`[`) would open Find. X11 and Wayland
 * report AltGr as its own level shift, so this applies to Windows only.
 *
 * The rule, as GLFW and SDL apply it: right Alt held with left Ctrl held and right
 * Ctrl not is AltGr, and neither Ctrl nor Alt is part of the chord. (A real LCtrl+RAlt chord on such
 * a layout reads the same way; every Windows program that types AltGr characters makes that trade.) */
static int w32_altgr_in(const BYTE* st) {
    return (st[VK_RMENU] & 0x80) && (st[VK_LCONTROL] & 0x80) && !(st[VK_RCONTROL] & 0x80);
}
static int w32_mods_from(const BYTE* st) {
    int m = 0;
    if (st[VK_SHIFT]   & 0x80) m |= 1;
    if (st[VK_CONTROL] & 0x80) m |= 2;
    if (st[VK_MENU]    & 0x80) m |= 4;
    if ((st[VK_LWIN] | st[VK_RWIN]) & 0x80) m |= 8;
    if (w32_altgr_in(st)) m &= ~(2 | 4);
    return m;
}
static int w32_mods(void) {
    BYTE st[256];
    if (!GetKeyboardState(st)) return 0;
    return w32_mods_from(st);
}

/* the fake left-Ctrl message that comes with AltGr: a non-extended VK_CONTROL whose next message is an
 * extended VK_MENU stamped with the same time. Press and release both come in such a pair. */
static int w32_is_altgr_ctrl(UINT msg, WPARAM wp, LPARAM lp, DWORD time, const MSG* next) {
    if (wp != VK_CONTROL || (HIWORD(lp) & KF_EXTENDED)) return 0;
    if (!(msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYUP)) return 0;
    if (!next) return 0;
    if (!(next->message == WM_KEYDOWN || next->message == WM_SYSKEYDOWN || next->message == WM_KEYUP || next->message == WM_SYSKEYUP)) return 0;
    return next->wParam == VK_MENU && (HIWORD(next->lParam) & KF_EXTENDED) && next->time == time;
}

/* A printable key becomes its Unicode codepoint, which for ASCII and Latin-1 is the X11 keysym, and
 * above that is the keysym's 0x01000000 direct-Unicode form, the same encoding key_text decodes. */
static int w32_printable_keysym(WPARAM vk, LPARAM lparam) {
    BYTE state[256];
    if (!GetKeyboardState(state)) return 0;
    /* Ctrl and Alt are not part of the symbol. ToUnicode applies whatever is held, so Ctrl+P
     * translates to U+0010 (DLE), which is under 32 and discarded by the test below, so the key
     * event would never be pushed. The seam reports a keysym and a modifier mask separately, as X11
     * does, so the symbol wanted here is the one the key carries without them.
     *
     * Shift stays. X11 reports the shifted keysym (Shift+p is `P` (80), never `p` (112)) and the
     * shell's chord table is written against that: `ctrl+shift+p` is looked up by the shifted name. */
    WCHAR buf[8];
    UINT scan = (UINT)((lparam >> 16) & 0xff);
    /* AltGr is the exception and is part of the symbol: it is the level that makes `>` out of y. Translated
     * with it held first; only a key with nothing on that level (AltGr+q on a layout without one) falls
     * back to the plain symbol. The 0x4 flag keeps ToUnicode from consuming a dead key's state. */
    if (w32_altgr_in(state)) {
        int na = ToUnicode((UINT)vk, scan, state, buf, 8, 0x4);
        if (na > 0 && (unsigned)buf[0] >= 32) {
            unsigned cpa = (unsigned)buf[0];
            if (cpa < 256) return (int)cpa;
            return (int)(0x01000000u | cpa);
        }
    }
    state[VK_CONTROL] = 0; state[VK_LCONTROL] = 0; state[VK_RCONTROL] = 0;
    state[VK_MENU]    = 0; state[VK_LMENU]    = 0; state[VK_RMENU]    = 0;
    int n = ToUnicode((UINT)vk, scan, state, buf, 8, 0);
    if (n <= 0) return 0;
    unsigned cp = (unsigned)buf[0];
    if (cp < 32) return 0;
    if (cp < 256) return (int)cp;
    return (int)(0x01000000u | cp);
}

static char* w32_wide_to_utf8(const WCHAR* ws) {
    if (!ws) return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, 0, 0, 0, 0);
    if (n <= 0) return 0;
    char* out = (char*)malloc((size_t)n);
    if (!out) return 0;
    WideCharToMultiByte(CP_UTF8, 0, ws, -1, out, n, 0, 0);
    return out;
}

static LRESULT CALLBACK w32_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    NoriW32* s = (NoriW32*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
    /* Windows asks on every move over the client area, and the answer has to be given each time:
     * a SetCursor made elsewhere is undone the moment the pointer moves. Outside the client area
     * (the frame, the resize border) DefWindowProc has to handle it, or the window edges stop working. */
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT && !s->locked) { SetCursor(win32_cursor_for(s->shape_cur)); return TRUE; }
        break;
    case WM_CLOSE:
        s->closed = 1; w32_push(s, 1, 0, 0); return 0;
    case WM_DESTROY:
        s->closed = 1; w32_push(s, 1, 0, 0); return 0;
    case WM_SIZE: {
        int nw = w32_lg(s, LOWORD(lp)), nh = w32_lg(s, HIWORD(lp));
        if (nw != s->w || nh != s->h) { s->w = nw; s->h = nh; w32_push(s, 2, nw, nh); }
        return 0;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        { MSG nx; if (PeekMessageW(&nx, hwnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE) && w32_is_altgr_ctrl(msg, wp, lp, GetMessageTime(), &nx)) { s->altgr = 1; return 0; } }
        int ks = w32_named_keysym(wp);
        /* right Alt as AltGr is ISO_Level3_Shift, as X11 names it, which is a level, not a chord modifier, so
         * the shell's held-modifier tracking does not count it as Alt. "As AltGr" is known from the
         * message just dropped above, not from GetKeyState: that is the pair, and nothing else is. */
        if (wp == VK_MENU && (HIWORD(lp) & KF_EXTENDED) && s->altgr) ks = 0xfe03;
        if (!ks) ks = w32_printable_keysym(wp, lp);
        if (ks) w32_push(s, 3, ks, w32_mods());
        return 0;
    }
    case WM_KEYUP: case WM_SYSKEYUP: {
        { MSG nx; if (PeekMessageW(&nx, hwnd, WM_KEYFIRST, WM_KEYLAST, PM_NOREMOVE) && w32_is_altgr_ctrl(msg, wp, lp, GetMessageTime(), &nx)) return 0; }
        int ks = w32_named_keysym(wp);
        if (wp == VK_MENU && (HIWORD(lp) & KF_EXTENDED) && s->altgr) { ks = 0xfe03; s->altgr = 0; }
        if (!ks) ks = w32_printable_keysym(wp, lp);
        if (ks) w32_push(s, 4, ks, w32_mods());
        return 0;
    }
    case WM_MOUSEMOVE:
        s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp)); return 0;
    /* Every press and release is queued as its own event (7 = down, 8 = up; a = button, b,c = the
     * position it happened at), because the button state alone loses a click that begins and ends
     * between two polls. The state fields stay for the polled API, with the same latch. */
    case WM_LBUTTONDOWN: s->btn = 1; s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp));
                         s->lx = s->px; s->ly = s->py; s->tap_latch = 1;
                         w32_push3(s, 7, 1, s->px, s->py); return 0;
    case WM_LBUTTONUP:   s->btn = 0; s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp));
                         w32_push3(s, 8, 1, s->px, s->py); return 0;
    case WM_MBUTTONDOWN: s->mid = 1; s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp));
                         w32_push3(s, 7, 2, s->px, s->py); return 0;
    case WM_MBUTTONUP:   s->mid = 0; s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp));
                         w32_push3(s, 8, 2, s->px, s->py); return 0;
    case WM_RBUTTONDOWN: s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp));
                         w32_push3(s, 7, 3, s->px, s->py); return 0;
    case WM_RBUTTONUP:   s->px = w32_lg(s, GET_X_LPARAM(lp)); s->py = w32_lg(s, GET_Y_LPARAM(lp));
                         w32_push3(s, 8, 3, s->px, s->py); return 0;
    case WM_MOUSEWHEEL: {
        /* One notch is WHEEL_DELTA; a precision wheel sends fractions, so the remainder is carried
         * rather than truncated to zero on every message. Sign matches the other backends: down is
         * positive, and Windows reports the opposite. */
        s->scroll_accum += -GET_WHEEL_DELTA_WPARAM(wp);
        int notches = s->scroll_accum / WHEEL_DELTA;
        if (notches) { s->scroll_accum -= notches * WHEEL_DELTA; w32_push(s, 6, notches, 0); }
        return 0;
    }
    case WM_DROPFILES: {
        HDROP hd = (HDROP)wp;
        UINT n = DragQueryFileW(hd, 0xFFFFFFFF, 0, 0);
        size_t cap = 1; char* acc = (char*)calloc(cap, 1);
        for (UINT i = 0; i < n && acc; i++) {
            WCHAR path[MAX_PATH];
            if (!DragQueryFileW(hd, i, path, MAX_PATH)) continue;
            char* u8 = w32_wide_to_utf8(path);
            if (!u8) continue;
            size_t add = strlen(u8) + 8 + 1;
            char* grown = (char*)realloc(acc, cap + add);
            if (!grown) { free(u8); break; }
            acc = grown;
            strcat(acc, "file://"); strcat(acc, u8); strcat(acc, "\n");
            cap += add; free(u8);
        }
        DragFinish(hd);
        free(s->drop); s->drop = acc;
        w32_push(s, 5, 0, 0);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;                       /* the app paints every pixel; erasing first only flickers */
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* Declare DPI awareness, before opening anything.
 *
 * A process that does not declare DPI awareness is told by Windows on a scaled display that the
 * screen is 96 DPI; it draws at that size, and the compositor stretches the result with a
 * bilinear filter to fill the real pixels. Every glyph comes out soft, whichever renderer drew it,
 * because the blur is applied afterwards. It also makes `nori_win_scale120` useless: GetDeviceCaps
 * answers with the virtualised 96 too, so the UI cannot scale itself.
 *
 * Requested at run time rather than linked: Per-Monitor V2 is Windows 10 1703 and later, the shcore
 * form is 8.1, and the old user32 one is Vista. Each is tried in turn and any of them is better than
 * the stretch. Nothing is required; an older Windows keeps the unaware behaviour. */
static void w32_declare_dpi_aware(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    HMODULE u32 = LoadLibraryA("user32.dll");
    if (u32) {
        typedef BOOL (WINAPI *SetCtxFn)(void*);
        SetCtxFn setctx = (SetCtxFn)(void*)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4 */
        if (setctx && setctx((void*)(intptr_t)-4)) return;
    }
    HMODULE sh = LoadLibraryA("shcore.dll");
    if (sh) {
        typedef HRESULT (WINAPI *SetAwFn)(int);
        SetAwFn setaw = (SetAwFn)(void*)GetProcAddress(sh, "SetProcessDpiAwareness");
        if (setaw && setaw(2) == S_OK) return;      /* PROCESS_PER_MONITOR_DPI_AWARE */
    }
    if (u32) {
        typedef BOOL (WINAPI *SetOldFn)(void);
        SetOldFn setold = (SetOldFn)(void*)GetProcAddress(u32, "SetProcessDPIAware");
        if (setold) setold();
    }
}

/* The window lives on its own thread, which is required rather than a preference.
 *
 * A Win32 message queue belongs to the thread that created the window: only that thread can receive
 * its messages, and a window whose messages nobody receives is one Windows marks "not responding"
 * and stops redrawing. The render loop above this backend is a coroutine, so it is resumed on
 * whichever worker thread the scheduler picks. After the first suspension it is generally not the
 * thread that opened the window, and every message would sit in a queue nobody was reading.
 *
 * So the window is created here, on a thread of its own, which does nothing but receive messages and
 * push what they mean into the queue below. Any thread can then poll that queue, present a frame, or
 * ask for the size; the pointer, the keyboard and the close button keep working while the loop is
 * busy fetching a page.
 */
static DWORD WINAPI w32_win_thread(LPVOID arg) {
    NoriW32* s = (NoriW32*)arg;
    w32_declare_dpi_aware();
    HINSTANCE hi = GetModuleHandleW(NULL);
    WNDCLASSW wc; memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = w32_proc;
    wc.hInstance = hi;
    wc.hCursor = NULL;   /* we answer WM_SETCURSOR ourselves; a class cursor would overwrite it */
    wc.lpszClassName = L"NoriWindowClass";
    RegisterClassW(&wc);                /* a second window reuses the registration; failure is fine */

    /* Size the client area to what was asked for: CreateWindow's w/h include the frame, so without
     * this the drawable is smaller than requested by however thick the border happens to be. */
    RECT r; r.left = 0; r.top = 0; r.right = s->startw; r.bottom = s->starth;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowExW(0, L"NoriWindowClass", s->title, WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                r.right - r.left, r.bottom - r.top, NULL, NULL, hi, NULL);
    if (hwnd) {
        s->hwnd = hwnd;
        /* the DPI this window opened on; scale120 keeps it current from then on */
        { HDC dc0 = GetDC(hwnd); s->dpi = dc0 ? GetDeviceCaps(dc0, LOGPIXELSX) : 96; if (dc0) ReleaseDC(hwnd, dc0); }
        if (s->dpi <= 0) s->dpi = 96;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        DragAcceptFiles(hwnd, TRUE);
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
    } else {
        s->closed = 1;
    }
    SetEvent(s->ready);
    if (!hwnd) return 0;

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    s->closed = 1;
    SetEvent(s->wake);                  /* a loop asleep in frame_wait learns the window is gone */
    return 0;
}

void* nori_win_open(const char* title, int w, int h) {
    NoriW32* s = (NoriW32*)calloc(1, sizeof(NoriW32));
    if (!s) return 0;
    s->w = w; s->h = h; s->px = -1; s->py = -1; s->dpi = 96;
    s->startw = w; s->starth = h;
    if (title) MultiByteToWideChar(CP_UTF8, 0, title, -1, s->title, 512);
    InitializeCriticalSection(&s->qlock);
    s->ready = CreateEventW(NULL, TRUE, FALSE, NULL);        /* manual reset: read once, by open */
    s->wake  = CreateEventW(NULL, FALSE, FALSE, NULL);       /* auto reset: one sleeper at a time */
    if (!s->ready || !s->wake) { DeleteCriticalSection(&s->qlock); free(s); return 0; }
    s->thread = CreateThread(NULL, 0, w32_win_thread, s, 0, &s->tid);
    if (!s->thread) { CloseHandle(s->ready); CloseHandle(s->wake); DeleteCriticalSection(&s->qlock); free(s); return 0; }
    WaitForSingleObject(s->ready, INFINITE);
    if (!s->hwnd) {
        WaitForSingleObject(s->thread, 1000);
        CloseHandle(s->thread); CloseHandle(s->ready); CloseHandle(s->wake);
        DeleteCriticalSection(&s->qlock); free(s);
        return 0;
    }
    return s;
}

int nori_win_backend(void* h) { (void)h; return BK_WIN32; }

/* `out` must have room for three ints: the mouse events fill (button, x, y). */
int nori_win_poll_event(void* h, int* out) {
    NoriW32* s = (NoriW32*)h;
    if (!s) return 1;
    out[0] = 0; out[1] = 0; out[2] = 0;
    int type = 0;
    EnterCriticalSection(&s->qlock);
    if (s->qhead != s->qtail) {
        WEv e = s->q[s->qhead];
        s->qhead = (s->qhead + 1) % EVQ;
        out[0] = e.a; out[1] = e.b; out[2] = e.c;
        type = e.type;
    }
    LeaveCriticalSection(&s->qlock);
    return type;
}

/* A press no polled read has seen yet is latched (as on every other backend): a click shorter than
   a frame is reported once, at the position it was pressed, instead of being lost entirely. */
static int w32_take_down(NoriW32* s) {
    if (s->btn) { s->tap_latch = 0; return 1; }
    if (s->tap_latch) { s->tap_latch = 0; return 1; }
    return 0;
}
static int w32_px(NoriW32* s) { return s->tap_latch ? s->lx : s->px; }
static int w32_py(NoriW32* s) { return s->tap_latch ? s->ly : s->py; }

int nori_win_mouse(void* h, int* out_xy) {
    NoriW32* s = (NoriW32*)h;
    if (!s) { out_xy[0] = -1; out_xy[1] = -1; return 0; }
    out_xy[0] = w32_px(s); out_xy[1] = w32_py(s); return w32_take_down(s);
}
/* Windows lets an application move the cursor, so pointer lock is the X11 shape: hide it, confine it
 * to the window, and warp it back to the centre after every read — the distance it got is the delta,
 * and it can never reach an edge. ClipCursor alone would stop at the border and report nothing. */
int nori_win_lock_pointer(void* h, int on) {
    NoriW32* s = (NoriW32*)h;
    if (!s || !s->hwnd) return 0;
    if (on) {
        RECT r;
        if (!GetClientRect(s->hwnd, &r)) return 0;
        POINT tl = { r.left, r.top }, br = { r.right, r.bottom };
        ClientToScreen(s->hwnd, &tl); ClientToScreen(s->hwnd, &br);
        RECT scr = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&scr);
        if (!s->locked) ShowCursor(FALSE);
        s->warp_x = s->w / 2; s->warp_y = s->h / 2;
        POINT c = { s->warp_x, s->warp_y };
        ClientToScreen(s->hwnd, &c);
        SetCursorPos(c.x, c.y);
        s->px = s->warp_x; s->py = s->warp_y;
        s->locked = 1;
        return 1;
    }
    if (s->locked) { ClipCursor(NULL); ShowCursor(TRUE); s->locked = 0; }
    return 1;
}
int nori_win_inhibit_shortcuts(void* h, int on) { (void)h; (void)on; return 0; }
int nori_win_pointer_locked(void* h) { NoriW32* s = (NoriW32*)h; return s && s->locked; }
/* The delta since the last call, then re-centre. Read and warp together, so the next read sees only
 * real movement rather than the correction this one made. Units are 1/256 px, as the seam says. */
int nori_win_pointer_delta(void* h, int* out_dxdy) {
    NoriW32* s = (NoriW32*)h;
    if (!s || !s->locked) { out_dxdy[0] = 0; out_dxdy[1] = 0; return 0; }
    out_dxdy[0] = (s->px - s->warp_x) * 256;
    out_dxdy[1] = (s->py - s->warp_y) * 256;
    s->warp_x = s->w / 2; s->warp_y = s->h / 2;
    POINT c = { s->warp_x, s->warp_y };
    ClientToScreen(s->hwnd, &c);
    SetCursorPos(c.x, c.y);
    s->px = s->warp_x; s->py = s->warp_y;
    return 1;
}
int nori_win_mouse_x(void* h)      { NoriW32* s = (NoriW32*)h; return s ? w32_px(s) : -1; }
int nori_win_mouse_y(void* h)      { NoriW32* s = (NoriW32*)h; return s ? w32_py(s) : -1; }
int nori_win_pointer_x(void* h)    { return nori_win_mouse_x(h); }   /* no tap is latched here */
int nori_win_pointer_y(void* h)    { return nori_win_mouse_y(h); }
int nori_win_mouse_down(void* h)   { NoriW32* s = (NoriW32*)h; return s ? w32_take_down(s) : 0; }
/* The button state IS the held state here: a mouse reports press and release, so nothing
   has to be latched to notice a drag. */
int nori_win_pointer_held(void* h)   { NoriW32* s = (NoriW32*)h; return s ? s->btn :  0; }
int nori_win_mouse_middle(void* h) { NoriW32* s = (NoriW32*)h; return s ? s->mid :  0; }

/* 0x00RRGGBB is what a 32bpp BI_RGB DIB already means on a little-endian machine, so the caller's
 * framebuffer goes to GDI untouched. biHeight is negated because the canvas is top-down. */
void nori_win_shm_present(void* h, void* pixels, int ww, int hh) {
    NoriW32* s = (NoriW32*)h;
    if (!s || !pixels || ww <= 0 || hh <= 0) return;
    BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = ww;
    bi.bmiHeader.biHeight = -hh;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = GetDC(s->hwnd);
    if (!dc) return;
    SetDIBitsToDevice(dc, 0, 0, ww, hh, 0, 0, 0, hh, pixels, &bi, DIB_RGB_COLORS);
    ReleaseDC(s->hwnd, dc);
}

/* No frame callback to synchronise to, so sleep the caller's budget (enough to keep a render loop
 * off a busy spin without claiming to be vsynced) and wake early when the window thread queues
 * something, so input is answered on the frame it arrives rather than the one after. */
void nori_win_frame_wait(void* h, int timeout_ms) {
    NoriW32* s = (NoriW32*)h;
    if (!s || timeout_ms <= 0) return;
    int pending;
    EnterCriticalSection(&s->qlock);
    pending = s->qhead != s->qtail;
    LeaveCriticalSection(&s->qlock);
    if (pending) return;
    WaitForSingleObject(s->wake, (DWORD)timeout_ms);
}

int nori_win_scale120(void* h) {
    NoriW32* s = (NoriW32*)h;
    if (!s) return 120;
    HDC dc = GetDC(s->hwnd);
    if (!dc) return 120;
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(s->hwnd, dc);
    if (dpi <= 0) return 120;
    /* …and remember it, because w32_lg needs the same number to turn a click back into logical
     * pixels. The renderer asks for the scale every frame, so this follows a window dragged onto a
     * second monitor without a WM_DPICHANGED handler of its own. */
    s->dpi = dpi;
    int v = (int)(((double)dpi / 96.0) * 120.0 + 0.5);
    return v < 120 ? 120 : v;
}

int nori_win_closed(void* h) { NoriW32* s = (NoriW32*)h; return s ? s->closed : 1; }
/* Windows does not revoke a window's surface while the program runs, so this is "still open". */
int nori_win_drawable(void* h) { NoriW32* s = (NoriW32*)h; return s ? !s->closed : 0; }
int nori_win_w(void* h)      { NoriW32* s = (NoriW32*)h; return s ? s->w : 0; }
int nori_win_h(void* h)      { NoriW32* s = (NoriW32*)h; return s ? s->h : 0; }

/* No display connection on Windows; the HWND is the handle a GPU surface is built from. */
void* nori_win_display(void* h) { (void)h; return 0; }
void* nori_win_surface(void* h) { NoriW32* s = (NoriW32*)h; return s ? (void*)s->hwnd : 0; }

void nori_win_clipboard_set(void* h, const char* text) {
    NoriW32* s = (NoriW32*)h;
    if (!s || !text) return;
    int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, 0, 0);
    if (n <= 0) return;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)n * sizeof(WCHAR));
    if (!g) return;
    WCHAR* p = (WCHAR*)GlobalLock(g);
    if (!p) { GlobalFree(g); return; }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, p, n);
    GlobalUnlock(g);
    if (!OpenClipboard(s->hwnd)) { GlobalFree(g); return; }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, g)) GlobalFree(g);  /* on success the clipboard owns it */
    CloseClipboard();
}

char* nori_win_clipboard_get(void* h) {
    NoriW32* s = (NoriW32*)h;
    if (!s) return 0;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return 0;
    if (!OpenClipboard(s->hwnd)) return 0;
    HANDLE g = GetClipboardData(CF_UNICODETEXT);
    char* out = 0;
    if (g) {
        WCHAR* p = (WCHAR*)GlobalLock(g);
        if (p) { out = w32_wide_to_utf8(p); GlobalUnlock(g); }
    }
    CloseClipboard();
    return out;
}

char* nori_win_drop_text(void* h) {
    NoriW32* s = (NoriW32*)h;
    if (!s || !s->drop) return 0;
    char* out = s->drop;                /* handed over; the caller frees, as on every backend */
    s->drop = 0;
    return out;
}

/* A desktop window owns its rectangle, the pointer hovers, and a wheel notch is four logical pixels. */
int  nori_win_inset_top(void* h)        { (void)h; return 0; }
int  nori_win_has_hover(void* h)        { (void)h; return 1; }
int  nori_win_scroll_unit_1000(void* h) { (void)h; return 4000; }
void nori_win_soft_keyboard(void* h, int show) { (void)h; (void)show; }
/* No system back gesture here; the Escape key is already an ordinary key event. */
int nori_win_catch_back(void* h, int on) { (void)h; (void)on; return 0; }

/* Pointer shape. Windows resets the cursor to the window class's on every WM_SETCURSOR, so the
 * shape is remembered and re-applied there rather than only being set once here. */
int nori_win_set_cursor(void* h, int shape) {
    NoriW32* s = (NoriW32*)h;
    if (!s) return 0;
    if (s->locked) return 0;              /* hidden for pointer lock; leave it hidden */
    if (shape < 0 || shape > 5) shape = 0;
    s->shape_cur = shape;
    SetCursor(win32_cursor_for(shape));
    return 1;
}
void nori_win_apply_viewport(void* h)          { (void)h; }

/* Starting a drag is OLE (DoDragDrop with an IDataObject), which is not implemented. These
 * report that nothing was dragged rather than approximating it. Receiving a drop works, through
 * WM_DROPFILES above. */
void nori_win_start_drag(void* h, const char* text) { (void)h; (void)text; }
int  nori_win_drag_finished(void* h)                { (void)h; return 0; }

/* The window belongs to another thread, so it is asked to go rather than destroyed from here:
   DestroyWindow only works on the owning thread. WM_CLOSE runs the same path the title bar's button
   does, the message loop ends, and the thread returns. */
void nori_win_close(void* h) {
    NoriW32* s = (NoriW32*)h;
    if (!s) return;
    if (s->locked) { ClipCursor(NULL); ShowCursor(TRUE); s->locked = 0; }   /* the clip is system-wide; it outlives the window */
    if (s->hwnd) PostMessageW(s->hwnd, WM_CLOSE, 0, 0);
    if (s->thread) { WaitForSingleObject(s->thread, 2000); CloseHandle(s->thread); }
    CloseHandle(s->ready); CloseHandle(s->wake);
    DeleteCriticalSection(&s->qlock);
    free(s->drop);
    free(s);
}

/* The desktop shell's half (std/window/window_shell.nori) is served by the native build only
 * (__native_wayland_shell.nori): the C Wayland shim would need the layer-shell and foreign-toplevel
 * protocols' generated code, and Windows has no such protocols. Every call answers "not available" — the answer a compositor
 * without the protocols gets — and nori_win_fd offers no descriptor: poll the window instead. */
int   nori_win_fd(void* h) { (void)h; return -1; }
int   nori_win_dispatch(void* h) { return h ? 1 : -1; }   /* no descriptor to sleep on: always "poll it" */
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
