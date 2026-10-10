/* Being woken on Android: JobScheduler, reached through the one generic Java class roll packages.
 *
 * Two directions cross here, and they are not symmetrical:
 *
 *   Nori -> Java   asking to be woken. Ordinary JNI, made while the app is running.
 *   Java -> Nori   the waking itself. The framework starts nori.bg.NoriJob, which loads this
 *                  library and calls run(), and that is the entry point that needs a class to
 *                  name in the manifest.
 */
#include <jni.h>
#include <string.h>

extern void* nori_android_vm(void);
extern void* nori_android_context(void);
extern void  nori_android_set_context(void* ctx);
extern int   nori_enter_main(int argc, char** argv);
extern void  nori_android_start_logging(void);

/* Set for the duration of a woken run, so the program can ask how it was started. A plain global
   is enough: the runtime refuses to enter twice at once, so there is never more than one run to
   describe. */
static int g_woken;

int nori_bg_woken(void) { return g_woken; }

/* Not FindClass. A thread attached from native code gets the system class loader, which knows only
   framework classes and has never heard of the APK's dex; the lookup fails in a way that looks
   like the class being missing. The app's own loader is the one that has it, and a Context
   is where to ask for it. */
static jclass find_job_class(JNIEnv* env, jobject ctx) {
    jclass cc = (*env)->GetObjectClass(env, ctx);
    jmethodID gcl = (*env)->GetMethodID(env, cc, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!gcl || (*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    jobject loader = (*env)->CallObjectMethod(env, ctx, gcl);
    if (!loader || (*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    jclass lc = (*env)->FindClass(env, "java/lang/ClassLoader");
    jmethodID load = lc ? (*env)->GetMethodID(env, lc, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;") : 0;
    if (!load || (*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    jstring name = (*env)->NewStringUTF(env, "nori.bg.NoriJob");
    jclass job = (jclass)(*env)->CallObjectMethod(env, loader, load, name);
    (*env)->DeleteLocalRef(env, name);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    return job;
}

static int call_static_ctx(const char* method, const char* sig, int arg, int has_arg) {
    JavaVM* vm = (JavaVM*)nori_android_vm();
    jobject ctx = (jobject)nori_android_context();
    if (!vm || !ctx) return 0;
    JNIEnv* env = 0;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK || !env) return 0;
    jclass job = find_job_class(env, ctx);
    if (!job) return 0;
    jmethodID m = (*env)->GetStaticMethodID(env, job, method, sig);
    if (!m || (*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    if (has_arg) (*env)->CallStaticVoidMethod(env, job, m, ctx, (jint)arg);
    else         (*env)->CallStaticVoidMethod(env, job, m, ctx);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    return 1;
}

int nori_bg_schedule(int hours) {
    return call_static_ctx("schedule", "(Landroid/content/Context;I)V", hours, 1) ? 1 : 0;
}

int nori_bg_cancel(void) {
    return call_static_ctx("cancel", "(Landroid/content/Context;)V", 0, 0) ? 1 : 0;
}

/* ---- the way in ------------------------------------------------------------------------------
 * Called by NoriJob on a thread the framework made, in a process that may have no activity at all.
 * The service is the Context here, so it is registered for the duration, which lets
 * std/notify post from a job without knowing it is running in one.
 *
 * Returns whatever the program returned, or -1 if a run was already in progress and the entry was
 * refused. Refusal is the expected outcome, not a failure: a program that is open does not need
 * waking, and entering it twice at once would corrupt both runs.
 */
JNIEXPORT jint JNICALL Java_nori_bg_NoriJob_run(JNIEnv* env, jclass cls) {
    (void)cls;
    jobject self = 0;
    JavaVM* vm = 0;
    (*env)->GetJavaVM(env, &vm);

    /* The JobService instance is not handed to a static method, so the Context comes from the
       class's own application context instead — good enough for everything a job may do, and it
       does not depend on the service outliving the call. */
    jclass ac = (*env)->FindClass(env, "android/app/ActivityThread");
    jmethodID cur = ac ? (*env)->GetStaticMethodID(env, ac, "currentApplication", "()Landroid/app/Application;") : 0;
    if (cur) self = (*env)->CallStaticObjectMethod(env, ac, cur);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); self = 0; }

    jobject held = self ? (*env)->NewGlobalRef(env, self) : 0;
    if (held) nori_android_set_context(held);

    /* printl goes to stdout, and in a process with no activity nobody is pumping stdout into
       logcat. Without this a woken program runs normally but looks, from outside, like it never
       started. */
    nori_android_start_logging();

    char arg0[] = "nori";
    char arg1[] = "--nori-woken";
    char* argv[] = { arg0, arg1, 0 };
    g_woken = 1;
    int rc = nori_enter_main(2, argv);
    g_woken = 0;

    if (held) {
        nori_android_set_context(0);
        (*env)->DeleteGlobalRef(env, held);
    }
    return (jint)rc;
}
