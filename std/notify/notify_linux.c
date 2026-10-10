/* Desktop notifications on Linux, through libnotify, loaded at runtime and never linked.
 *
 * A notification is the one thing in this library that another program draws. The desktop's shell
 * owns the popup: its style, its position, how long it lives, whether it appears at all. So the
 * honest interface is small (say what happened, get back a handle to replace or withdraw it) and
 * everything else is the shell's business.
 *
 * dlopen is used rather than -lnotify. Linking it would mean a program that merely might
 * notify cannot start on a machine without the library, and a headless box is exactly where that
 * happens. Loaded this way the answer to "can I notify?" is a runtime question with a real answer,
 * which is what nori_notify_available reports. dlsym reaches the library's dependencies too, so
 * g_object_unref comes through the same handle without a second dlopen on glib.
 */
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>

#define SLOTS 64

typedef int   (*fn_init)(const char*);
typedef void* (*fn_new)(const char*, const char*, const char*);
typedef int   (*fn_show)(void*, void**);
typedef int   (*fn_update)(void*, const char*, const char*, const char*);
typedef int   (*fn_close)(void*, void**);
typedef void  (*fn_unref)(void*);
typedef void* (*fn_caps)(void);
typedef void  (*fn_listfree)(void*, void*);

static struct {
    void* lib;
    int   tried, ready;
    fn_init init; fn_new mk; fn_show show; fn_update update; fn_close close; fn_unref unref;
    fn_caps caps; fn_listfree listfree; void* gfree;
    int   probed, reachable;
    char  app[96];
} g = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "nori" };

/* A live notification and the tag it answers to. `gen` is what keeps a recycled slot from being
   cancelled by the id of the notification that used to live in it. */
static struct { void* obj; char tag[64]; int gen; } g_slot[SLOTS];
static int g_next;                       /* round-robin, for when nothing is free */

static void say(char* err, int n, const char* msg) {
    if (err && n > 0) { strncpy(err, msg, (size_t)n - 1); err[n - 1] = 0; }
}

/* GError is { GQuark domain; gint code; gchar* message; }: the message pointer sits after two
   32-bit fields, padded. Worth reading: "no notification daemon" and "connection refused" are
   different problems for whoever is looking at the log. */
static const char* gerr_msg(void* e) {
    if (!e) return 0;
    char* p = (char*)e;
    char* m = *(char**)(p + 8);
    return m;
}

static int load(void) {
    if (g.tried) return g.ready;
    g.tried = 1;
    g.lib = dlopen("libnotify.so.4", RTLD_LAZY);
    if (!g.lib) g.lib = dlopen("libnotify.so", RTLD_LAZY);
    if (!g.lib) return 0;
    g.init   = (fn_init)  dlsym(g.lib, "notify_init");
    g.mk     = (fn_new)   dlsym(g.lib, "notify_notification_new");
    g.show   = (fn_show)  dlsym(g.lib, "notify_notification_show");
    g.update = (fn_update)dlsym(g.lib, "notify_notification_update");
    g.close  = (fn_close) dlsym(g.lib, "notify_notification_close");
    g.unref  = (fn_unref) dlsym(g.lib, "g_object_unref");
    g.caps     = (fn_caps)    dlsym(g.lib, "notify_get_server_caps");
    g.listfree = (fn_listfree)dlsym(g.lib, "g_list_free_full");
    g.gfree    = dlsym(g.lib, "g_free");
    if (!g.init || !g.mk || !g.show) return 0;
    if (!g.init(g.app)) return 0;        /* no session bus, or no name to register under */
    g.ready = 1;
    return 1;
}

void nori_notify_set_app(const char* name) {
    if (!name || !*name) return;
    strncpy(g.app, name, sizeof(g.app) - 1); g.app[sizeof(g.app) - 1] = 0;
    /* Before the first send this simply chooses the name; after it, libnotify has already
       registered and the new name applies to whatever the next init would be. */
}

/* More than "is the library here". notify_init registers a name and touches no bus at all, so a
   headless machine passes it happily and then drops every notification — available() would be
   answering a question nobody asked. notify_get_server_caps makes a real call to the daemon and
   comes back empty when there is nobody to answer, which is the thing a caller wants to know
   before it offers someone a "notify me" setting it cannot honour.
   Cached: it costs a round trip, and send() does not need it — that reports what the daemon
   actually said. Asking here is also what makes libnotify print its one warning on a machine with
   no bus, which is why send() does not ask. */
int nori_notify_available(void) {
    if (!load()) return 0;
    if (g.probed) return g.reachable;
    g.probed = 1;
    if (!g.caps) { g.reachable = 1; return 1; }   /* too old to ask: assume, rather than deny */
    void* l = g.caps();
    g.reachable = l ? 1 : 0;
    if (l && g.listfree && g.gfree) g.listfree(l, g.gfree);
    return g.reachable;
}

/* Returns an id > 0, or 0 with `err` filled. The id encodes the slot and its generation. */
int nori_notify_send(const char* title, const char* body, const char* tag, char* err, int errlen) {
    if (!load()) { say(err, errlen, "no notification service (libnotify absent, or no session bus)"); return 0; }
    if (!title) title = "";
    if (!body)  body  = "";

    int idx = -1;
    if (tag && *tag) {
        for (int i = 0; i < SLOTS; i++)
            if (g_slot[i].obj && strncmp(g_slot[i].tag, tag, sizeof(g_slot[i].tag) - 1) == 0) { idx = i; break; }
    }
    if (idx < 0) {
        for (int i = 0; i < SLOTS; i++) if (!g_slot[i].obj) { idx = i; break; }
    }
    if (idx < 0) {
        /* Every slot is taken. The oldest popup is long gone from the screen (the object is just a
           handle), so recycle it, and bump the generation so its old id cancels nothing. */
        idx = g_next % SLOTS; g_next++;
        g_slot[idx].gen++;
        if (g_slot[idx].obj && g.close) g.close(g_slot[idx].obj, 0);
    }

    void* e = 0;
    if (g_slot[idx].obj) {
        if (g.update) g.update(g_slot[idx].obj, title, body, "dialog-information");
    } else {
        g_slot[idx].obj = g.mk(title, body, "dialog-information");
        if (!g_slot[idx].obj) { say(err, errlen, "libnotify would not build the notification"); return 0; }
    }
    if (tag && *tag) { strncpy(g_slot[idx].tag, tag, sizeof(g_slot[idx].tag) - 1); g_slot[idx].tag[sizeof(g_slot[idx].tag) - 1] = 0; }
    else g_slot[idx].tag[0] = 0;

    if (!g.show(g_slot[idx].obj, &e)) {
        const char* m = gerr_msg(e);
        say(err, errlen, m ? m : "the notification service refused it");
        return 0;
    }
    return g_slot[idx].gen * 256 + (idx + 1);
}

int nori_notify_cancel(int id) {
    if (id <= 0 || !g.ready) return 0;
    int idx = (id % 256) - 1;
    int gen = id / 256;
    if (idx < 0 || idx >= SLOTS) return 0;
    if (!g_slot[idx].obj || g_slot[idx].gen != gen) return 0;   /* already recycled: not yours */
    if (g.close) g.close(g_slot[idx].obj, 0);
    if (g.unref) g.unref(g_slot[idx].obj);
    g_slot[idx].obj = 0; g_slot[idx].tag[0] = 0; g_slot[idx].gen++;
    return 1;
}
