/* No notifier on this platform.
 *
 * macOS is the one that matters here: UNUserNotificationCenter will not deliver for a bare
 * executable; it needs a signed .app bundle, and NSUserNotification (which did not) is gone. So
 * the whole implementation is an honest "no". Answering yes and
 * dropping the notification silently would be worse: the program would believe it had spoken.
 *
 * This is also the file to link when a build wants nothing to do with notifications and still
 * needs the symbols to resolve.
 */
#include <string.h>

void nori_notify_set_app(const char* name) { (void)name; }
int  nori_notify_available(void) { return 0; }

int nori_notify_send(const char* title, const char* body, const char* tag, char* err, int errlen) {
    (void)title; (void)body; (void)tag;
    if (err && errlen > 0) {
        strncpy(err, "no notification service on this platform", (size_t)errlen - 1);
        err[errlen - 1] = 0;
    }
    return 0;
}

int nori_notify_cancel(int id) { (void)id; return 0; }
