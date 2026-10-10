/* Notifications on Windows, as a tray balloon (Shell_NotifyIcon with NIF_INFO).
 *
 * Not a modern "toast". Toasts are a WinRT API keyed to an AppUserModelID, and an AUMID only
 * resolves for a program installed with a Start-menu shortcut that carries it. A single .exe run
 * from anywhere has no such identity, so the toast API accepts the call and shows nothing. The
 * balloon is what a lone executable can do: it is older, it is plainer, and it appears.
 *
 * The tray icon exists only to carry the balloon; it is added on the first send and removed at
 * exit. A hidden message-only window owns it because Shell_NotifyIcon requires an HWND, which is
 * not a reason for this file to know about std/window. A notification is not a window, and the
 * window here is an implementation detail the caller never sees or waits on.
 */
#include <windows.h>
#include <shellapi.h>
#include <string.h>

static struct {
    HWND hwnd;
    int  added;
    int  next_id;
    char app[96];
    char tag[64];        /* the tag currently on screen: a repeat replaces the balloon */
    int  tag_id;
} g = { 0, 0, 1, "nori", "", 0 };

static void say(char* err, int n, const char* msg) {
    if (err && n > 0) { strncpy(err, msg, (size_t)n - 1); err[n - 1] = 0; }
}

static NOTIFYICONDATAA nid_base(void) {
    NOTIFYICONDATAA n;
    memset(&n, 0, sizeof(n));
    n.cbSize = sizeof(n);
    n.hWnd   = g.hwnd;
    n.uID    = 1;
    return n;
}

/* A message-only window: never shown, never painted, no message loop needed; Shell_NotifyIcon
   only wants something to hang the icon on. */
static int ensure_window(void) {
    if (g.hwnd) return 1;
    WNDCLASSA wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = GetModuleHandleA(0);
    wc.lpszClassName = "NoriNotifySink";
    RegisterClassA(&wc);           /* a second call fails harmlessly with CLASS_ALREADY_EXISTS */
    g.hwnd = CreateWindowExA(0, "NoriNotifySink", "", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, wc.hInstance, 0);
    return g.hwnd ? 1 : 0;
}

static int ensure_icon(void) {
    if (g.added) return 1;
    if (!ensure_window()) return 0;
    NOTIFYICONDATAA n = nid_base();
    n.uFlags = NIF_ICON | NIF_TIP;
    n.hIcon  = LoadIconA(0, IDI_INFORMATION);
    strncpy(n.szTip, g.app, sizeof(n.szTip) - 1);
    if (!Shell_NotifyIconA(NIM_ADD, &n)) return 0;
    g.added = 1;
    return 1;
}

void nori_notify_set_app(const char* name) {
    if (!name || !*name) return;
    strncpy(g.app, name, sizeof(g.app) - 1); g.app[sizeof(g.app) - 1] = 0;
}

/* There is no asking the shell whether balloons are switched on (the setting is per-user and not
   readable in any supported way), so this answers whether the tray will take an icon at all. */
int nori_notify_available(void) { return ensure_icon() ? 1 : 0; }

int nori_notify_send(const char* title, const char* body, const char* tag, char* err, int errlen) {
    if (!ensure_icon()) { say(err, errlen, "the notification area would not accept an icon"); return 0; }
    NOTIFYICONDATAA n = nid_base();
    n.uFlags = NIF_INFO;
    n.dwInfoFlags = NIIF_INFO;
    strncpy(n.szInfoTitle, title ? title : "", sizeof(n.szInfoTitle) - 1);
    strncpy(n.szInfo,      body  ? body  : "", sizeof(n.szInfo) - 1);
    if (!Shell_NotifyIconA(NIM_MODIFY, &n)) { say(err, errlen, "the shell refused the balloon"); return 0; }

    /* One icon means one balloon at a time: a new one replaces whatever is showing, tagged or not.
       So a tag only decides which ID comes back, and repeating a tag returns the same one. */
    if (tag && *tag) {
        if (strncmp(g.tag, tag, sizeof(g.tag) - 1) != 0) {
            strncpy(g.tag, tag, sizeof(g.tag) - 1); g.tag[sizeof(g.tag) - 1] = 0;
            g.tag_id = g.next_id++;
        }
        return g.tag_id;
    }
    g.tag[0] = 0;
    return g.next_id++;
}

/* Withdrawing a balloon means taking the icon away; there is no per-balloon handle to revoke. */
int nori_notify_cancel(int id) {
    if (id <= 0 || !g.added) return 0;
    NOTIFYICONDATAA n = nid_base();
    int ok = Shell_NotifyIconA(NIM_DELETE, &n) ? 1 : 0;
    g.added = 0; g.tag[0] = 0;
    return ok;
}
