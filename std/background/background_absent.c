/* No scheduler on this platform.
 *
 * Desktop Linux, macOS and Windows all have one (systemd user timers, launchd, Task Scheduler),
 * and each is a real piece of work: writing a unit file or a plist, finding the program's own path,
 * and reckoning with the fact that a timer outlives the program that asked for it and has to be
 * findable again to cancel. None of that is implemented, so this reports failure.
 *
 * `woken()` still works everywhere, because it also reads --nori-woken from the command line: a
 * cron line or a systemd unit written by hand can start a program this way, and the program
 * needs no different code to notice.
 */
int nori_bg_woken(void) { return 0; }
int nori_bg_schedule(int hours) { (void)hours; return -2; }
int nori_bg_cancel(void) { return 0; }
