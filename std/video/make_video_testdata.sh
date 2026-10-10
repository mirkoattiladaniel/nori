#!/usr/bin/env bash
# std/video/make_video_testdata.sh: the streams video_test.nori decodes, made with the host's tools (ffmpeg for the
# sources and the remuxing, libvpx's vpxenc to encode VP9, ffmpeg's libx264 for the H.264 the decoder refuses), and
# what libvpx's own decoder (vpxdec) makes of them: for each stream, its frame count and the FNV-1a (32-bit) of all its
# frames' I420 planes in order. video_test.nori decodes with std/video and must get the same number, bit for bit.
# The host tools only make and check the files; the decoder under test is std/video's own.
#
#   std/video/make_video_testdata.sh        (prints the table to paste into video_test.nori)
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
out="$here/testdata"
t=$(mktemp -d)
trap 'rm -rf "$t"' EXIT
src() { # w h seconds name
    ffmpeg -loglevel error -y -f lavfi -i "testsrc2=size=$1x$2:rate=30,noise=alls=10:allf=t" -t "$3" -pix_fmt yuv420p "$t/$4.y4m"
}
fnv() { python3 -c "
import sys
h = 2166136261
for b in open(sys.argv[1], 'rb').read(): h = ((h ^ b) * 16777619) & 0xffffffff
print(h)" "$1"; }
enc() { # name w h frames src args... : a WebM made by vpxenc, and the reference
    local n=$1 w=$2 h=$3 f=$4 s=$5; shift 5
    vpxenc --codec=vp9 --quiet -w "$w" -h "$h" --limit="$f" "$@" -o "$out/$n.webm" "$t/$s.y4m"
    vpxdec --i420 -o "$t/$n.yuv" "$out/$n.webm"
    local frames=$(( $(stat -c %s "$t/$n.yuv") / (w * h + 2 * ((w + 1) / 2) * ((h + 1) / 2)) ))
    echo "$n.webm $frames $(fnv "$t/$n.yuv")"
}
src 176 144 1 s176
src 302 170 1 s302
src 520 120 1 s520
src 352 288 1 s352
enc basic 176 144 12 s176 --end-usage=q --cq-level=30
enc odd 302 170 8 s302 --end-usage=q --cq-level=24
enc lossless 176 144 3 s176 --lossless=1
enc errres 176 144 10 s176 --error-resilient=1 --end-usage=q --cq-level=34
enc fpar 176 144 10 s176 --frame-parallel=1 --end-usage=q --cq-level=34
enc aq 352 288 8 s352 --aq-mode=3 --end-usage=cbr --target-bitrate=250
enc segq 352 288 8 s352 --aq-mode=1 --end-usage=q --cq-level=36
enc tiles 520 120 6 s520 --tile-columns=1 --tile-rows=1 --end-usage=q --cq-level=36
enc sharp 176 144 10 s176 --sharpness=6 --end-usage=q --cq-level=44
enc rt 352 288 10 s352 --rt --cpu-used=7 --end-usage=cbr --target-bitrate=150 --lag-in-frames=0
# the same stream in three containers (remuxed, not encoded again)
ffmpeg -loglevel error -y -i "$out/basic.webm" -c copy "$out/basic.mp4"
ffmpeg -loglevel error -y -i "$out/basic.webm" -c copy "$out/basic.mkv"
# a codec the decoder refuses
ffmpeg -loglevel error -y -f lavfi -i "testsrc2=size=64x48:rate=10" -t 0.3 -c:v libx264 -pix_fmt yuv420p "$out/h264.mp4"
# broken: cut short, a frame's bytes flipped, a frame header claiming 65536 on a side
python3 - "$out" <<'PY'
import sys, os
out = sys.argv[1]
b = open(os.path.join(out, 'basic.webm'), 'rb').read()
open(os.path.join(out, 'bad_truncated.webm'), 'wb').write(b[:len(b) * 2 // 3])
x = bytearray(b)
for i in range(len(x) // 2, len(x) // 2 + 200, 7): x[i] ^= 0x5a
open(os.path.join(out, 'bad_flipped.webm'), 'wb').write(bytes(x))
# the key frame's frame_size: after the frame marker, profile, show_existing, frame_type, show_frame, error_res
# (8 bits), the sync code (24), colour config (3 + 1 bits for profile 0): the size is 16 + 16 bits at bit 36
k = b.index(b'\x49\x83\x42') - 1
y = bytearray(b)
bits = int.from_bytes(y[k:k + 12], 'big')
total = 96
pos = 36
mask = ((1 << 32) - 1) << (total - pos - 32)
bits = (bits & ~mask) | (0xFFFFFFFF << (total - pos - 32))
y[k:k + 12] = bits.to_bytes(12, 'big')
open(os.path.join(out, 'bad_huge.webm'), 'wb').write(bytes(y))
PY
ls -la "$out" >&2
