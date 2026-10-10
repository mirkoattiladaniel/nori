# std/window

```nori
import "std/window" as window
```

std/window: create and drive a native OS window. One import, whatever the machine is:

  | platform | backend | chosen |
  |---|---|---|
  | Linux, Wayland session | xdg-shell (`window_wayland.c`) | at runtime, by `WAYLAND_DISPLAY` |
  | Linux, X session       | Xlib (`window_x11.c`)          | at runtime, as the fallback |
  | Linux, `--native` build | Wayland and X11 in Nori (`__native_linux.nori`) | at runtime, the same way |
  | Android                | ANativeWindow (`window_android.c`) | at link time, by the manifest |
  | Windows                | Win32 (`window_win32.c`)       | at link time |

All of them answer the same 26 C entry points, so nothing here branches on the platform and no
caller has to. `backend(w)` says which one is live, for the one case that needs it:
building a GPU surface, where Wayland wants display+surface and X11/Win32 want the window handle.

  import "std/window" as window
  var w = window::open("Hello", 960, 600)
  while !window::should_close(w) {
      var pumping = true
      while pumping { match window::poll(w) { window::Event::NoEvent => { pumping = false }  _ => { } } }
      window::present(w, canvas_ptr, ww, hh)
  }
  window::close(w)

Link the shim for the target. On a Linux desktop that is both backends plus `-DNORI_HAVE_X11`,
since which display server is running is not known until the program starts:
  [native]
  cfile = std/window/window_dispatch.c, std/window/window_wayland.c, std/window/window_x11.c
  clink = wayland-client, xkbcommon, X11
  ccflag = -DNORI_HAVE_X11

A program built with `noric --build --native` needs none of that, and no C at all: that build has no
C compiler, so it links `__native_linux.nori` in the shim's place: the same seam in Nori, speaking
the Wayland wire protocol and the X11 core protocol over their sockets and decoding the compositor's
XKB keymap itself. The program and its imports do not change.

A program that never opens a window links `std/window/window_absent.c` instead, and needs no
display-server libraries at all. It matters because std/nori_ui reaches this module through its
renderer, so a headless job (a page rendered to a file, a layout test) declares this seam
without using it. `open` then returns a window `opened` reports as false, which is the same
answer the dispatcher gives when no display server accepts the connection.
### `enum Event`

What happened. `Scroll(n)` is n logical pixels of "move the content up": a wheel rolled towards
the person, or a finger swiped up a touch screen, both meaning "show me what is further down". A
list adds it to its scroll offset; subtracting it makes a page run away from the finger that is
dragging it.

Scroll is measured in pixels, not notches, and in logical rather than device pixels. A wheel has
notches and a finger does not, so one of the two has to be converted, and converting the finger
makes a dragged list move in jumps: quantised to 24dp it climbs the screen in steps rather than
following the hand. So a notch is defined as a distance instead (three lines of text, near
enough) and a drag reports the distance it actually travelled. Logical pixels, so the same
gesture moves a page the same amount on a dense screen as on a coarse one.

Mouse presses are not in this enum: a match on an enum must cover every variant, so adding one
would break every program that already drives this loop. They are drained with `next_click`
instead, which existing code can ignore and new code can read.

### `struct Click`

One press or one release: `kind` is CLICK_DOWN or CLICK_UP, `button` is 1 = left, 2 = middle,
3 = right (a touch screen taps with 1), and `x`,`y` is where the pointer was at that moment.
`kind` is CLICK_NONE for the empty answer `next_click` gives when there is nothing left.

### `fn key_text(sym: Int) -> Str`

the UTF-8 text a keysym produces ("" for non-printable/named keys). ASCII + Latin-1 map directly;
keysyms with the 0x01000000 flag are direct Unicode. Layout + shift are already baked into the keysym.

### `fn keymap_parse(ptr: Int, size: Int) -> Int`

An XKB keymap decoder for a program that reads a Wayland keyboard itself rather than through a window here (a lock
screen's ext_session_lock_v1 surface is on the program's own connection): `keymap_parse` takes the keymap
wl_keyboard.keymap hands over (format 1, mapped: `ptr`, `size` bytes) and gives a handle (0: not one it can read);
`keymap_keysym` the keysym evdev key `key` (wl_keyboard.key's number) types under the modifiers wl_keyboard.modifiers
says (`mods`: depressed | latched | locked), the first group's; `keymap_keysym_group` in the group (the layout in use)
wl_keyboard.modifiers says. It is the same decoding a window's KeyDown carries; `key_text` makes text of it.
`keymap_free` lets the handle go.

### `fn KEY_ESCAPE() -> Int`

keysym constants (X11 values) for the named keys a text field cares about.

### `fn BACKEND_SURFACE() -> Int`

backend ids, as reported by `backend`.

### `fn opened(w: Window) -> Bool`

did the window actually open? Test this rather than `native_display(w) != 0`: Win32 has no
display connection and reports 0 for one that opened perfectly well.

### `fn backend(w: Window) -> Int`

which backend opened this window (BACKEND_SURFACE / BACKEND_X11 / BACKEND_WIN32). On Linux this
is decided when the window opens, not when the program is built, so it is a question about this
run: the same binary reports 0 under a compositor and 1 under an X server.

### `fn native_display(w: Window) -> Int`

the display connection for a GPU surface: a wl_display* on Wayland, a Display* on X11, 0 on
Win32 (which has none; the window handle is the whole address there).

### `fn native_surface(w: Window) -> Int`

the drawable for a GPU surface: a wl_surface* on Wayland, a Window XID on X11, an HWND on Win32.
Which of those it is follows from `backend`, and a GPU backend needs to know: a Wayland surface
is built from display+surface, an X11 or Win32 one from the handle.

### `fn native_handle(w: Window) -> Int`

the same value as `native_surface`, under the name the X11/Win32 side of the world calls it.

### `fn width(w: Window) -> Int`

current window width (updated by Resized events).

### `fn height(w: Window) -> Int`

current window height (updated by Resized events).

### `fn logical_width(w: Window) -> Int`

The window size in logical pixels, which is the unit to lay out in on every backend.

Backends do not measure a window in the same units. Wayland and Win32 report a logical window
and a scale beside it: the compositor's viewporter and the desktop's DPI setting both describe a
window whose size in the units a caller lays out in is smaller than the pixels it is given. X11 and Android report the pixels themselves, and the scale
is a number the caller applies. So `width` alone is not an answer: dividing it by the scale
unconditionally shrinks a window by that factor twice, which shows up as a page drawn into the
top-left corner with black where the rest of the window is; never dividing draws a page too
small for its pixels.

These give the size in logical pixels wherever the program runs. Multiply by `scale` for the
framebuffer to present.

### `fn logical_height(w: Window) -> Int`

the window's height in logical pixels; see `logical_width`.

### `fn open(title: Str, w: Int, h: Int) -> Window`

open a window titled `title`, of size `w` x `h`. Which backend serves it is decided here.

### `fn poll(inout w: Window) -> Event`

the next pending event (NoEvent when the queue is empty). Call repeatedly until NoEvent, then render.

Presses and releases are taken off the same backend queue on the way past and kept for
`next_click`, in order, so a loop that polls to NoEvent has already collected every click that
arrived, whether or not it goes on to read them.

Any thread may poll. On Windows that is not free: a message queue belongs to the thread that
created the window, so the Win32 backend gives the window a thread of its own that does nothing
but receive messages, and `poll` reads the queue that thread fills. A render loop resumed on a
different thread than the one that called `open` (as a coroutine loop is after its
first suspension) therefore keeps getting events, and the window keeps answering the pointer
while the loop is busy.

### `fn fd(w: Window) -> Int`

The descriptor this window's events arrive on, for a program with an event loop of its own, one
that waits in poll(2) or epoll on its windows beside other things (sockets, timers, children).
-1 for a window that did not open or whose connection is gone, and on the C backends, which offer
none (poll them each turn instead).

Wait on it only after `dispatch` says nothing is queued. Events can be read off the socket ahead of
time (a `present` that waits for a buffer reads whatever arrived meanwhile), and an event already
read is in this window's queue, not on the socket, so the descriptor does not become readable for
it. The loop is:

    while running {
        if window::dispatch(win) > 0 { ...window::poll(win) to NoEvent... }
        else { ...wait until window::fd(win) (or anything else) is readable... }
    }

Key repeat is synthesized from a clock inside `poll`, not sent by the display server, so a program
that sleeps on the descriptor sees a held key's first press and not its repeats.

### `fn dispatch(w: Window) -> Int`

Send what is queued for the display server and read what has arrived, without waiting, decoding it
into this window's event queue. Returns how many events `poll` has to hand out (0: nothing, so
it is time to sleep on `fd`), or -1 when the window is gone. `poll` does the same reading itself, so a
program that polls every frame never needs this; it is for a loop that sleeps.

### `fn next_click(inout w: Window) -> Click`

Take the oldest press or release nobody has read yet; `kind == CLICK_NONE()` when there are none.

Polled state can miss a click. A mouse button is down for something like
50ms and a frame can take longer, so a press and its release both land between two frames:
`mouse_pos().down` is false on both sides of them and an edge-detecting caller sees nothing
(`xdotool click` is invisible that way, while `mousedown; sleep; mouseup` is seen). These are
the discrete events the display server actually sent, in order, so three clicks in one frame
are three clicks, each one says which button it was, and each one carries the position the
pointer had when it happened rather than wherever it ended up.

Drain it after the poll loop, in the same frame:

    var c = window::next_click(win)
    while c.kind != window::CLICK_NONE() {
        if c.kind == window::CLICK_DOWN() && c.button == 1 { press_at(c.x, c.y) }
        c = window::next_click(win)
    }

Only `poll` fills this, so a frame that never polls collects nothing.

### `fn mouse_pos(w: Window) -> MousePos`

current pointer snapshot (surface-local position + button-1 held); no buffer needed, no unsafe
for the caller. `(x,y)` is `(-1,-1)` before the pointer has entered the window.

Reading this consumes a click. `down` is true while the button is physically held, and once
more for a press that no earlier call has seen: a click that began and ended between two frames
would otherwise be invisible to a caller that only compares this frame's state with the last.
That latched click reports the position it was pressed at, not where the pointer ended up, and
reading it clears it, so one click is one press-release edge.

The latch is one bit. Two clicks between two calls are reported as one, and it does not say
which button was pressed. `next_click` keeps both, and anything that counts clicks or cares
about the right button should read those instead. `pointer_at` reads the
position without consuming anything.

### `fn inset_top(w: Window) -> Int`

Logical pixels at the top of the window that something else already occupies.

A phone draws its clock, battery and signal across the top of the screen while handing the
app the whole surface, so anything painted at y=0 ends up underneath them. A desktop window
owns its rectangle outright and this is 0, so a caller that offsets its chrome by this value
is correct on both without asking which it is on.

### `fn has_hover(w: Window) -> Bool`

Whether this pointer can hover: rest over something without pressing it.

True for a mouse, false for a touch screen: a finger is only on the glass while it presses,
and it sweeps across everything it drags over. A caller that applies `:hover`, tooltips or
rollover art unconditionally therefore recomputes them the whole length of every swipe, which
is the difference between a scroll that glides and one that stutters.

### `fn scroll_unit(w: Window) -> Float`

Logical pixels that one unit of a `Scroll` event is worth.

A mouse and a finger do not scroll in the same quantity. A wheel reports notches, and four
logical pixels a notch is the convention every desktop caller already assumes. A finger
reports its own travel, and rounding that to notches moves the page in visible steps, so a
touch backend reports device pixels and returns a fraction here instead. Multiply the event's
count by this and the page follows the input on both.

### `fn catch_back(w: Window, on: Bool)`

Take the system back gesture for this program, instead of letting it leave the app.

On Android, a back gesture (or the button older phones have) arrives as `KeyDown(KEY_ESCAPE())`
whether or not this is on. What this changes is whether the system also acts on it: off, it
finishes the activity, so a program sees the key and is closed regardless; on, the activity
stays and the key is the program's to answer.

It is off by default, and this is a call rather than a setting because of what it costs to get
wrong: a program that catches back and then ignores it has trapped the person inside it, with no
way out but the task switcher. Turn it on only alongside code that answers the key: dismiss
whatever is dismissable, and when there is nothing left, `close` the window, which is what the
system would have done.

Everywhere else this does nothing: there is no system back gesture on a desktop, and Escape is
already an ordinary key event.

### `fn soft_keyboard(w: Window, show: Bool)`

Ask the platform to raise or dismiss an on-screen keyboard.

This is a request, not a command: a platform with a real keyboard does nothing, and Android may
decline (no focus, or a hardware keyboard attached). Call it when a text field takes focus
and again with `false` when it loses focus. The same call is correct on every platform, which is
why it is here rather than behind a per-platform branch in the caller.

### `fn set_cursor(w: Window, shape: Int) -> Bool`

ask the window for a pointer shape. Returns false if the platform could not apply it (an old
compositor with no cursor-shape-v1, or a backend with no cursor at all), in which case the
pointer keeps whatever it had.

Call it every frame with the shape the position implies. Every backend here ignores a request for
the shape it is already showing, so this is cheap and there is no state to unwind.

### `fn mouse_middle(w: Window) -> Bool`

is the middle mouse button currently held?

### `fn mouse(w: Window, out_xy: Int) -> Bool`

current pointer position (surface-local); writes x,y into `out_xy` (two Int32) and returns true if
the left button is held, or was clicked and not yet observed, as `mouse_pos` does; the latch
is shared. `out_xy` must point to at least 8 bytes. (Prefer `mouse_pos`.)

### `fn present(w: Window, pixels: Int, ww: Int, hh: Int)`

present a CPU framebuffer to the window. `pixels` points to `ww*hh` 0x00RRGGBB pixels (e.g.
std/nori_ui/paint_cpu's canvas_ptr), which is the byte order every backend here wants.

### `fn frame_wait(w: Window, timeout_ms: Int)`

pace the loop: block until the display is ready for the next frame or `timeout_ms` elapses. Use
instead of a fixed sleep after present(). Only Wayland has a true frame callback; the others
wait out the budget, which keeps a render loop off a busy spin without claiming to be vsynced.

### `fn set_alpha(w: Window, on: Bool)`

Present with an alpha channel: `pixels` are then premultiplied 0xAARRGGBB (std/nori_ui's
`render::set_alpha` draws them so) and what is behind the window shows through where they are
translucent (a dock's rounded corners, a panel at 88%). Off (the default) the top byte is ignored.
Wayland only (wl_shm's ARGB8888); elsewhere the window stays opaque.

### `fn frame_done(w: Window) -> Bool`

Has the compositor shown the last frame `present` gave it? False from a present until its frame
callback fires, which is the moment to draw the next frame of an animation without queueing frames the
display cannot show. It waits for nothing: a program that sleeps on `fd` asks after each `dispatch`.
Always true where frames are not paced (X11, Win32).

### `fn has_keyboard(w: Window) -> Bool`

Does the window have the keyboard focus? (Wayland: between wl_keyboard enter and leave. X11 and
Win32 do not track it here and say true.) A popup that should close when it loses the keyboard asks.

### `fn next_timeout_ms(w: Window) -> Int`

How long a program that sleeps on the window's descriptor (`fd`) may sleep before `poll` has an event that
arrives on no socket: a held key's next repeat, which Wayland leaves to the client (wl_keyboard.repeat_info).
-1 when nothing is due (no key held): sleep until the descriptor is readable. 0: poll now. X11 and Win32
repeat keys themselves (their events come on the socket) and say -1.

### `fn set_title(w: Window, title: Str)`

The window's title from now on: what the title bar, the dock and a window list show. A program
whose title follows what it shows (a file manager's folder, an editor's file) calls it as that
changes; the same title again costs nothing.

Wayland and X11 (the native build's): xdg_toplevel.set_title, WM_NAME and _NET_WM_NAME. The C
backends keep the title `open` gave the window.

### `fn set_app_id(w: Window, id: Str)`

Which application the window belongs to: its desktop entry's name without `.desktop`
(`org.nori.Files`), what a compositor matches its rules and a dock its icon on. Call it right after
`open`, before the first `present`, so the window is never shown without it.

Wayland: xdg_toplevel.set_app_id; X11 (native): WM_CLASS, instance and class both `id`. The C
backends do not set one.

### `fn activation_token(w: Window, app_id: Str) -> Str`

An activation token (xdg_activation_v1) for a program this one is about to start: put it in the
child's environment as XDG_ACTIVATION_TOKEN (and DESKTOP_STARTUP_ID), and the compositor gives the
child's first window the focus, as it would a window this one opened. Made from the last click or
key this window received: ask right after the input that asked for the launch. `app_id` may be "".
Waits for the compositor's answer. "" when there is none to be had (no xdg_activation, not Wayland).

### `fn scale(w: Window) -> Float`

the display's preferred fractional scale (1.0, 1.5, 2.0, ...). Render your buffer at
logical*scale pixels and lay out in logical coords for crisp HiDPI (the viewport maps it back).

### `fn apply_viewport(w: Window)`

(GPU path) map the physical wgpu buffer to the logical window size. Call after gpu_resize.

### `fn clipboard_set(w: Window, text: Str)`

set the system clipboard (selection) to `text` (UTF-8).

### `fn start_drag(w: Window, text: Str)`

start a drag-and-drop from this window carrying `text` (e.g. a tab id); drop it on another
window's drop target to transfer it across OS windows. Needs a recent press (an input serial).

### `fn start_drag_files(w: Window, paths: Vec<Str>)`

start a drag-and-drop of files from this window: `paths` (absolute) offered as text/uri-list (file:// URIs, the
freedesktop form every file manager, browser and terminal takes) and as plain text (the paths, a line each). Needs
the press that began the drag (an input serial; the button still held). Wayland only; elsewhere nothing.

### `fn file_uri(path: Str) -> Str`

a file:// URI of an absolute path: every byte outside RFC 3986's unreserved set and "/" percent-encoded

### `fn uri_list_paths(text: Str) -> Vec<Str>`

the absolute paths of a dropped text/uri-list (Event::Dropped): its file:// URIs (an empty or "localhost" host),
percent-decoded; comment lines (#) and other schemes are left out

### `fn drag_finished(w: Window) -> Bool`

did the last start_drag from this window complete with a drop elsewhere (the move succeeded)?

### `fn clipboard_get(w: Window) -> Str`

read the system clipboard as text ("" if empty / no offer).

### `fn drawable(w: Window) -> Bool`

Is there a surface to draw on right now? False while an Android app is backgrounded (the system
takes the surface away and hands a new one back on return), and true on a desktop for as long as
the window is open.

Distinct from `should_close`, which means the user is done with this window. A loop that treats
"cannot draw" as "finished" ends when someone switches apps, leaving the activity alive with a
dead program behind it: a black window, and a transparent card in the recents list.

    while !window::should_close(win) {
        ...poll events...
        if window::drawable(win) { ...render, present... }
        window::frame_wait(win, 16)
    }

### `fn lock_pointer(w: Window, on: Bool) -> Bool`

Lock the pointer to the window and hide it, as a first-person camera needs.

Two different mechanisms sit behind this call, because the display servers disagree about
whether an application may move the cursor. X11 lets it: grab the pointer, hide it, and warp it
back to the centre after every read, so the delta is how far it got and it can never reach an
edge. Wayland forbids warping, so the surface backend uses `pointer-constraints-v1`
to pin the cursor and `relative-pointer-v1` to keep delivering motion once it cannot move.

Returns false when the request could not be made at all: no pointer yet, or a Wayland
compositor that does not advertise the two extensions. True is not a promise that the compositor
obliged: Wayland may refuse a lock and offers no way to find out. `pointer_is_locked` reports
what was asked.

### `fn inhibit_shortcuts(w: Window, on: Bool) -> Bool`

Hold the display server's own key bindings off while this window has the keyboard, so every key combination
reaches it (Wayland's keyboard-shortcuts-inhibit): what a settings page that records a new shortcut needs.
The compositor may refuse; false when the request could not be made (no such protocol, another backend).

### `fn pointer_is_locked(w: Window) -> Bool`

was a lock requested and not released?

### `fn pointer_delta(w: Window) -> MouseDelta`

The pointer motion since the last call, in pixels, and cleared by reading.

Only meaningful while locked: unlocked, both backends answer zero and a caller should difference
`pointer_at` instead. The C side carries 1/256 px as ints to keep that surface integer-only, so
the division is here.

### `struct MouseDelta`

what `pointer_delta` answers.

### `fn pointer_at(w: Window) -> MousePos`

Where the pointer is, without consuming anything.

`mouse_pos` reports a completed tap and clears the latch as it answers, so calling it to follow
a finger eats the very tap somebody else is waiting for. This asks only the position, which is
what anything dragged needs on every frame. Logical pixels, like `mouse_pos`.

### `fn pointer_held(w: Window) -> Bool`

Is a finger or button down right now?

Distinct from `mouse_pos().down`, which reports a completed tap once and clears it. That suits
a button but not anything dragged. A scrubber needs to know the touch is still in
progress, and where it is, on every frame; reading this consumes nothing.

### `fn should_close(w: Window) -> Bool`

Is the person done with this window? True once they close it, or once Android is finishing the
activity, but not merely because it went to the background.

That distinction is why `drawable` exists next to it. Android takes the surface away
every time an app is backgrounded, so a loop that ends when it cannot draw ends when someone
switches apps, leaving the activity alive with a dead program inside it.

### `fn close(inout w: Window)`

disconnect and free.


### `fn LAYER_BACKGROUND() -> Int`

The layers, bottom to top. Windows sit between BOTTOM and TOP: a wallpaper is BACKGROUND, a taskbar
is TOP (above the windows) or BOTTOM (below them, which only shows where they leave room), and a lock
screen or a notification is OVERLAY.

### `fn ANCHOR_TOP() -> Int`

The edges a layer surface is anchored to, OR'd together. Anchored to one edge it sits centred along
it; anchored to both edges of an axis it is stretched along that axis (ask for size 0 there).

### `fn KEYBOARD_NONE() -> Int`

Whether a layer surface takes the keyboard: never (a taskbar), always while it is mapped (a lock
screen, a launcher that owns the keys), or when it is clicked like a window (a start menu's search).

### `struct LayerSpec`

How a layer surface asks to be placed. `exclusive_zone` is the strip of its edge it reserves:
windows are laid out beside it, not under it: a taskbar reserves its height. 0 reserves nothing and
-1 asks to be placed over other surfaces' zones too.

### `fn layer_spec(layer: Int, anchor: Int, width: Int, height: Int) -> LayerSpec`

a spec with no exclusive zone, no keyboard and no margins; set the fields that differ.

### `fn open_layer(namespace: Str, spec: LayerSpec) -> Window`

Open a layer surface. `namespace` says what it is ("taskbar", "notifications"): compositors match
their rules on it. The compositor answers with the size it gives the surface before this returns, so
`width`/`height` of the window are that size, e.g. the screen's width for a bar anchored left and right.
`opened` is false when there is no compositor or it has no layer shell.

### `fn layer_set_keyboard(w: Window, mode: Int)`

Change a layer surface's keyboard interactivity while it is shown (KEYBOARD_*). A launcher opens
with EXCLUSIVE, so the keys go to it at once whatever the compositor does on map, and switches to ON_DEMAND once it
has them, so a click elsewhere takes them away (and it can close).

### `fn layer_set_size(w: Window, width: Int, height: Int)`

Ask for another size while a layer surface is shown (a stack of notifications that grew): the compositor
answers, and `poll` reports Resized with the size it gave once that arrives; present the next frame at it. The
surface stays mapped, so there is no empty frame between the two sizes.

### `fn set_input_none(w: Window, none: Bool)`

Let the pointer through a surface (`none`, an empty input region: clicks and hovers reach what is under it, as for
a surface fading out), or take it again over the whole surface. Wayland only; elsewhere nothing.

### `struct Toplevels`

The compositor's toplevels, kept up to date on a connection of its own.

### `struct Toplevel`

One toplevel. `id` is this list's own name for it, stable for as long as the toplevel exists and
never reused; `identifier` is the compositor's (ext_foreign_toplevel_list_v1's), "" when it does not
say one.

### `fn toplevels_open() -> Toplevels`

Connect and take the toplevels there are. `toplevels_ok` says whether the compositor offered a list.

### `fn toplevels_ok(t: Toplevels) -> Bool`

did the compositor give us a window list?

### `fn toplevels_can_act(t: Toplevels) -> Bool`

can the list's toplevels be acted on (activated, closed, minimized)? The ext list alone only lists.

### `fn toplevels_fd(t: Toplevels) -> Int`

the descriptor the list's events arrive on (-1 when there is none)

### `fn toplevels_dispatch(t: Toplevels) -> Int`

Decode whatever has arrived, without waiting. 1 when the list changed (a toplevel came, went, or
changed its title, app id or state), 0 when nothing did, -1 when the compositor is gone.

### `fn toplevels_list(t: Toplevels) -> Vec<Toplevel>`

the toplevels, oldest first

### `fn toplevel_activate(t: Toplevels, id: Int) -> Bool`

Ask the compositor to activate (focus and raise) toplevel `id`, unminimizing it if need be. False
when it is gone, or the compositor only lists toplevels. A request: the toplevel's `activated` state
says what the compositor did.

### `fn toplevel_close(t: Toplevels, id: Int) -> Bool`

ask toplevel `id` to close (as its close button would: the program may ask first)

### `fn toplevel_set_minimized(t: Toplevels, id: Int, on: Bool) -> Bool`

minimize toplevel `id`, or bring it back

### `fn toplevel_set_maximized(t: Toplevels, id: Int, on: Bool) -> Bool`

maximize toplevel `id`, or restore it

### `struct Workspace`

One workspace. `id` is this list's name for it, stable while it exists and never reused; `ident` is
the compositor's own (ext_workspace_handle_v1.id, "" when it gives none); `coords` its position in
its group's grid ("1" for Sway's workspace 1), "" when it has none.

### `fn workspaces_ok(t: Toplevels) -> Bool`

does the compositor offer workspaces (ext_workspace_v1)?

### `fn workspaces(t: Toplevels) -> Vec<Workspace>`

the workspaces, in the order the compositor announced them

### `fn workspace_activate(t: Toplevels, id: Int) -> Bool`

Ask the compositor to switch to workspace `id` (activate it, and commit). False when it is gone or
there are no workspaces. A request: the workspace's `active` says what the compositor did.

### `fn toplevels_close(inout t: Toplevels)`

disconnect and free


