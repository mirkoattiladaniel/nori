/* Notifications on Android, through NotificationManager over JNI.
 *
 * There is no C API for this: notifications are a framework service, so every step below is a
 * Java call made by hand. It is long, but there is nothing clever in it.
 *
 * Three things the platform insists on, each of which silently drops the notification if missed:
 *   - a channel, since API 26. No channel, no notification, and no error either.
 *   - a small icon. A notification without one is rejected outright. A dex-free APK has no
 *     drawables of its own, so this borrows one from the framework by name rather than hardcoding
 *     a resource id that is not ours to depend on.
 *   - the POST_NOTIFICATIONS permission, since API 33, which is a runtime permission: declaring it
 *     in the manifest is necessary and not sufficient. Someone has to be asked.
 *
 * The activity comes from std/window's glue. That is not a layering slip: on Android the
 * NativeActivity is the program's entry point, so an app that can draw anything already has one,
 * and a command-line binary pushed to /data/local/tmp has no app identity to notify under anyway.
 */
#include <android/native_activity.h>
#include <jni.h>
#include <string.h>
#include <stdio.h>

/* Not the activity. A program often wants to send a notification when it has no
   activity (a background job with something to report), so this asks for a Context and a VM,
   which a service process has and an activity process also has. */
extern void* nori_android_vm(void);
extern void* nori_android_context(void);

static char g_app[96] = "nori";
static int  g_next_id = 1;
static int  g_channel_ready = 0;
static int  g_asked_permission = 0;

#define CHANNEL_ID "nori.notify"
#define TAGS 32

/* Tagged notifications all post under id 1 and are told apart by their tag, so an id handed back
   to Nori has to carry the tag. This is that lookup: id 1000+i means "the notification tagged
   g_tag[i]". Nothing here is freed — a tag is a name a program reuses, not a resource. */
static char g_tag[TAGS][64];

/* The PendingIntent that reopens this app, or 0. FLAG_IMMUTABLE is required: since API 31 a
   PendingIntent built without saying whether it is mutable is refused outright, and the refusal
   arrives as an exception at build time rather than as a notification that quietly does nothing. */
static jobject launch_intent(JNIEnv* env, jobject ctx);

static void say(char* err, int n, const char* msg) {
    if (err && n > 0) { strncpy(err, msg, (size_t)n - 1); err[n - 1] = 0; }
}

/* A pending exception poisons every later JNI call, so it is cleared at each step rather than
   allowed to accumulate into something that looks like an unrelated failure. */
static int cleared(JNIEnv* env) {
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 1; }
    return 0;
}

static int sdk_int(JNIEnv* env) {
    jclass c = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (!c || cleared(env)) return 0;
    jfieldID f = (*env)->GetStaticFieldID(env, c, "SDK_INT", "I");
    if (!f || cleared(env)) return 0;
    return (int)(*env)->GetStaticIntField(env, c, f);
}

static jobject get_manager(JNIEnv* env, jobject ctx) {
    jclass cc = (*env)->GetObjectClass(env, ctx);
    jmethodID gss = (*env)->GetMethodID(env, cc, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!gss || cleared(env)) return 0;
    jstring name = (*env)->NewStringUTF(env, "notification");
    jobject nm = (*env)->CallObjectMethod(env, ctx, gss, name);
    if (cleared(env)) nm = 0;
    (*env)->DeleteLocalRef(env, name);
    return nm;
}

/* A framework drawable, looked up by name. Every Android build has ic_dialog_info; asking for it
   by name means this does not depend on a numeric id that could differ. */
static jint small_icon(JNIEnv* env, jobject ctx) {
    jclass cc = (*env)->GetObjectClass(env, ctx);
    jmethodID gr = (*env)->GetMethodID(env, cc, "getResources", "()Landroid/content/res/Resources;");
    if (!gr || cleared(env)) return 0;
    jobject res = (*env)->CallObjectMethod(env, ctx, gr);
    if (!res || cleared(env)) return 0;
    jclass rc = (*env)->GetObjectClass(env, res);
    jmethodID gi = (*env)->GetMethodID(env, rc, "getIdentifier",
                                       "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
    if (!gi || cleared(env)) return 0;
    jstring n = (*env)->NewStringUTF(env, "ic_dialog_info");
    jstring t = (*env)->NewStringUTF(env, "drawable");
    jstring p = (*env)->NewStringUTF(env, "android");
    jint id = (*env)->CallIntMethod(env, res, gi, n, t, p);
    if (cleared(env)) id = 0;
    (*env)->DeleteLocalRef(env, n); (*env)->DeleteLocalRef(env, t); (*env)->DeleteLocalRef(env, p);
    return id;
}

static jobject launch_intent(JNIEnv* env, jobject ctx) {
    jclass cc = (*env)->GetObjectClass(env, ctx);
    jmethodID gpm  = (*env)->GetMethodID(env, cc, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jmethodID gpn  = (*env)->GetMethodID(env, cc, "getPackageName", "()Ljava/lang/String;");
    if (!gpm || !gpn || cleared(env)) return 0;
    jobject pm  = (*env)->CallObjectMethod(env, ctx, gpm);
    jstring pkg = (jstring)(*env)->CallObjectMethod(env, ctx, gpn);
    if (!pm || !pkg || cleared(env)) return 0;

    jclass pmc = (*env)->GetObjectClass(env, pm);
    jmethodID gli = (*env)->GetMethodID(env, pmc, "getLaunchIntentForPackage",
                                        "(Ljava/lang/String;)Landroid/content/Intent;");
    if (!gli || cleared(env)) return 0;
    jobject intent = (*env)->CallObjectMethod(env, pm, gli, pkg);
    if (!intent || cleared(env)) return 0;   /* a package with no launcher entry has nothing to open */

    jclass pic = (*env)->FindClass(env, "android/app/PendingIntent");
    if (!pic || cleared(env)) return 0;
    jmethodID ga = (*env)->GetStaticMethodID(env, pic, "getActivity",
                       "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;");
    if (!ga || cleared(env)) return 0;
    jint flags = 0x08000000;                              /* FLAG_UPDATE_CURRENT */
    if (sdk_int(env) >= 23) flags |= 0x04000000;          /* FLAG_IMMUTABLE, required from 31 */
    jobject pi = (*env)->CallStaticObjectMethod(env, pic, ga, ctx, 0, intent, flags);
    if (cleared(env)) return 0;
    return pi;
}

static void ensure_channel(JNIEnv* env, jobject nm) {
    if (g_channel_ready || sdk_int(env) < 26) { g_channel_ready = 1; return; }
    jclass nc = (*env)->FindClass(env, "android/app/NotificationChannel");
    if (!nc || cleared(env)) return;
    jmethodID init = (*env)->GetMethodID(env, nc, "<init>", "(Ljava/lang/String;Ljava/lang/CharSequence;I)V");
    if (!init || cleared(env)) return;
    jstring cid  = (*env)->NewStringUTF(env, CHANNEL_ID);
    jstring name = (*env)->NewStringUTF(env, g_app);
    jobject chan = (*env)->NewObject(env, nc, init, cid, name, 3);   /* IMPORTANCE_DEFAULT */
    if (chan && !cleared(env)) {
        jclass nmc = (*env)->GetObjectClass(env, nm);
        jmethodID create = (*env)->GetMethodID(env, nmc, "createNotificationChannel",
                                               "(Landroid/app/NotificationChannel;)V");
        if (create && !cleared(env)) { (*env)->CallVoidMethod(env, nm, create, chan); cleared(env); }
        g_channel_ready = 1;
    }
    (*env)->DeleteLocalRef(env, cid); (*env)->DeleteLocalRef(env, name);
}

/* 1 granted, 0 not. Below API 33 there is nothing to grant. */
static int have_permission(JNIEnv* env, jobject ctx) {
    if (sdk_int(env) < 33) return 1;
    jclass cc = (*env)->GetObjectClass(env, ctx);
    jmethodID chk = (*env)->GetMethodID(env, cc, "checkSelfPermission", "(Ljava/lang/String;)I");
    if (!chk || cleared(env)) return 0;
    jstring p = (*env)->NewStringUTF(env, "android.permission.POST_NOTIFICATIONS");
    jint r = (*env)->CallIntMethod(env, ctx, chk, p);
    if (cleared(env)) r = -1;
    (*env)->DeleteLocalRef(env, p);
    return r == 0;
}

/* Ask once. NativeActivity never sees onRequestPermissionsResult, so there is no callback to wait
   for: the dialog goes up and the answer shows up in checkSelfPermission whenever the next send
   asks. That is why this reports "asked" rather than pretending to have sent something. */
static void request_permission(JNIEnv* env, jobject ctx) {
    if (g_asked_permission || sdk_int(env) < 33) return;
    g_asked_permission = 1;
    jclass cc = (*env)->GetObjectClass(env, ctx);
    jmethodID req = (*env)->GetMethodID(env, cc, "requestPermissions", "([Ljava/lang/String;I)V");
    if (!req || cleared(env)) return;
    jclass sc = (*env)->FindClass(env, "java/lang/String");
    jobjectArray arr = (*env)->NewObjectArray(env, 1, sc, (*env)->NewStringUTF(env, "android.permission.POST_NOTIFICATIONS"));
    if (arr && !cleared(env)) { (*env)->CallVoidMethod(env, ctx, req, arr, 1); cleared(env); }
}

void nori_notify_set_app(const char* name) {
    if (!name || !*name) return;
    strncpy(g_app, name, sizeof(g_app) - 1); g_app[sizeof(g_app) - 1] = 0;
}

/* Attach this thread and hand back the Context to work with. 0 when the process has no JVM or no
   Context — a command-line binary pushed to /data/local/tmp, say, which has no app identity to
   notify under in the first place. */
static jobject attach(JNIEnv** env) {
    JavaVM* vm = (JavaVM*)nori_android_vm();
    jobject ctx = (jobject)nori_android_context();
    if (!vm || !ctx) return 0;
    if ((*vm)->AttachCurrentThread(vm, env, NULL) != JNI_OK || !*env) return 0;
    return ctx;
}

int nori_notify_available(void) {
    JNIEnv* env = 0;
    jobject ctx = attach(&env);
    if (!ctx) return 0;
    return have_permission(env, ctx) ? 1 : 0;
}

int nori_notify_send(const char* title, const char* body, const char* tag, char* err, int errlen) {
    JNIEnv* env = 0;
    jobject ctx = attach(&env);
    if (!ctx) { say(err, errlen, "no Android context (is this a command-line binary?)"); return 0; }

    if (!have_permission(env, ctx)) {
        request_permission(env, ctx);
        say(err, errlen, "POST_NOTIFICATIONS not granted — asked; try again once answered");
        return 0;
    }

    jobject nm = get_manager(env, ctx);
    if (!nm) { say(err, errlen, "no NotificationManager"); return 0; }
    ensure_channel(env, nm);

    jint icon = small_icon(env, ctx);
    if (!icon) { say(err, errlen, "no small icon: the platform drops a notification without one"); return 0; }

    /* Notification.Builder gained the channel argument in 26; before that there was no channel. */
    jclass bc = (*env)->FindClass(env, "android/app/Notification$Builder");
    if (!bc || cleared(env)) { say(err, errlen, "no Notification.Builder"); return 0; }
    jobject b = 0;
    if (sdk_int(env) >= 26) {
        jmethodID init = (*env)->GetMethodID(env, bc, "<init>", "(Landroid/content/Context;Ljava/lang/String;)V");
        jstring cid = (*env)->NewStringUTF(env, CHANNEL_ID);
        if (init) b = (*env)->NewObject(env, bc, init, ctx, cid);
        (*env)->DeleteLocalRef(env, cid);
    } else {
        jmethodID init = (*env)->GetMethodID(env, bc, "<init>", "(Landroid/content/Context;)V");
        if (init) b = (*env)->NewObject(env, bc, init, ctx);
    }
    if (!b || cleared(env)) { say(err, errlen, "could not build the notification"); return 0; }

    jmethodID sTitle = (*env)->GetMethodID(env, bc, "setContentTitle", "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
    jmethodID sText  = (*env)->GetMethodID(env, bc, "setContentText",  "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;");
    jmethodID sIcon  = (*env)->GetMethodID(env, bc, "setSmallIcon",    "(I)Landroid/app/Notification$Builder;");
    jmethodID sAuto  = (*env)->GetMethodID(env, bc, "setAutoCancel",   "(Z)Landroid/app/Notification$Builder;");
    jmethodID sIntent= (*env)->GetMethodID(env, bc, "setContentIntent", "(Landroid/app/PendingIntent;)Landroid/app/Notification$Builder;");
    jmethodID build  = (*env)->GetMethodID(env, bc, "build",           "()Landroid/app/Notification;");
    if (!sTitle || !sText || !sIcon || !build || cleared(env)) { say(err, errlen, "Notification.Builder is not the shape expected"); return 0; }

    jstring jt = (*env)->NewStringUTF(env, title ? title : "");
    jstring jb = (*env)->NewStringUTF(env, body ? body : "");
    (*env)->CallObjectMethod(env, b, sTitle, jt);
    (*env)->CallObjectMethod(env, b, sText, jb);
    (*env)->CallObjectMethod(env, b, sIcon, icon);
    if (sAuto) (*env)->CallObjectMethod(env, b, sAuto, JNI_TRUE);
    /* Every notification opens the program that sent it when tapped, with no API for choosing
       otherwise. The intent is not built by hand: getLaunchIntentForPackage asks the system for
       the same intent the launcher icon uses, so this lands wherever the launcher would and needs
       to know nothing about the activity's name. */
    jobject pi = launch_intent(env, ctx);
    if (pi && sIntent) (*env)->CallObjectMethod(env, b, sIntent, pi);
    cleared(env);

    jobject note = (*env)->CallObjectMethod(env, b, build);
    (*env)->DeleteLocalRef(env, jt); (*env)->DeleteLocalRef(env, jb);
    if (!note || cleared(env)) { say(err, errlen, "the notification would not build"); return 0; }

    /* The tag is what makes a repeat replace rather than stack: same tag and id, one popup. An
       untagged notice gets a fresh id, so those stack, which is what a stream of events wants. */
    jclass nmc = (*env)->GetObjectClass(env, nm);
    jmethodID notify = (*env)->GetMethodID(env, nmc, "notify", "(Ljava/lang/String;ILandroid/app/Notification;)V");
    if (!notify || cleared(env)) { say(err, errlen, "NotificationManager has no notify(tag, id, n)"); return 0; }

    int id = (tag && *tag) ? 1 : g_next_id++;
    jstring jtag = (tag && *tag) ? (*env)->NewStringUTF(env, tag) : (*env)->NewStringUTF(env, "");
    (*env)->CallVoidMethod(env, nm, notify, jtag, id, note);
    (*env)->DeleteLocalRef(env, jtag);
    if (cleared(env)) { say(err, errlen, "the notification was refused"); return 0; }

    /* The id handed back carries the tag's identity for cancel: tagged notices all use id 1 and
       differ by tag, so the tag has to be remembered here. */
    if (tag && *tag) {
        for (int i = 0; i < TAGS; i++) {
            if (strncmp(g_tag[i], tag, sizeof(g_tag[i]) - 1) == 0) return 1000 + i;
            if (!g_tag[i][0]) {
                strncpy(g_tag[i], tag, sizeof(g_tag[i]) - 1); g_tag[i][sizeof(g_tag[i]) - 1] = 0;
                return 1000 + i;
            }
        }
        return 1000;          /* table full: the id points at the oldest tag, which is recoverable */
    }
    return id;
}

int nori_notify_cancel(int id) {
    if (id <= 0) return 0;
    JNIEnv* env = 0;
    jobject ctx = attach(&env);
    if (!ctx) return 0;
    jobject nm = get_manager(env, ctx);
    if (!nm) return 0;
    jclass nmc = (*env)->GetObjectClass(env, nm);
    jmethodID cancel = (*env)->GetMethodID(env, nmc, "cancel", "(Ljava/lang/String;I)V");
    if (!cancel || cleared(env)) return 0;

    jstring jtag; int nid;
    if (id >= 1000 && id < 1000 + TAGS) { jtag = (*env)->NewStringUTF(env, g_tag[id - 1000]); nid = 1; }
    else                                { jtag = (*env)->NewStringUTF(env, "");             nid = id; }
    (*env)->CallVoidMethod(env, nm, cancel, jtag, nid);
    (*env)->DeleteLocalRef(env, jtag);
    if (cleared(env)) return 0;
    return 1;
}
