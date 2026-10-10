# std/video

```nori
import "std/video" as video
```





std/video: video files decoded on the CPU in Nori: the containers Matroska/WebM (mkv.nori) and MP4/ISO
BMFF (mp4.nori), read by offset (a packet at a time, never the whole file), and the VP9 codec (vp9*.nori).
  import "std/video" as video
  var v = video::video_open("clip.webm")
  if video::video_ok(v) {
      while video::video_next(v) {                       // decodes until the next frame to show
          video::video_to_xrgb(v, buffer, stride)        // the frame, converted to XRGB
          wait(video::video_frame_ms(v))                 // its duration
      }
      video::video_rewind(v)                             // and again from the first frame
  } else { say(video::video_error(v)) }                  // e.g. "H.264 video: not supported yet"
  video::video_close(v)
A file whose codec this decoder lacks opens (its container is read) and says so: video_codec names it, video_ok is
false, and video_error says "H.264 video: not supported yet". Only VP9 is decoded; AV1, H.264 and H.265 are not.

Threads. Decoding (video_next) and converting (video_to_xrgb) share their work out with fork/join (`parallel`): a
frame's tile columns, the loop filter's planes, the conversion's rows. That makes them, and the functions calling
them, suspendable, so call them in a statement (not a `while` condition) from a function that takes no struct by
parameter and returns none (keep the Video in a global).
### `struct Video`

an open video

### `fn video_open(path: Str) -> Video`

open a video file: its container read (the track, its codec, its size), its decoder made when the codec is one
this module decodes

### `fn video_open_fd(fd: Int) -> Video`

the same over a file already open (a sandboxed reader handed it as a descriptor); video_close closes it

### `fn video_ok(v: Video) -> Bool`

can its frames be decoded?

### `fn video_error(v: Video) -> Str`

why not (or why the last frame did not decode)

### `fn video_codec(v: Video) -> Str`

the codec: "vp9", "vp8", "av1", "h264", "hevc", "mpeg4" or what the container calls it

### `fn video_container(v: Video) -> Str`

the container: "webm" (Matroska), "mp4", or ""

### `fn video_width(v: Video) -> Int`

the size the container says (the frames may say otherwise; video_frame_w/h are what was decoded)

### `fn video_frame_ms(v: Video) -> Int`

the shown frame's time and duration (ms)

### `fn video_frames(v: Video) -> Int`

frames shown since it was opened (or rewound)

### `fn video_frame_w(v: Video) -> Int`

the decoded frame's size

### `fn video_take_decode_us(inout v: Video) -> Int`

the time spent decoding since the last call (microseconds)

### `fn video_set_threads(v: Video, n: Int)`

decode tiles on this many threads

### `fn video_next(inout v: Video) -> Bool`

decode until the next frame to show. false at the end of the file or on an error (video_error says).

### `fn video_rewind(inout v: Video) -> Bool`

back to the first frame (Matroska: the first cluster the Cues point at; MP4: sample 0). The decoder starts again
at the first key frame.

### `fn video_close(inout v: Video)`

let everything go

### `fn video_to_xrgb(v: Video, dst: Int, stride: Int)`

the shown frame converted to XRGB words (0x00RRGGBB) at `dst` (rows `stride` bytes apart): BT.709 for HD and
larger frames, BT.601 for smaller ones (VP9's own color_space when it says one), limited range unless the frame
says full; chroma interpolated from the four nearest samples

### `fn yuv420_to_xrgb(yp: Int, up: Int, vp: Int, ys: Int, uvs: Int, w: Int, h: Int, bt709: Bool, full: Bool, dst: Int, stride: Int)`

4:2:0 to XRGB. bt709: the HD matrix, else BT.601; full: 0-255 range, else 16-235/16-240. The products are looked
up (a table per term), the chroma interpolated between its two nearest rows (3:1) and then its two nearest columns
(3:1), and the rows shared out between four threads.

### `fn video_probe(path: Str) -> Str`

what a file is, without decoding anything: "webm vp9 1920x1080", "mp4 h264 1280x720", or "" with why in `why`






### `fn vp9_new() -> V9`

a new decoder (frames are made when the first key frame says their size)

### `fn vp9_free(d: V9)`

let everything go

### `fn vp9_decode(d: V9, p: Int, n: Int) -> Int`

decode a packet (one frame, or a superframe of several: Annex B). Returns the number of frames to show (0 or 1:
the last shown one is in d.out), or -1 with d.err said.

### `fn vp9_out_y(d: V9) -> Int`

the frame to show after vp9_decode said 1: its planes (Y, U, V), strides and size

### `fn vp9_set_threads(d: V9, n: Int)`

decode tiles on this many threads (1: on the caller's)








