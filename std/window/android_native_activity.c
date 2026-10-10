/* android_native_activity.c — the Android app entry point, wiring an activity to a Nori program.
 *
 * NativeActivity rather than a Java shim: the APK then needs no Java at all, which removes javac and
 * d8 from the build entirely — an AndroidManifest naming android.app.NativeActivity plus this .so is
 * a complete application. The cost is that the soft keyboard and the clipboard live behind JNI and
 * stay out of reach (see the stubs in window_android.c); a Java shim is the answer when those are
 * wanted, and it can reuse every entry point below unchanged.
 *
 * The main loop is inverted here, and that is the whole point of the file. A Nori program runs
 * `main` to completion; Android instead calls an app back and expects each callback to return
 * promptly, because they all run on the UI thread and blocking one freezes the application. So the
 * Nori program gets a thread of its own and keeps its ordinary shape — open a window, loop, render —
 * while the callbacks below only deposit state for it to find. Nothing about the Nori side changes.
 *
 * Link:  --cfile std/window/android_native_activity.c --cfile std/window/window_android.c
 *        --clink android --clink log
 */
#include <android/native_activity.h>
#include <android/configuration.h>
#include <jni.h>
#include <android/looper.h>
#include <android/input.h>
#include <android/log.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "nori", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "nori", __VA_ARGS__)

/* the Nori program's entry, the same one _start calls in a standalone build */
extern int nori_enter_main(int argc, char** argv);   /* refuses (-1) if a program is already running here */

/* window_android.c — the only route from an activity callback into the window backend */
extern void nori_android_surface_created(void* nativeWindow);
extern void nori_android_surface_resized(int w, int h);
extern void nori_android_surface_destroyed(void);
extern void nori_android_finishing(void);
extern void nori_android_set_density(int dpi);
extern void nori_android_touch(int action, int x, int y);
extern void nori_android_key(int down, int keysym, int mods);
extern int  nori_android_keysym_of(int keycode);
extern int  nori_android_unicode_of(int keycode, int meta);

static pthread_t g_nori_thread;
static int       g_nori_started = 0;

/* ---- stdout/stderr -> logcat ----------------------------------------------------------------
 * Android throws away an app's stdout. Without this, `printl` in a Nori program writes into
 * nothing, and the only way to see what a program did is to make it draw or send the answer
 * somewhere, which is a poor way to debug the thing that is failing to send answers.
 *
 * A pipe replaces both descriptors and a thread reads it a line at a time into the log. Lines
 * longer than the buffer are split rather than dropped, because the interesting part of a long
 * line is usually the end of it. */
static pthread_t g_log_thread;
static int       g_log_pipe[2];

/* ---- log capture ----------------------------------------------------------------------------- */
/* Everything the program writes also lands in a ring buffer the program itself can read back. On a
   device with no adb this is the only way to see output that a crash would otherwise take with it —
   most usefully a sanitizer's report, which is written to stderr moments before it aborts. */
#define NORI_LOGCAP_SZ 8192
static char g_logcap[NORI_LOGCAP_SZ];
static volatile int g_logcap_len = 0;

static void nori_logcap(const char* line) {
    int n = (int)strlen(line);
    if (n > NORI_LOGCAP_SZ - 2) n = NORI_LOGCAP_SZ - 2;
    if (g_logcap_len + n + 1 >= NORI_LOGCAP_SZ) {
        /* keep the tail: a sanitizer puts its verdict first but the frames after it are the point */
        int drop = g_logcap_len + n + 1 - NORI_LOGCAP_SZ;
        if (drop > g_logcap_len) drop = g_logcap_len;
        memmove(g_logcap, g_logcap + drop, (size_t)(g_logcap_len - drop));
        g_logcap_len -= drop;
    }
    memcpy(g_logcap + g_logcap_len, line, (size_t)n);
    g_logcap_len += n;
    g_logcap[g_logcap_len++] = '\n';
}
long long nori_logcap_len(void) { return g_logcap_len; }
long long nori_logcap_byte(long long i) {
    if (i < 0 || i >= g_logcap_len) return 0;
    return (unsigned char)g_logcap[i];
}

static void* log_pump(void* unused) {
    (void)unused;
    char buf[512];
    size_t used = 0;
    for (;;) {
        ssize_t n = read(g_log_pipe[0], buf + used, sizeof(buf) - used - 1);
        if (n <= 0) return 0;
        used += (size_t)n;
        buf[used] = 0;
        char* start = buf;
        for (;;) {
            char* nl = strchr(start, '\n');
            if (!nl) break;
            *nl = 0;
            __android_log_write(ANDROID_LOG_INFO, "nori", start);
            nori_logcap(start);
            start = nl + 1;
        }
        used = strlen(start);
        memmove(buf, start, used + 1);
        if (used == sizeof(buf) - 1) {          /* a line with no newline in sight: flush it */
            __android_log_write(ANDROID_LOG_INFO, "nori", buf);
            used = 0;
        }
    }
    return 0;
}

static void start_logging_once(void);
/* Also for a program with no activity. A woken job's printl would otherwise go to a stdout nobody
   is reading — the program runs correctly and appears, from outside, to have done nothing at all,
   which is exactly how the first background run looked. */
void nori_android_start_logging(void) { start_logging_once(); }

static void start_logging_once(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    setvbuf(stdout, 0, _IOLBF, 0);
    setvbuf(stderr, 0, _IONBF, 0);
    if (pipe(g_log_pipe) != 0) return;
    dup2(g_log_pipe[1], STDOUT_FILENO);
    dup2(g_log_pipe[1], STDERR_FILENO);
    pthread_create(&g_log_thread, NULL, log_pump, NULL);
}

static void* nori_main_thread(void* unused) {
    (void)unused;
    LOGI("nori: program starting");
    long long rc = nori_enter_main(0, 0);
    LOGI("nori: program returned %lld", rc);
    return 0;
}

/* Start the program once, and only once there is something to draw on. Starting it at onCreate
   instead would race: the Nori side opens a window immediately, and with no surface yet that call
   returns 0 and the program exits believing it has no display. Android destroys and recreates the
   surface every time the app is backgrounded, so this fires repeatedly — the guard is what keeps it
   one program rather than one per resume. */
static void start_nori_once(void) {
    if (g_nori_started) return;
    g_nori_started = 1;
    start_logging_once();
    if (pthread_create(&g_nori_thread, NULL, nori_main_thread, NULL) != 0) {
        LOGE("nori: could not start the program thread");
        g_nori_started = 0;
    }
}

/* ---- crash guard ----------------------------------------------------------------------------- */
/* An Android app cannot read its own tombstone: logcat needs READ_LOGS, which needs adb, which needs
   wi-fi. So a native crash on a device that is only ever on mobile data is invisible — the process
   disappears and nothing anywhere says why.

   This catches the fatal signals, records what the kernel reported, and longjmps back to the caller
   so the program can SAY what happened instead of vanishing. It is a debugging aid and it is honest
   about its limits: returning from a SIGSEGV means continuing on a heap that may already be
   inconsistent, so the guarded call is for finding a fault, never for surviving one. */
#include <stdlib.h>
#include <setjmp.h>
#include <ucontext.h>

static sigjmp_buf g_crash_jmp;
static volatile sig_atomic_t g_crash_armed = 0;
static volatile long g_crash_sig = 0, g_crash_addr = 0, g_crash_pc = 0;

static void nori_crash_handler(int sig, siginfo_t* info, void* uctx) {
    if (!g_crash_armed) { _exit(128 + sig); }   /* outside a guarded call: die as before */
    g_crash_armed = 0;
    g_crash_sig  = sig;
    g_crash_addr = (long)(info ? (long)(intptr_t)info->si_addr : 0);
#if defined(__aarch64__)
    if (uctx) g_crash_pc = (long)((ucontext_t*)uctx)->uc_mcontext.pc;
#elif defined(__x86_64__)
    if (uctx) g_crash_pc = (long)((ucontext_t*)uctx)->uc_mcontext.gregs[REG_RIP];
#endif
    siglongjmp(g_crash_jmp, 1);
}

/* Run fnp() with the fatal signals caught. 0 = returned normally, 1 = crashed. */
long long nori_guard_run(long long fnp) {
    /* An alternate stack, because SA_ONSTACK alone does nothing: without this the handler runs on the
       faulting stack, and the one fault that most needs catching — running out of stack — cannot then
       run a handler at all. Per thread, so it is installed here, on the thread that will crash. */
    static __thread char altstack[SIGSTKSZ * 4];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = sizeof altstack;
    ss.ss_flags = 0;
    sigaltstack(&ss, 0);

    struct sigaction sa, old_segv, old_bus, old_ill, old_sys, old_abrt, old_trap, old_fpe;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = nori_crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old_segv);
    sigaction(SIGBUS,  &sa, &old_bus);
    sigaction(SIGILL,  &sa, &old_ill);
    sigaction(SIGSYS,  &sa, &old_sys);
    /* SIGABRT is the one that matters most on Android and the one this first missed: bionic's
       allocator does not fault on a corrupt heap, it CALLS ABORT. A guard that watches only for bad
       addresses sees nothing and the process still disappears. SIGTRAP and SIGFPE come along because
       the toolchain's own checks raise them. */
    sigaction(SIGABRT, &sa, &old_abrt);
    sigaction(SIGTRAP, &sa, &old_trap);
    sigaction(SIGFPE,  &sa, &old_fpe);
    g_crash_sig = 0; g_crash_addr = 0; g_crash_pc = 0;
    long long rc;
    if (sigsetjmp(g_crash_jmp, 1) == 0) {
        g_crash_armed = 1;
        ((void (*)(void))(intptr_t)fnp)();
        rc = 0;
    } else {
        rc = 1;
    }
    g_crash_armed = 0;
    sigaction(SIGSEGV, &old_segv, 0);
    sigaction(SIGBUS,  &old_bus,  0);
    sigaction(SIGILL,  &old_ill,  0);
    sigaction(SIGSYS,  &old_sys,  0);
    sigaction(SIGABRT, &old_abrt, 0);
    sigaction(SIGTRAP, &old_trap, 0);
    sigaction(SIGFPE,  &old_fpe,  0);
    return rc;
}
long long nori_crash_sig(void)  { return g_crash_sig; }
long long nori_crash_addr(void) { return g_crash_addr; }
long long nori_crash_pc(void)   { return g_crash_pc; }
/* The load address of this library, so a PC can be turned back into a file offset a symbolizer can
   read. Without it the PC is a runtime address that means nothing off the device. */
#include <dlfcn.h>
long long nori_lib_base(void) {
    Dl_info di;
    if (dladdr((void*)(intptr_t)&nori_lib_base, &di) && di.dli_fbase) return (long long)(intptr_t)di.dli_fbase;
    return 0;
}

/* ---- input ---------------------------------------------------------------------------------- */
extern int nori_android_catch_back(void);

/* Read whatever the queue has. Returning 1 keeps the callback registered with the looper. */
static int input_cb(int fd, int events, void* data) {
    (void)fd; (void)events;
    AInputQueue* q = (AInputQueue*)data;
    AInputEvent* ev = NULL;
    while (AInputQueue_getEvent(q, &ev) >= 0) {
        /* the IME gets first refusal — skipping this swallows soft-keyboard input entirely */
        if (AInputQueue_preDispatchEvent(q, ev)) continue;
        int handled = 0;
        int type = AInputEvent_getType(ev);
        if (type == AINPUT_EVENT_TYPE_MOTION) {
            int action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
            nori_android_touch(action, (int)AMotionEvent_getX(ev, 0), (int)AMotionEvent_getY(ev, 0));
            handled = 1;
        } else if (type == AINPUT_EVENT_TYPE_KEY) {
            int code = AKeyEvent_getKeyCode(ev);
            int meta0 = AKeyEvent_getMetaState(ev);
            int sym  = nori_android_keysym_of(code);
            /* A printable key has no named keysym: the window API's convention is that its keysym is
               its codepoint. Without this every letter typed on the soft keyboard was dropped here,
               so the keyboard could be raised and still type nothing. */
            if (sym == 0) sym = nori_android_unicode_of(code, meta0);
            if (sym != 0) {
                int down = (AKeyEvent_getAction(ev) == AKEY_EVENT_ACTION_DOWN);
                int meta = AKeyEvent_getMetaState(ev);
                int mods = 0;
                if (meta & AMETA_SHIFT_ON) mods |= 1;
                if (meta & AMETA_CTRL_ON)  mods |= 2;
                if (meta & AMETA_ALT_ON)   mods |= 4;
                if (meta & AMETA_META_ON)  mods |= 8;
                nori_android_key(down, sym, mods);
                /* Back is reported either way, as Escape. Whether it is consumed is the program's
                   choice: unhandled, the system does what a person expects and leaves the activity;
                   handled, the activity stays and the Escape is the program's to act on. Consuming
                   it unasked would trap anyone in a program that ignores the key, so the default is
                   the system's -- see nori_sfc_catch_back. */
                handled = (code != AKEYCODE_BACK) || nori_android_catch_back();
            }
        }
        AInputQueue_finishEvent(q, ev, handled);
    }
    return 1;
}



/* ---- activity callbacks (all on the UI thread) ---------------------------------------------- */

static void on_window_created(ANativeActivity* a, ANativeWindow* w) {
    (void)a;
    nori_android_surface_created(w);
    start_nori_once();
}
static void on_window_resized(ANativeActivity* a, ANativeWindow* w) {
    (void)a;
    nori_android_surface_resized(ANativeWindow_getWidth(w), ANativeWindow_getHeight(w));
}
/* Not the end of the program. Android destroys the surface whenever the app goes to the background
   and hands over a fresh one on return, so the Nori side sees "cannot draw right now" and carries
   on; killing the thread here would make every task switch fatal. */
static void on_window_destroyed(ANativeActivity* a, ANativeWindow* w) {
    (void)a; (void)w;
    nori_android_surface_destroyed();
}
static void on_input_created(ANativeActivity* a, AInputQueue* q) {
    (void)a;
    AInputQueue_attachLooper(q, ALooper_forThread(), 0, input_cb, q);
}
static void on_input_destroyed(ANativeActivity* a, AInputQueue* q) {
    (void)a;
    AInputQueue_detachLooper(q);
}
static void on_destroy(ANativeActivity* a) {
    (void)a;
    nori_android_finishing();
}
static void on_low_memory(ANativeActivity* a) { (void)a; }
static void on_config_changed(ANativeActivity* a) { (void)a; }
/* State is not saved. A Nori program's state lives in its own heap on its own thread, and there is
   no general way to serialise that from here — so if the system kills the process in the background
   the program starts fresh. Returning NULL says so honestly rather than saving something partial;
   a program that needs to survive that should persist what matters itself. */
static void* on_save_state(ANativeActivity* a, size_t* outLen) { (void)a; *outLen = 0; return NULL; }

/* The ELF entry point Android's loader looks for. Everything above hangs off it. */
__attribute__((visibility("default")))
/* The activity, kept because showing the soft keyboard is a call ON it and the request arrives later
   from the program's own thread. */
static ANativeActivity* g_activity = NULL;

/* The activity itself, for a program that needs JNI of its own.
 *
 * Everything else in this file that touches JNI does so for something every app wants — the
 * keyboard, the insets. An app with a need this file does not know about (handing a downloaded APK
 * to the package installer, say) would otherwise have to fork the glue to reach `vm` and `clazz`,
 * which are the whole of what JNI needs to start. Returns NULL before the activity exists.
 *
 * Deliberately opaque: `void*` rather than ANativeActivity*, so a caller that does not include
 * <android/native_activity.h> still links, and one that does can cast. */
void* nori_android_activity(void) { return g_activity; }

/* Leave the activity, because the program asked to. window::close means "this program is done", and
   on Android done has to be said to the system: the native thread ending leaves the activity sitting
   there resumed, showing whatever was on the glass, with nothing drawing into it. Safe from any
   thread — finish() is posted to the UI thread — and only reached from nori_sfc_close, which is only
   reached from a program closing its own window.

   This is what makes a caught BACK able to end the way an uncaught one would: dismiss what there is
   to dismiss, and when there is nothing left, leave. */
void nori_android_finish(void) {
    if (g_activity) ANativeActivity_finish(g_activity);
}


/* ---- reaching JNI when there is no activity ---------------------------------------------------
 *
 * An app is not the only way this library gets loaded. The system can start a background job in a
 * process with no activity at all — nothing on screen, nothing created, g_activity still NULL —
 * and code in there still needs a JavaVM to call anything and a Context to call it on.
 *
 * The VM is captured in JNI_OnLoad, which the runtime calls on every System.loadLibrary, activity
 * or not. The Context is registered by whoever has one; an activity is preferred when there is
 * one, because it is the more capable Context and the one an app's own calls expect. */
static JavaVM*  g_vm  = NULL;
static jobject  g_ctx = NULL;          /* a global ref, owned by whoever registered it */

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)reserved;
    g_vm = vm;
    return JNI_VERSION_1_6;
}

/* The process's JavaVM, from the activity if there is one and from JNI_OnLoad otherwise. */
void* nori_android_vm(void) {
    if (g_activity && g_activity->vm) return g_activity->vm;
    return g_vm;
}

/* Register a Context for code running without an activity — a Service, say. Pass a global ref:
   a local one is dead the moment the JNI call that produced it returns. NULL clears it. */
void nori_android_set_context(void* ctx) { g_ctx = (jobject)ctx; }

/* The Context to make framework calls on: the activity when the app is up, otherwise whatever was
   registered. NULL when this process has neither, which is a real answer and not a failure. */
void* nori_android_context(void) {
    if (g_activity) return g_activity->clazz;
    return g_ctx;
}

/* Raise or dismiss the on-screen keyboard. ANativeActivity_showSoftInput is the NDK's own call and
   needs no JNI; it is a REQUEST, and the system may decline it (a hardware keyboard attached, or the
   window not focused), which is why nothing here waits for a result. */
/* Whether this program asked for the keyboard and has not asked for it to go away. Needed because
   the only call that RAISES one here is a toggle. */
static int g_kbd_shown = 0;

/* activity.getWindow().getDecorView().getWindowToken() — what hideSoftInputFromWindow identifies a
   window by. NULL if any step of it is not there, which is handled by not making the call. */
static jobject window_token(JNIEnv* env) {
    jclass acls = (*env)->GetObjectClass(env, g_activity->clazz);
    jmethodID gw = acls ? (*env)->GetMethodID(env, acls, "getWindow", "()Landroid/view/Window;") : NULL;
    jobject win = gw ? (*env)->CallObjectMethod(env, g_activity->clazz, gw) : NULL;
    if (!win) return NULL;
    jclass wcls = (*env)->GetObjectClass(env, win);
    jmethodID gdv = (*env)->GetMethodID(env, wcls, "getDecorView", "()Landroid/view/View;");
    jobject dv = gdv ? (*env)->CallObjectMethod(env, win, gdv) : NULL;
    if (!dv) return NULL;
    jclass vcls = (*env)->GetObjectClass(env, dv);
    jmethodID gwt = (*env)->GetMethodID(env, vcls, "getWindowToken", "()Landroid/os/IBinder;");
    return gwt ? (*env)->CallObjectMethod(env, dv, gwt) : NULL;
}

void nori_android_soft_keyboard(int show) {
    if (!g_activity || !g_activity->vm) return;

    /* ANativeActivity_showSoftInput is the obvious call and it does not work here. The IME will only
       talk to a "served" view — one that returns an InputConnection — and NativeActivity's content
       view returns none, so the request is refused before it reaches the keyboard:
           W InputMethodManager: Ignoring showSoftInput() as view=...NativeContentView... is not served
       toggleSoftInput asks the manager directly instead and needs no served view, which is why this
       goes through JNI rather than the NDK wrapper. */
    JNIEnv* env = NULL;
    if ((*g_activity->vm)->AttachCurrentThread(g_activity->vm, &env, NULL) != JNI_OK || !env) return;

    jclass ctx = (*env)->FindClass(env, "android/content/Context");
    jfieldID fid = ctx ? (*env)->GetStaticFieldID(env, ctx, "INPUT_METHOD_SERVICE", "Ljava/lang/String;") : NULL;
    jobject name = fid ? (*env)->GetStaticObjectField(env, ctx, fid) : NULL;
    jclass acls = (*env)->GetObjectClass(env, g_activity->clazz);
    jmethodID gss = acls ? (*env)->GetMethodID(env, acls, "getSystemService",
                                               "(Ljava/lang/String;)Ljava/lang/Object;") : NULL;
    jobject imm = (name && gss) ? (*env)->CallObjectMethod(env, g_activity->clazz, gss, name) : NULL;
    if (imm) {
        jclass icls = (*env)->GetObjectClass(env, imm);
        if (show) {
            /* toggleSoftInput is a toggle, and it is used only to raise: asking for it twice puts
               away the keyboard the app just asked for. g_kbd_shown is what stops that. SHOW_FORCED
               (2) because there is no served view for a gentler request to reach. */
            jmethodID tog = (*env)->GetMethodID(env, icls, "toggleSoftInput", "(II)V");
            if (tog && !g_kbd_shown) {
                (*env)->CallVoidMethod(env, imm, tog, 2, 0);
                g_kbd_shown = 1;
            }
        } else {
            /* And not toggleSoftInput(0, HIDE_IMPLICIT_ONLY). A keyboard raised with SHOW_FORCED is
               not implicit, so that flag refuses to put it away — which left the app with a keyboard
               it had raised itself and no way to lower, covering whatever was underneath.
               hideSoftInputFromWindow says it plainly, and needs the window's token rather than a
               served view. */
            jmethodID hide = (*env)->GetMethodID(env, icls, "hideSoftInputFromWindow",
                                                 "(Landroid/os/IBinder;I)Z");
            jobject token = window_token(env);
            if (hide && token) {
                (*env)->CallBooleanMethod(env, imm, hide, token, 0);
                g_kbd_shown = 0;
            }
        }
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*g_activity->vm)->DetachCurrentThread(g_activity->vm);
}

/* How far down the screen the app may safely draw. A phone puts the clock, battery and signal in a
   strip along the top, and the activity's surface is the WHOLE screen — so anything drawn at y=0
   lands underneath them. The NDK exposes no inset API at all; the value lives on the Java window, so
   this is a JNI hop like the keyboard is.
   Returns DEVICE pixels, or 0 when the answer is unavailable, which is also the right answer on a
   platform that has no such strip. */
int nori_android_inset_top(void) {
    if (!g_activity || !g_activity->vm) return 0;
    JNIEnv* env = NULL;
    if ((*g_activity->vm)->AttachCurrentThread(g_activity->vm, &env, NULL) != JNI_OK || !env) return 0;
    int top = 0;

    jclass acls = (*env)->GetObjectClass(env, g_activity->clazz);
    jmethodID getWindow = acls ? (*env)->GetMethodID(env, acls, "getWindow", "()Landroid/view/Window;") : NULL;
    jobject window = getWindow ? (*env)->CallObjectMethod(env, g_activity->clazz, getWindow) : NULL;
    jclass wcls = window ? (*env)->GetObjectClass(env, window) : NULL;
    jmethodID getDecor = wcls ? (*env)->GetMethodID(env, wcls, "getDecorView", "()Landroid/view/View;") : NULL;
    jobject decor = getDecor ? (*env)->CallObjectMethod(env, window, getDecor) : NULL;
    jclass vcls = decor ? (*env)->GetObjectClass(env, decor) : NULL;
    jmethodID getInsets = vcls ? (*env)->GetMethodID(env, vcls, "getRootWindowInsets",
                                                    "()Landroid/view/WindowInsets;") : NULL;
    jobject insets = getInsets ? (*env)->CallObjectMethod(env, decor, getInsets) : NULL;
    if (insets) {
        jclass icls = (*env)->GetObjectClass(env, insets);
        /* getSystemWindowInsetTop is deprecated but present everywhere this runs, and needs none of
           the Type constants the newer getInsets(int) call would have to look up first. */
        jmethodID topM = (*env)->GetMethodID(env, icls, "getSystemWindowInsetTop", "()I");
        if (topM) top = (int)(*env)->CallIntMethod(env, insets, topM);
    }
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); top = 0; }
    (*g_activity->vm)->DetachCurrentThread(g_activity->vm);
    return top;
}

void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
    g_activity = activity;
    (void)savedState; (void)savedStateSize;
    LOGI("nori: activity created");
    activity->callbacks->onNativeWindowCreated   = on_window_created;
    activity->callbacks->onNativeWindowResized   = on_window_resized;
    activity->callbacks->onNativeWindowDestroyed = on_window_destroyed;
    activity->callbacks->onInputQueueCreated     = on_input_created;
    activity->callbacks->onInputQueueDestroyed   = on_input_destroyed;
    activity->callbacks->onDestroy               = on_destroy;
    activity->callbacks->onLowMemory             = on_low_memory;
    activity->callbacks->onConfigurationChanged  = on_config_changed;
    activity->callbacks->onSaveInstanceState     = on_save_state;
    /* An app has no /tmp. Programs that write scratch files reach for one anyway, and on Android the
       writable place is the activity's own internal data directory — which only the activity knows.
       Exporting it as TMPDIR means portable code keeps working here without an Android special case,
       since that is the variable everything already consults. */
    if (activity->internalDataPath && *activity->internalDataPath) {
        setenv("TMPDIR", activity->internalDataPath, 1);
        LOGI("nori: TMPDIR=%s", activity->internalDataPath);
    }
    /* The real display density, which is what makes one UI serve both a desktop and a phone: at
       160dpi (mdpi) scale is 1.0, and a modern handset is 400-560dpi, so everything laid out in
       logical pixels comes out 2.5-3.5x larger. Read from the activity's own configuration; the
       hardcoded 160 that stood here is why the browser chrome arrived desktop-sized on a phone. */
    int dpi = 160;
    if (activity->assetManager) {
        AConfiguration* cfg = AConfiguration_new();
        if (cfg) {
            AConfiguration_fromAssetManager(cfg, activity->assetManager);
            int d = AConfiguration_getDensity(cfg);
            /* ACONFIGURATION_DENSITY_DEFAULT/NONE/ANY are sentinels, not measurements. */
            if (d > 0 && d != ACONFIGURATION_DENSITY_NONE && d != ACONFIGURATION_DENSITY_ANY) dpi = d;
            AConfiguration_delete(cfg);
        }
    }
    LOGI("nori: display density %ddpi (scale %d/120)", dpi, dpi * 120 / 160);
    nori_android_set_density(dpi);
}
