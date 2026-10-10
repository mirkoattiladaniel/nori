# std/tui

```nori
import "std/tui" as tui
```

std/tui: terminal UI. ANSI rendering primitives, a double-buffered screen you draw into, raw-mode
key input (Linux), and image/thumbnail blitting. The ANSI builders and the Screen are pure (they
build/return strings, so they're testable headlessly); raw mode + key reading need a real terminal
(cfg(linux)).

  import "std/tui" as tui
  fn main() -> Int {
    var saved = tui::begin()                 // alt screen + hide cursor + raw mode
    var s = tui::screen(40, 10)
    s.box_ex(0, 0, 40, 10, tui::rgb(80, 200, 255), 3)   // rounded box, truecolor
    s.put_ex(2, 1, "héllo, nori", tui::rgb(120, 255, 120), 0 - 1, tui::BOLD())
    tui::draw(s)
    var _k = tui::read_key()                  // blocks for one keypress
    tui::end(saved)                           // restore the terminal
    return 0
  }

Colors: a color is an Int, either a basic SGR foreground code (BLACK()..WHITE(), 30..37) or a 24-bit
truecolor value from rgb(r,g,b). A background slot of `0 - 1` means "terminal default". Attributes are
a bitmask (BOLD|DIM|ITALIC|UNDERLINE|REVERSE) OR'd together.
### `fn esc() -> Str`

the ESC byte (chr(27)) as a 1-char string.

### `fn BLACK() -> Int`

SGR foreground code for black (30).

### `fn RED() -> Int`

SGR foreground code for red (31).

### `fn GREEN() -> Int`

SGR foreground code for green (32).

### `fn YELLOW() -> Int`

SGR foreground code for yellow (33).

### `fn BLUE() -> Int`

SGR foreground code for blue (34).

### `fn MAGENTA() -> Int`

SGR foreground code for magenta (35).

### `fn CYAN() -> Int`

SGR foreground code for cyan (36).

### `fn WHITE() -> Int`

SGR foreground code for white (37).

### `fn DEFAULT() -> Int`

SGR foreground code for the terminal default color (39).

### `fn rgb(r: Int, g: Int, b: Int) -> Int`

pack r,g,b (0..255) into a truecolor value usable anywhere a color Int is expected.

### `fn is_rgb(c: Int) -> Bool`

is this color a truecolor value (vs. a basic SGR code)?

### `fn BOLD() -> Int`

attribute bit: bold.

### `fn DIM() -> Int`

attribute bit: dim/faint.

### `fn ITALIC() -> Int`

attribute bit: italic.

### `fn UNDERLINE() -> Int`

attribute bit: underline.

### `fn REVERSE() -> Int`

attribute bit: reverse video (swap fg/bg).

### `fn GFX_AUTO() -> Int`

graphics mode: choose from the environment on each blit (the default).

### `fn GFX_NONE() -> Int`

graphics mode: skip image blits entirely.

### `fn GFX_HALF() -> Int`

graphics mode: truecolor half-blocks (portable to any 24-bit terminal).

### `fn GFX_KITTY() -> Int`

graphics mode: Kitty graphics protocol (pixel-perfect).

### `fn text_width(s: Str) -> Int`

display width of `s` in terminal columns (wide CJK/emoji count as 2, combining marks as 0).

### `fn EV_NONE() -> Int`

event kind: no event / EOF.

### `fn EV_KEY() -> Int`

event kind: a keypress; see the `key` field (same codes as read_key()).

### `fn EV_MOUSE_DOWN() -> Int`

event kind: a mouse button was pressed (`button`, `col`, `row`).

### `fn EV_MOUSE_UP() -> Int`

event kind: a mouse button was released.

### `fn EV_MOUSE_DRAG() -> Int`

event kind: the mouse moved with a button held (drag).

### `fn EV_SCROLL() -> Int`

event kind: the scroll wheel moved; see `button` (SCROLL_UP / SCROLL_DOWN).

### `fn MB_LEFT() -> Int`

mouse button: left.

### `fn MB_MIDDLE() -> Int`

mouse button: middle.

### `fn MB_RIGHT() -> Int`

mouse button: right.

### `fn SCROLL_UP() -> Int`

scroll direction (in `button` when kind == EV_SCROLL): up.

### `fn SCROLL_DOWN() -> Int`

scroll direction (in `button` when kind == EV_SCROLL): down.

### `fn KEY_UP() -> Int`

arrow up (ESC[A).

### `fn KEY_DOWN() -> Int`

arrow down (ESC[B).

### `fn KEY_RIGHT() -> Int`

arrow right (ESC[C).

### `fn KEY_LEFT() -> Int`

arrow left (ESC[D).

### `fn KEY_HOME() -> Int`

Home (ESC[H or ESC[1~).

### `fn KEY_END() -> Int`

End (ESC[F or ESC[4~).

### `fn KEY_PGUP() -> Int`

Page Up (ESC[5~).

### `fn KEY_PGDN() -> Int`

Page Down (ESC[6~).

### `fn KEY_INS() -> Int`

Insert (ESC[2~).

### `fn KEY_DEL() -> Int`

Delete (ESC[3~).

### `fn KEY_ESC() -> Int`

Escape (a lone ESC, or any unrecognized escape sequence).

### `struct Event`

one input event from read_event(). For EV_KEY, `key` holds the byte/arrow code (as read_key()); for
mouse kinds, `col`/`row` are 0-based cell coordinates and `button` is a MB_* button or, for EV_SCROLL,
a SCROLL_* direction.

### `fn clear() -> Str`

erase the whole screen and move the cursor home (ESC[2J ESC[H).

### `fn home() -> Str`

move the cursor to the top-left (ESC[H).

### `fn move_to(row: Int, col: Int) -> Str`

move the cursor to (row, col), both 1-based (ESC[row;colH).

### `fn fg(code: Int) -> Str`

set the foreground SGR color to `code` (ESC[<code>m).

### `fn bg(code: Int) -> Str`

set the background SGR color to `code` (uses code+10, ESC[<code+10>m).

### `fn fg_rgb(r: Int, g: Int, b: Int) -> Str`

set the foreground to a 24-bit color (ESC[38;2;r;g;bm).

### `fn bg_rgb(r: Int, g: Int, b: Int) -> Str`

set the background to a 24-bit color (ESC[48;2;r;g;bm).

### `fn reset() -> Str`

reset all SGR attributes to defaults (ESC[0m).

### `fn hide_cursor() -> Str`

hide the cursor (ESC[?25l).

### `fn show_cursor() -> Str`

show the cursor (ESC[?25h).

### `fn alt_enter() -> Str`

switch to the alternate screen buffer (ESC[?1049h).

### `fn alt_exit() -> Str`

leave the alternate screen buffer, restoring the primary one (ESC[?1049l).

### `fn mouse_on() -> Str`

enable mouse reporting: button press/release + drag, with SGR extended coordinates
(ESC[?1000;1002;1006h). Send this (or use begin_mouse) before reading mouse events with read_event.

### `fn mouse_off() -> Str`

disable mouse reporting (the counterpart of mouse_on()).

### `fn screen(w: Int, h: Int) -> Screen`

allocate a w*h Screen with every cell a space in the default color.

## Screen

### `fn use_graphics(inout self, mode: Int)`

pin the image-drawing mode for this screen (GFX_AUTO/NONE/HALF/KITTY). Call once with
detect_graphics() for a live-probed result instead of the AUTO environment heuristic.

### `fn wipe(inout self)`

reset every cell to a space in the default color and drop any queued image overlays.

### `fn cell(self, x: Int, y: Int, ch: Str, color: Int)`

set the glyph and foreground color of the cell at (x, y); out-of-bounds writes are ignored.

### `fn cell_ex(self, x: Int, y: Int, ch: Str, fgc: Int, bgc: Int, at: Int)`

set glyph, fg color, bg color (`0 - 1` = default) and attribute mask of the cell at (x, y).

### `fn put(inout self, x: Int, y: Int, s: Str, color: Int)`

write string `s` left-to-right from (x, y), one cell per Unicode code point, in `color`.

### `fn put_ex(self, x: Int, y: Int, s: Str, fgc: Int, bgc: Int, at: Int)`

write string `s` (UTF-8 aware) from (x, y) with fg color, bg color and attribute mask. Advances by
each code point's display width: double-width CJK/emoji take two cells (the second is reserved),
combining/zero-width marks take none.

### `fn fill(self, x: Int, y: Int, w: Int, h: Int, bgc: Int)`

fill a w*h rectangle at (x, y) with spaces on background color `bgc` (a colored panel).

### `fn box(self, x: Int, y: Int, w: Int, h: Int, color: Int)`

draw an ASCII box border (corners '+', edges '-'/'|') of size w*h with top-left at (x, y).

### `fn box_ex(self, x: Int, y: Int, w: Int, h: Int, color: Int, style: Int)`

draw a box of size w*h at (x, y). style: 0 ASCII (+-|), 1 single line, 2 double line, 3 rounded.

### `fn frame(self) -> Str`

build the full ANSI string that paints this screen: cleared, absolute-positioned, with fg/bg/attr
runs coalesced, then any queued image overlays.

### `fn draw(s: Screen) -> Int`

paint a screen to the terminal (one frame). printl adds a trailing newline; the frame positions
every cell absolutely, so the only effect is the cursor ending on a fresh line.

### `fn blit_image(inout s: Screen, im: image::Image, x: Int, y: Int, cols: Int, rows: Int)`

draw a decoded image into the screen at cell (x, y), scaled to fit `cols` x `rows` character cells,
honoring the screen's graphics mode (GFX_AUTO uses the environment heuristic). The Kitty path queues
a pixel-perfect overlay (emitted when the frame is drawn); otherwise truecolor half-block cells over a
black background. `im` may be disposed right after this call returns (both paths copy what they need).

### `fn blit_half_image(s: Screen, im: image::Image, x: Int, y: Int, cols: Int, rows: Int)`

render `im` as truecolor half-block cells (2 vertical pixels per cell), transparent pixels over black.
Portable to any truecolor terminal; use this to force the fallback even where Kitty is available (e.g.
tmux without passthrough).

### `fn blit_half_image_over(s: Screen, im: image::Image, x: Int, y: Int, cols: Int, rows: Int, bg_color: Int)`

like blit_half_image, but composite transparent pixels over `bg_color` (a truecolor rgb() value; a
basic SGR code is treated as black). Use this so thumbnails blend into your panel instead of black.

### `fn kitty_image_seq(im: image::Image, cols: Int, rows: Int) -> Str`

build the Kitty graphics escape that displays `im` scaled to cols x rows cells, without drawing it.
Pair with add_overlay to place it. Useful for caching an expensive image so it isn't re-encoded every
frame. Returns "" if `im` is bad or the payload can't be built.

### `fn add_overlay(inout s: Screen, sink seq: Str, col: Int, row: Int)`

queue a prebuilt overlay escape `seq` (e.g. from kitty_image_seq) at cell (col, row); it is emitted
when the screen is drawn. A no-op for an empty `seq`.

### `fn blit_kitty_image(inout s: Screen, im: image::Image, x: Int, y: Int, cols: Int, rows: Int)`

queue `im` as a Kitty graphics overlay at cell (x, y), scaled to cols x rows. Falls back to half-block
if the payload can't be built. Only visible on Kitty-capable terminals.

### `fn supports_kitty() -> Bool`

does the current terminal support the Kitty graphics protocol (kitty, ghostty, wezterm)? A cheap
env-based heuristic (TERM / KITTY_WINDOW_ID / TERM_PROGRAM); for a definitive answer use
detect_graphics(), which asks the terminal directly.

### `fn clear_images() -> Str`

the Kitty escape that removes all currently-displayed images. clear() (ESC[2J) does not erase Kitty
graphics, so emit this each frame before redrawing to avoid ghost thumbnails. No-op on other terminals.

### `fn detect_graphics() -> Int`

probe the terminal for graphics support by sending a Kitty query + DA1 and reading the reply.
Returns GFX_KITTY if the terminal acknowledges Kitty graphics, else GFX_HALF. Toggles raw mode
around the probe, so it is safe to call before or after begin(). Non-tty stdin returns GFX_HALF.

### `fn raw_enter() -> Int`

enter cbreak/raw mode on stdin (no echo, no line buffering); returns a handle holding the original termios; pass it to raw_exit to restore.

### `fn raw_exit(t: Int) -> Int`

restore the original terminal mode from the handle `t` returned by raw_enter, and free it. A handle
of 0, not one raw_enter gave, does nothing and returns -1, rather than hand libc a null pointer:
`tui::raw_exit(0)` segfaulted a program that called it to "reset" the terminal.

### `fn read_key() -> Int`

block for one keypress and return its byte; arrow keys (ESC[A/B/C/D) return 1000+final byte (UP=1065 DOWN=1066 RIGHT=1067 LEFT=1068); EOF/error returns -1.

### `fn read_event() -> Event`

block for one input event: a keypress or an SGR mouse event (press/release/drag/scroll). For a key,
`key` is the byte, or a named code for a navigation key: arrows KEY_UP/DOWN/LEFT/RIGHT and
KEY_HOME/END/PGUP/PGDN/INS/DEL (the multi-byte CSI escapes are decoded whole: no stray '~' leaks).
Mouse events only arrive after mouse_on()/begin_mouse(). EOF/error yields EV_NONE. Any unrecognized
escape sequence is consumed and reported as KEY_ESC.

### `fn input_pending() -> Bool`

true if input is already waiting on stdin (non-blocking poll). Use to debounce expensive work —
e.g. skip decoding an image preview while the user is holding a key or scrolling.

### `fn term_size() -> Int`

the terminal size packed as (rows << 16) | cols via TIOCGWINSZ, or 0 if unavailable.

### `fn term_rows() -> Int`

the terminal height in rows (high 16 bits of term_size), or 0 if unavailable.

### `fn term_cols() -> Int`

the terminal width in columns (low 16 bits of term_size), or 0 if unavailable.

### `fn stdin_is_tty() -> Bool`

true if stdin is connected to a terminal (isatty(0)).

### `fn begin() -> Int`

begin a full-screen session (alt screen + hide cursor + raw mode); returns the saved-termios handle for end().

### `fn begin_mouse() -> Int`

like begin(), but also enable mouse reporting so read_event() delivers mouse events.

### `fn end(saved: Int) -> Int`

end the session: restore terminal mode from `saved`, disable mouse reporting, show the cursor, and
leave the alternate screen. Safe to call whether or not mouse reporting was enabled.


