package nori.bg;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.util.Log;

/**
 * The only Java in a Nori app, and the same file in every one of them.
 *
 * It knows nothing about what the program does. It loads the program's library and enters it,
 * the way the activity glue does when someone opens the app, so everything that happens when a
 * program is woken is written in Nori and shares every function with the rest of the program.
 *
 * It exists because a job hours from now is started by the framework, and the framework starts
 * classes. There is no manifest attribute naming a C function and no API that takes one.
 * NativeActivity bridges this for activities but there is no equivalent for services, so this
 * class fills that gap.
 *
 * Nobody writes or edits it: `roll` compiles and packages it when a manifest says
 * background = true.
 */
public class NoriJob extends JobService {

    /** Fixed, because there is one of these per program. Re-scheduling replaces it. */
    private static final int JOB_ID = 0x4E4F5249;      // "NORI"

    static { System.loadLibrary("noriapp"); }

    /** Enter the program with the woken flag set. Returns what the program returned, or -1 if it
     *  was already running in this process and the entry was refused. */
    private static native int run();

    /** Called from the program itself, through JNI, to ask for the wake-ups. */
    public static void schedule(Context ctx, int hours) {
        JobScheduler js = (JobScheduler) ctx.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (js == null) return;
        long ms = (long) hours * 60L * 60L * 1000L;
        js.schedule(new JobInfo.Builder(JOB_ID, new ComponentName(ctx, NoriJob.class))
                .setRequiredNetworkType(JobInfo.NETWORK_TYPE_ANY)
                .setPeriodic(ms)
                // Survive a reboot, or the wake-ups stop the first time the phone restarts, and
                // nobody would notice.
                .setPersisted(true)
                .build());
    }

    public static void cancel(Context ctx) {
        JobScheduler js = (JobScheduler) ctx.getSystemService(Context.JOB_SCHEDULER_SERVICE);
        if (js != null) js.cancel(JOB_ID);
    }

    @Override
    public boolean onStartJob(final JobParameters params) {
        // onStartJob runs on the main thread, and the program may take as long as it takes.
        // Returning true promises that jobFinished will be called when this thread is done.
        new Thread(new Runnable() {
            @Override public void run() {
                try {
                    NoriJob.run();
                } catch (Throwable t) {
                    // Never silently. A missing native method or an unloadable library arrives
                    // here, and if swallowed it looks the same as a program that ran and
                    // found nothing to do.
                    Log.w("nori", "background run failed", t);
                } finally {
                    jobFinished(params, false);
                }
            }
        }).start();
        return true;
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        return true;    // taken away mid-flight; it is periodic, so let it come back
    }
}
