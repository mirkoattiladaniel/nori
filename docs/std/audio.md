# std/audio

```nori
import "std/audio" as audio
```

std/audio/alsa: the Linux ALSA (libasound) PCM playback backend behind `std/audio`'s `Device`.

Why this uses `dlopen` rather than linking. Nori can bind a C library three ways, and
the obvious two don't fit here:

  * `extern alsa "alsa/asoundlib.h" link "asound" { … }` works and is shorter. But `std/audio` is
    imported by `std/src/lib.nori`, which type-checks the whole standard library as one unit, so
    that binding would make ALSA's headers a build dependency of all of std and `libasound` a
    link dependency of every program that touches it. No other std module does that:
    `std/window` and `std/nori_ui`'s wgpu backend both keep their C out of the default build. A
    machine without `libasound2-dev` could not compile std at all.
  * a raw `extern fn` / `std/dl` binding cannot express ALSA's setup path: `snd_pcm_open` takes a
    `snd_pcm_t**` out-parameter and the hardware-parameter API is opaque structs.

So this file uses an inline `extern c { }` block, which the compiler compiles and links itself
(no `--cfile` or `--clink` for the caller). The block (a) declares the eleven libasound entry points it uses with its
own prototypes, needing no ALSA header, (b) resolves them with `dlopen("libasound.so.2")` +
`dlsym` on first use, needing no link library, and (c) owns the `snd_pcm_t*` behind small helpers
so the struct marshalling never crosses into Nori. `available()` is then a real question with a
real answer: false on a machine with no ALSA installed, where the program still runs.

The constants. Not including the header means the four enum values below are spelled out. They
are ALSA ABI, fixed since 1.0 and not free to change.

Writes block (`snd_pcm_writei` on a device opened with mode 0): `write()` returns when the
frames are in the ring buffer, and waits while the buffer is full. This is not a callback/pull
backend and it does not use threads. A game should call `write()` from a loop
that also does other work, sized by `avail()`.

Native builds. Everything above describes a build through LLVM. A program built with
`noric --build --native` has no C compiler, so the `extern c` block below is not compiled there:
std/native_seams links `std/audio/__native_audio.nori` in its place, which answers the same `naud_*`
calls in Nori, needs no libasound, and leaves the executable static. The device name picks the
protocol, mirroring what libasound does with those names on a desktop:

  * "default", "pipewire", "pulse", "sysdefault": the PulseAudio native protocol (version 35)
    over the sound server's Unix socket: `$PULSE_SERVER` (unix:PATH), else
    `$XDG_RUNTIME_DIR/pulse/native`, served by PulseAudio or pipewire-pulse. A write blocks until
    the server's flow control has taken every frame; drain waits for the server to play them out.
  * "hw:C,D", "plughw:C,D": the kernel PCM interface on /dev/snd/pcmC{C}D{D}p through its ioctls.
    `hw:` refuses parameters the card does not take with -EINVAL; `plughw:` converts the sample
    format and channel count and changes the rate by linear interpolation (not band-limited). A
    device the sound server holds answers -EBUSY, and `open_default` moves on.
  * "null": accepts the parameters and discards writes at once, as libasound's null PCM does.

Errors are the same negated errno values with the C library's text, `alsa_available()` is true,
and `alsa_version()` names the native backend and its protocols instead of a libasound version.

Windows. There is no libasound and no C block: the `cfg(windows)` half below defines the `aud_*` entry
points the functions at the end call (on Linux and macOS they forward to the C block's `naud_*`) in
Nori over winmm's waveOut, one implementation for LLVM and native builds alike.
"default", "pipewire", "pulse" and "sysdefault" open the preferred output (WAVE_MAPPER), "hw:C,D" and
"plughw:C,D" waveOut device C, "null" accepts sane parameters and discards writes; any other name
answers -ENOENT. A write queues its frames and blocks while more than the requested latency's worth is
still waiting to be played, as a write to a full ALSA buffer blocks.
### `fn alsa_available() -> Bool`

Is libasound installed and usable on this machine? Everything else in this module answers
-ENOSYS (-38) when this is false, so a program with no ALSA still runs.

### `fn alsa_version() -> Str`

libasound's own version string, e.g. "1.2.14", or "" when it is not installed.

### `fn alsa_strerror(code: Int) -> Str`

ALSA's own text for an error code (which this module reports negated, as ALSA does).

### `fn alsa_set_quiet(on: Bool)`

Silence libasound's stderr chatter. Error codes are unaffected; only the printing stops.

### `fn alsa_open(dev: Str, rate: Int, channels: Int, fmt: Int, latency_us: Int) -> Int`

Open `dev` (an ALSA PCM name: "default", "pipewire", "pulse", "plughw:0,0", "null", …) for
playback. `fmt` is 0 for S16_LE and 1 for FLOAT_LE. Returns an opaque handle > 0, or a negative
ALSA error code — pass it to `alsa_strerror`.

### `fn alsa_can_open(dev: Str, rate: Int, channels: Int, fmt: Int) -> Int`

Would `dev` accept this rate/channels/format? 0 = yes, negative = the ALSA error it refused with.
Opens and closes the device, so it answers about the real driver, not a guess.

### `fn alsa_write_raw(h: Int, ptr: Int, frames: Int) -> Int`

Write `frames` interleaved frames from raw memory at `ptr`. Blocks until they are queued.
Returns frames written, or a negative ALSA error.

### `fn alsa_avail(h: Int) -> Int`

Frames writable right now without blocking.

### `fn alsa_drain(h: Int) -> Int`

Block until the queued audio has played out.

### `fn alsa_drop_pending(h: Int) -> Int`

Discard everything queued, stop now, and leave the stream ready for the next write.

### `fn alsa_close(h: Int) -> Int`

Close the device.

### `fn alsa_abi_const(which: Int) -> Int`

One of the four ALSA enum values this backend hardcodes, so a test can check them against the
real header: 0 = SND_PCM_STREAM_PLAYBACK, 1 = SND_PCM_ACCESS_RW_INTERLEAVED,
2 = SND_PCM_FORMAT_S16_LE, 3 = SND_PCM_FORMAT_FLOAT_LE.


std/audio: PCM sound: a WAV reader/writer, a real output device, and a small voice mixer.

  import "std/audio" as audio

  var s = audio::wav_read(read_file("boing.wav"))          // -> Sound (S16 interleaved)
  var d = audio::open_default(s.rate, s.channels)          // an ALSA playback device
  if audio::ok(d) { audio::play_blocking(d, s)  audio::close(d) }

**The sample format is interleaved signed 16-bit little-endian, held in a `Str`.** A Nori `Str` is
a length-prefixed byte range that may contain NULs (`str_from_buf`), so it is the natural PCM
buffer here: it comes straight out of `read_file`, goes straight into `write_file`, and its
backing pointer can be handed to `snd_pcm_writei` with no copy. `Vec<Int>` would cost 8 bytes per
2-byte sample and a conversion on every write.

Not included: an audio graph, effects, a resampler of its own (ALSA's `soft_resample` covers
rate mismatch) and a callback/pull thread. `Device` writes block. The
mixer (see mixer.nori) sums voices into a buffer that the caller then writes. That is enough for
a game's `audio.play` / `audio.stop` / `audio.set_volume`.

Backends: ALSA (`alsa.nori`) on Linux, resolved with `dlopen` at first use. Importing std/audio
adds no link or header dependency, and on a machine with no libasound every call
answers -ENOSYS instead of the program failing to start. `pipewire` and `pulse` are reached as
ALSA PCM names, which is how both projects expose themselves to ALSA clients; no separate
binding is needed for either.
### `struct Sound`

16-bit signed little-endian interleaved PCM. `data.len()` is always `frames * channels * 2`.

### `struct Device`

An open playback device. `h` is the backend handle (> 0 when open); `rate`/`channels` are what
was actually negotiated, and `err` is the ALSA error code if the open failed (0 otherwise).

### `fn sound(rate: Int, channels: Int, sink data: Str) -> Sound`

A Sound from raw interleaved S16LE bytes.

### `fn frame_bytes(channels: Int) -> Int`

`frames * channels * 2` — the byte length one buffer of `frames` frames occupies.

### `fn frames(s: Sound) -> Int`

How many whole frames a Sound holds.

### `fn duration_ms(s: Sound) -> Int`

Duration in milliseconds.

### `fn sample_at(s: Sound, f: Int, c: Int) -> Int`

Sample `i` of frame `f`, channel `c`, as a signed value in [-32768, 32767].

### `fn silence(rate: Int, channels: Int, n: Int) -> Sound`

Sound of `n` frames of silence.

### `struct WavResult`

The outcome of decoding a WAV: `sound.rate == 0` means it failed and `error` says why.

### `fn wav_decode(b: Str) -> WavResult`

Decode a RIFF/WAVE file into S16 interleaved PCM, with the reason on failure.

### `fn wav_read(b: Str) -> Sound`

Decode a WAV, discarding the diagnostic. A Sound with `rate == 0` means it failed; use
`wav_decode` when you want to know why.

### `fn float_to_s16(f: Float) -> Int`

Round a normalised sample in [-1, 1] to S16, clamping (not wrapping) outside it.

### `fn f32_from_bits(bits: Int) -> Float`

Reassemble an IEEE-754 binary32 from its 32 raw bits (WAV float samples arrive as bits).

### `fn wav_write(s: Sound) -> Str`

Encode a Sound as a 16-bit PCM RIFF/WAVE file.

### `fn default_device_names() -> Vec<Str>`

The PCM device names tried by `open_default`, in order. "default" is first because it is what the
user configured. `pipewire` and `pulse` come next because on a modern desktop they are the real
server, and "default" only reaches them if the distro installed the ALSA plugin config (otherwise
"default" resolves to dmix over `hw:0,0` and fails with ENOENT if card 0 has no playback
device 0, as on an HDMI-only card). `plughw:` entries come last: they are the hardware directly,
with ALSA's conversion plugin in front so a rate the card refuses still works.

### `fn open_device(sink name: Str, rate: Int, channels: Int, latency_us: Int) -> Device`

Open a named PCM device. `latency_us` is the target buffer latency (20000 = 20 ms is a reasonable
game default; larger is safer under load). A failed open returns a Device with `h == 0` and `err`
set to the ALSA code, so `ok()` is false and `error_text()` names it.

### `fn open_default(rate: Int, channels: Int) -> Device`

Open the first device in `default_device_names()` that accepts this rate/channels. Returns a
closed Device carrying the last error if none did.

### `fn open_default_lat(rate: Int, channels: Int, latency_us: Int) -> Device`

`open_default` with an explicit target latency in microseconds.

### `fn ok(d: Device) -> Bool`

Is the device open and usable?

### `fn error_text(d: Device) -> Str`

The ALSA text for a failed open ("" when the device is fine).

### `fn write(d: Device, pcm: Str) -> Int`

Write interleaved S16 frames. Blocks until they are queued (the ring buffer drains in real time,
so writing more than the buffer holds takes as long as the audio lasts). Returns frames written,
or a negative ALSA error. Underruns are recovered internally and the write retried.

### `fn writable(d: Device) -> Int`

Frames writable right now without blocking. Use it to size the next mix.

### `fn play_blocking(d: Device, s: Sound) -> Int`

Play a whole Sound and wait for it to finish. Blocks for its full duration. The Sound's rate and
channel count must match the device's — `play_blocking` does not resample.

### `fn drain(d: Device) -> Int`

Block until everything queued has played out.

### `fn stop(d: Device) -> Int`

Throw away everything queued and stop immediately.

### `fn close(inout d: Device) -> Int`

Close the device.


std/audio/mixer: N simultaneous voices summed into one S16 buffer.

  var m = audio::mixer_new(44100, 2)
  let v = audio::mix_play(m, boing, 800, false)   // -> a voice id;  800 = 0.8 gain
  audio::mix_set_volume(m, v, 400)
  audio::mix_stop(m, v)
  let pcm = audio::mix_render(m, 1024)            // 1024 frames of S16 interleaved
  audio::write(dev, pcm)

This is the whole of what a game's `audio.play` / `audio.stop` / `audio.set_volume` needs.
There are no buses, no effects, no send/return graph and no per-voice panning.

**Gain is an integer per mille** (1000 = unity), not a Float. That keeps the mixing
loop in integer arithmetic, where clipping is exact. Gain above 1000 is allowed and will clip;
there is no limiter.

Rate mismatch between a Sound and the mixer is handled by nearest-neighbour stepping (position
advances by `sound.rate / mixer.rate` per output frame, in fixed point). That is audibly rough
for a large ratio; the better fix is to decode at the device rate, and `mix_play` accepts a Sound that
already matches. Channel mismatch is handled properly: mono fans out to every output channel and
stereo folds down by averaging.
### `struct Voice`

One playing sound. `pos_q16` is the read cursor in source frames, 16.16 fixed point.

### `struct Mixer`

A fixed set of voice slots summed into one output format.

### `fn mixer_new(rate: Int, channels: Int) -> Mixer`

A mixer producing `channels`-channel S16 at `rate` Hz. Voices are added and removed freely; the
slot vector only grows, and a stopped voice's slot is reused.

### `fn mix_play(inout m: Mixer, s: Sound, gain: Int, looping: Bool) -> Int`

Start `s` at `gain` per mille. Returns a voice id (> 0) to pass to `mix_stop` /
`mix_set_volume`, or 0 if the Sound is empty. A `looping` voice never finishes on its own.

### `fn mix_stop(inout m: Mixer, id: Int) -> Bool`

Stop one voice. Returns true if it was playing.

### `fn mix_stop_all(inout m: Mixer)`

Stop everything.

### `fn mix_set_volume(inout m: Mixer, id: Int, gain: Int) -> Bool`

Set one voice's gain in per mille. Returns true if the voice is playing.

### `fn mix_volume(m: Mixer, id: Int) -> Int`

One voice's gain in per mille, or -1 if it is not playing.

### `fn mix_master(inout m: Mixer, gain: Int)`

Set the gain applied to the whole mix, in per mille.

### `fn mix_playing(m: Mixer, id: Int) -> Bool`

Is this voice still playing?

### `fn mix_active(m: Mixer) -> Int`

How many voices are playing.

### `fn mix_render(inout m: Mixer, n: Int) -> Str`

Render `n` frames of the mix as S16 interleaved bytes (`n * channels * 2` of them). Voices that
run off the end of their Sound are retired (or wrapped, if looping). Sums are accumulated in
full-width Int and clamped once at the end, so many loud voices distort rather than wrap.

### `fn mix_pump(inout m: Mixer, d: Device, n: Int) -> Int`

Render `n` frames and write them straight to a device. Blocks if `n` is more than the device's
ring buffer can take — see `mix_pump_ready` for the call a game loop wants instead.

### `fn mix_pump_ready(inout m: Mixer, d: Device, max: Int) -> Int`

Top the device up with as much as it can take right now and no more, so the call
returns promptly and the game loop keeps running. Call this one every frame:

    while running {
        update_world()
        audio::mix_pump_ready(mixer, dev, 0)
        draw()
    }

`max` caps how much is written in one call (0 = no cap), which bounds the work when the device
has drained a lot, after a long frame for example. Returns frames written; 0 means the buffer was
already full, which is the normal case and not an error. A negative value is an ALSA error.


std/audio/mp3: an MP3 (MPEG-1/2 Layer I/II/III) decoder.

  let s = audio::mp3_decode(read_file("music.mp3"))     // -> Sound (S16 interleaved)

Provenance: this is lieff/minimp3 (`minimp3.h`, CC0), translated to Nori in a
safe style: every C pointer became a `Vec` plus an index, so there is no raw memory access
anywhere below and every array read is bounds-checked by Nori.

Limits: whole-buffer decode only: `mp3_decode` takes the entire file and returns the entire
Sound. There is no streaming/incremental API and no seek (minimp3's `mp3dec_ex` layer, which
provides both, is not included). ID3v2 tags are skipped; anything else non-audio is
resynchronised past by minimp3's own frame finder.
### `struct Mp3Info`

What the decoder learned about the stream it just read.

### `fn mp3_decode_info(bytes: Str) -> Mp3Info`

Decode a whole MP3 file, with a diagnostic. `sound.rate == 0` means nothing decoded and `error`
says why. A file that decodes partly (a truncated download) returns what it got, with `error`
empty; the frame count and duration tell you how much.

### `fn mp3_decode(bytes: Str) -> Sound`

Decode a whole MP3 file to S16 interleaved PCM. Returns a Sound with `rate == 0` if no frame
decoded; `mp3_decode_info` after the call explains why.

### `fn mp3_probe(bytes: Str) -> Mp3Info`

Read only the first frame's header: sample rate, channel count, layer, bitrate, without
decoding any audio. Returns `rate == 0` if there is no frame.


std/audio/vorbis: an Ogg Vorbis decoder.

  let s = audio::ogg_decode(read_file("music.ogg"))     // -> Sound (S16 interleaved)

Provenance: this is nothings/stb_vorbis (`stb_vorbis.c`, public domain / MIT), translated to Nori
in an unsafe style. The C pointers stayed pointers: this code
callocs, pokes and peeks raw memory where the C did, including a `pokef32` union type-pun
and `alloca` scoping. It is a C decoder running in Nori with C's memory model, inside `unsafe`.

Limits: whole-buffer decode only (`stb_vorbis_decode_memory`): the file goes in, the Sound comes
out. stb_vorbis's streaming, seeking and pushdata APIs are all present in the code below but are
not exposed, because nothing in std needs them.
### `fn ogg_decode(bytes: Str) -> Sound`

Decode a whole Ogg Vorbis file to S16 interleaved PCM. Returns a Sound with `rate == 0` if the
bytes are not a Vorbis stream this can read.

### `fn ogg_probe(bytes: Str) -> Sound`

Rate and channel count of an Ogg Vorbis stream, without keeping the PCM. Returns `rate == 0` if
it is not readable. (It still decodes: stb_vorbis's one-call API is all that is exposed here.)


