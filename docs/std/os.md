# std/os

```nori
import "std/os" as os
```

std/os: environment variables, directories, cwd, existence, and process spawn with an explicit
argument vector. No shell: arguments are passed as given, so spaces / quotes / `;` / `$` are safe.

Built on the unsafe subset (extern libc + raw memory). The spawn primitive (fork/exec/wait) is
provided by the runtime as nori_spawn_argv; all platform-specific code lives there.
### `fn read_file_head(path: Str, n: Int) -> Str`

read at most `n` bytes from the start of a file (a partial read, for sniffing headers of large
files without loading them, e.g. font name tables). "" if the file cannot be opened.

### `fn read_file_range(path: Str, off: Int, n: Int) -> Str`

read at most `n` bytes of a file starting at byte `off` (a partial read from anywhere; the
tables a font discovery needs can sit at the end of a 20 MB collection). "" if the file cannot be
opened or the offset cannot be reached; shorter than `n` at the end of the file.

### `fn read_at(fd: Int, buf: Int, off: Int, n: Int) -> Int`

read `n` bytes at byte `off` of the open file `fd` into `buf` (a seek, then reads until `n` or the end): the
count read, -1 when the offset cannot be reached. For a reader that walks a large file by offset (a video's
container) without opening it again for every read.

### `fn size_fd(fd: Int) -> Int`

the size in bytes of the open file `fd` (its end, by seeking there), -1 when it cannot be sought

### `fn O_RDONLY() -> Int`

open for reading only.

### `fn O_WRONLY() -> Int`

open for writing only.

### `fn O_RDWR() -> Int`

open for reading and writing.

### `fn O_CREAT() -> Int`

create the file if it doesn't exist (needs a `mode`, e.g. 0644 = 420). Platform-specific value:
always call this rather than writing the number, since Linux/macOS/Windows disagree (Linux 0o100,
macOS 0x200, msvcrt 0x100), and a hard-coded 64 means `_O_TEMPORARY` on Windows.

### `fn O_TRUNC() -> Int`

truncate the file to zero length on open. Platform-specific value: call, don't hard-code.

### `fn O_APPEND() -> Int`

append: every write goes to the end of the file. Platform-specific value: call, don't hard-code
(Linux 0o2000 collides with msvcrt's `_O_EXCL`).

### `fn open_file(path: Str, flags: Int, mode: Int) -> Int`

open `path` with `flags` (see O_* above) and `mode` (permission bits for a freshly O_CREAT'd file,
e.g. 0644 = 420; ignored otherwise). Returns the fd, or a negative errno on failure. Safe wrapper:
no `unsafe` needed at the call site, so creating a new file (O_CREAT) does not require raw open.

### `fn close_fd(fd: Int) -> Int`

close a file descriptor. Returns 0 on success.

### `fn read_fd(fd: Int, buf: Int, n: Int) -> Int`

read up to `n` bytes from `fd` into the raw buffer `buf`; returns the count (0 = EOF, <0 = error).
`buf` is a raw address (from `malloc`), so this is an unsafe-subset building block; for a safe
whole-file copy use `io::copy_file`.

### `fn write_fd(fd: Int, buf: Int, n: Int) -> Int`

write exactly `n` bytes from the raw buffer `buf` to `fd` (looping over short writes). Returns the
number of bytes written, or <0 on error.

### `struct File`

A re-establishable open file. Unlike the bare fd from `open_file` (which `snapshot()`
treats as an opaque resource and refuses over), a `File` carries its reopen metadata, so snapshot()/
resume() serialize {path, flags, mode, current offset} and re-open the file on resume; a `File`
held in a global survives a program image. Field order is fixed: the runtime's `nori_file_ser`/
`nori_file_de` read fd@0, path@1, flags@2, mode@3.

### `fn file_open(path: Str, flags: Int, mode: Int) -> File`

open a re-establishable File. flags/mode are the platform's own open values (see the O_* helpers).
On Windows the binary flag is added automatically, so a restored offset always matches the bytes.

### `fn file_ok(f: File) -> Bool`

true if the file opened successfully.

### `fn file_fd(f: File) -> Int`

the raw fd (for interop with the fd-based read_fd/write_fd building blocks).

### `fn file_read(f: File, buf: Int, n: Int) -> Int`

read up to `n` bytes into the raw buffer `buf`; returns the count (0 = EOF, <0 = error).

### `fn file_write(f: File, buf: Int, n: Int) -> Int`

write `n` bytes from the raw buffer `buf`; returns bytes written or <0.

### `fn file_close(f: File) -> Int`

close the file.

### `fn is_tty() -> Bool`

is stdout a terminal? (it gates colored output so piped/captured output stays plain).

### `fn os_cstr(s: Str) -> Int`

Str -> a malloc'd, NUL-terminated C string (the caller frees it).

### `fn os_from_cstr(p: Int) -> Str`

a NUL-terminated C string -> Str (reads bytes up to the NUL).

### `fn env(name: Str) -> Str`

the value of environment variable `name`, or "" if it is unset.

### `fn env_or(name: Str, dflt: Str) -> Str`

`name`'s value, or `dflt` if unset/empty.

### `fn make_dir(path: Str) -> Int`

create directory `path` (mode 0755). Returns 0 on success.

### `fn make_dir_p(path: Str) -> Int`

create `path` and any missing parent directories.

### `fn path_exists(path: Str) -> Bool`

does `path` exist?

### `fn cwd() -> Str`

the current working directory.

### `fn set_cwd(path: Str) -> Int`

change the process's current working directory to `path`. Returns 0 on success.

### `fn set_env(name: Str, val: Str) -> Int`

set environment variable `name` to `val` (overwriting); spawned children inherit it. Returns 0 on success.

### `fn make_exec(path: Str) -> Int`

mark `path` executable (mode 0755). Returns 0 on success.

### `fn append_file(path: Str, s: Str) -> Bool`

Append exact bytes to a file, creating it if it does not exist. Returns true when all of it was
written, which is the point, as with `write_out`: for a stream arriving in pieces, a partial
write is not a shorter file, it is a corrupt one, and the caller has to know the difference.
NUL-safe: the length written is the string's, not strlen's.

### `fn make_private(path: Str) -> Int`

mark `path` private to its owner (mode 0600). Returns 0 on success. For anything that is a
credential (a stored session, a key, a password file), where the default 0644 would leave it
readable by every account on the machine.

### `fn read_line() -> Str`

read one line from stdin (without the trailing newline); "" on EOF.

### `fn read_n(n: Int) -> Str`

read up to `n` bytes from stdin (fd 0); returns what was read ("" on immediate EOF). For length-framed
protocols (e.g. LSP message bodies) where you must read an exact byte count, not a line.

### `fn write_out(s: Str) -> Bool`

write `s` to stdout (fd 1) as exact bytes, with no trailing newline (unlike printl). For binary/framed
output. Returns true when all of it was written.

The return value is the point: this exists for framed output, where a partial write is not a
smaller message but a corrupt stream; a length-prefixed frame cut in half desynchronises the
reader for good. It does not trap, because a closed stdout (the reader went away, a pipe to
`head`) is an ordinary end to a program, not a bug in it. The caller decides which it is.

### `fn move_path(from: Str, to: Str) -> Int`

rename/move a file or directory (same filesystem). Returns 0 on success.

### `fn remove_tree(path: Str) -> Int`

recursively delete a file or directory tree. Returns 0 on success.

### `fn list_tree(path: Str) -> Vec<Str>`

every file under `path`, recursively, as a Vec of paths (directories are descended, not listed),
sorted. Empty Vec if the path is missing. Sorted because the filesystem's own order differs
between checkouts of one tree, so a build that imports sources in that order would depend on
where the tree was checked out.

### `fn read_dir(path: Str) -> Vec<Str>`

the immediate entries of directory `path` (names only, excluding "." and ".."). Filesystem order;
sort if you need determinism. Empty Vec if `path` can't be opened.

### `fn WATCH_NONBLOCK() -> Int`

IN_NONBLOCK: a read with nothing pending returns at once instead of blocking.

### `fn WATCH_CLOEXEC() -> Int`

IN_CLOEXEC: the descriptor does not survive an exec.

### `fn WATCH_TREE() -> Int`

IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF.

The events that change the shape of a tree. Not IN_MODIFY: a file being written to
does not add, remove or rename anything, and a file browser that redrew on every keystroke of a
build's output would be a worse version of polling.

### `fn WATCH_SAVES() -> Int`

WATCH_TREE and IN_CLOSE_WRITE: the tree's shape, and a file written and closed in it, such as a saved file,
whether the editor wrote it in place or (as most do) beside it and renamed it over. For a program that
applies a configuration when it is saved; still not IN_MODIFY, which fires for every write().

### `struct Watch`

an open watch: the inotify fd, its read buffer, and whether the kernel dropped anything.

### `fn watch_open() -> Watch`

open a watch. `ok` is false where the platform has no inotify; check it and keep a fallback.

### `fn watch_ok(w: Watch) -> Bool`

is this watch usable?

### `fn watch_overflowed(w: Watch) -> Bool`

did the kernel drop events since the last `watch_take`? A caller seeing this should re-scan from
scratch rather than trust what it has.

### `fn watch_add(w: Watch, path: Str) -> Int`

watch one directory. Returns a watch descriptor, or -1, which is not fatal: a directory may have
vanished between listing and watching, or the per-user watch limit
(/proc/sys/fs/inotify/max_user_watches) may be reached, and the watches already held still work.

### `fn watch_add_saves(w: Watch, path: Str) -> Int`

watch one directory for its shape and for files saved in it (WATCH_SAVES): -1 as watch_add.

### `fn watch_rm(w: Watch, wd: Int) -> Int`

stop watching one descriptor.

### `fn watch_take(inout w: Watch) -> Bool`

drain whatever the kernel has queued and report whether anything happened.

Coalescing is the point: a caller that re-scans on change wants one answer per look, not one per
event; copying a directory in produces hundreds, and they all mean the same thing. Returns false
immediately when the queue is empty, which is the ordinary case and costs one syscall.

### `fn watch_clear_overflow(inout w: Watch)`

clear the overflow flag, once the caller has done the full re-scan it asks for.

### `fn watch_close(inout w: Watch)`

close the watch and release its buffer. Every watch descriptor goes with the fd.

### `struct Stat`

the result of stat_path: whether it succeeded, plus directory-ness, byte size, mtime and raw mode.

### `fn stat_path(path: Str) -> Stat`

stat `path`: existence, directory-ness, byte size, mtime (unix seconds) and raw st_mode. ok=false
if the path can't be stat'd.

### `fn temp_dir() -> Str`

a directory this process may write scratch files into, without a trailing slash.

`/tmp` is not a place on Windows. A program that shells out and writes an intermediate file to
`/tmp/...` fails there, so the directory is taken from the environment: TMPDIR is the
POSIX spelling, TEMP and TMP the Windows ones, and the last resort is the platform's usual place.

### `fn home_dir() -> Str`

the current user's home directory, without a trailing slash ("" when the platform names none).

Windows does not set `HOME`; it names the same directory USERPROFILE, so a program that reads
only HOME writes its settings nowhere a user will find them again, or, with a `/tmp` fallback,
loses them at the next reboot.

### `fn state_dir(app: Str) -> Str`

where `app` keeps what it remembers between runs (settings, layout, history, keys) without a
trailing slash. The directory is not created; `make_dir_p` it before writing.

  | platform | directory |
  |---|---|
  | any, `XDG_STATE_HOME` set | `$XDG_STATE_HOME/<app>` |
  | Windows | `%LOCALAPPDATA%\<app>` (then `%APPDATA%`, then `<home>/AppData/Local`) |
  | macOS   | `~/Library/Application Support/<app>` |
  | Linux   | `~/.local/state/<app>` |

Never beside the program. An install directory is replaced whole by an update, and everything a
program had written into it goes with it, settings and keys included. And never relative to the
working directory: `HOME` is not set on Windows, and `env_or("HOME", ".") + "/.local/state"` quietly
becomes a folder inside whatever directory the program was started from.

With no home directory at all, the answer is under `temp_dir()`: it will not survive a reboot,
but it is a place that exists, which "" is not.

### `fn temp_path(name: Str) -> Str`

a scratch file path: `temp_dir()` joined to `name`.

### `fn is_dir(path: Str) -> Bool`

is `path` an existing directory?

### `fn is_file(path: Str) -> Bool`

is `path` an existing regular file, something you could open or run, as against a directory?

`path_exists` is also true for a directory, so a lookup for a program or a data file that stops
at `path_exists` accepts a directory of the same name and fails later, at the open or the exec,
with an error that blames the path.

### `fn file_size(path: Str) -> Int`

byte size of `path` (0 if it can't be stat'd).

### `fn run(argv: Vec<Str>) -> Int`

run a program with an explicit argument vector (argv[0] is the program, PATH-searched). No shell is
involved; each argument is passed as given. Returns the program's exit code (-1 on spawn failure).

### `fn run_silent(argv: Vec<Str>) -> Int`

like `run`, but the child's stdout+stderr are sent to /dev/null (the parent temporarily redirects
fd 1/2 across the fork, then restores them). For spawning helpers whose output you don't want, e.g.
probing whether a build compiles. Falls back to a normal `run` if /dev/null can't be opened.

Caveat: the redirect is process-wide for its duration. Anything already sitting in the stdout
buffer, including a line another task wrote a moment ago, is flushed to /dev/null when it
drains, and is simply gone. In a program with concurrent tasks that print, prefer `run` unless
the child is genuinely noisy.

### `struct Child`

a running child: its pid, and the two ends the parent kept.

### `fn PROC_RUNNING() -> Int`

`child_poll`'s answer while the child is still running.

### `fn PROC_GONE() -> Int`

`child_poll`'s answer for a child that is not ours to wait for: already reaped, or never spawned.

### `fn child_none() -> Child`

a Child that never started.

### `fn child_spawn(argv: Vec<Str>) -> Child`

start `argv` (argv[0] is the program, PATH-searched) without waiting for it.

Named `child_spawn` rather than `spawn` because `spawn` is a keyword, one of Nori's parallelism
forms, so `fn spawn` does not parse. It also puts it in the same family as the calls below.

No shell is involved and each argument is passed as given. The returned Child has `alive` false
if the spawn failed, which is the one thing a caller must check; every other call here is a
no-op on a dead Child rather than a trap.

### `fn child_read(c: Child) -> Str`

whatever the child has said since the last call, or "" if it has said nothing YET.

"" is not end-of-output: a child that is thinking and a child that has finished both read empty,
and only `child_poll` tells them apart. Reading is non-blocking, so calling this every frame
costs a syscall that returns immediately.

Text only. The chunk is read back through `os_from_cstr`, which stops at the first NUL, so a
child emitting binary is truncated at its first zero byte. Every caller this exists for (a
language server, a compiler, a test runner) speaks text.

### `fn child_write(c: Child, t: Str) -> Int`

send `t` to the child's stdin. Returns the bytes written, or -1.

A child that has already exited reports -1 rather than killing this process: SIGPIPE is ignored
from the first `child_spawn` onward, precisely so that a dead server is a failed write and not a
crashed editor.

### `fn child_poll(c: Child) -> Int`

PROC_RUNNING while it runs, the exit code once it has finished, PROC_GONE if it is not ours.

### `fn child_close_stdin(inout c: Child)`

close the child's stdin, which is how most line-oriented programs are told there is no more
input. The child usually exits shortly after; `child_poll` says when.

### `fn child_kill(c: Child) -> Int`

ask the child to stop (SIGTERM). It may take a moment; poll for the exit code.

### `fn child_close(inout c: Child)`

close both pipe ends. Call once the child has exited, or the fds leak.

This does not reap the child; `child_poll` does that, and a caller who closes without polling
leaves a zombie until the process exits.


