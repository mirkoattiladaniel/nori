#!/usr/bin/env python3
# std/image/make_anim_testdata.py: makes the animated pictures std/image/anim_test.nori reads (testdata/anim/), with the
# host's tools, and prints what the host's decoders make of them: per frame, the delay and the FNV-1a (32-bit) of the
# whole canvas as straight RGBA8, the numbers anim_test.nori compares its own frames to. The decoders here are the
# host's (ImageMagick for GIF, Pillow for APNG, libwebp's anim_dump/dwebp for WebP); they only make and check the
# files, the decoder under test is std/image's own.
#
#   python3 std/image/make_anim_testdata.py        (needs ImageMagick 7, Pillow, img2webp, cwebp, anim_dump, dwebp)
import os, subprocess, sys, tempfile, struct
from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, 'testdata', 'anim')
os.makedirs(out, exist_ok=True)
tmp = tempfile.mkdtemp()

def canon(b):
    """RGBA with every fully transparent pixel made 0,0,0,0 (what colour a hole has is not part of the picture)"""
    b = bytearray(b)
    for k in range(3, len(b), 4):
        if b[k] == 0:
            b[k - 3] = b[k - 2] = b[k - 1] = 0
    return bytes(b)

def fnv(b):
    h = 2166136261
    for x in b:
        h ^= x
        h = (h * 16777619) & 0xffffffff
    return h

def frame(w, h, seed, alpha=False, holes=False):
    """a deterministic picture: bands, a diagonal, a few colours (so palettes stay small), holes when asked"""
    im = Image.new('RGBA', (w, h))
    px = im.load()
    cols = [(220, 40, 40), (40, 200, 60), (30, 60, 220), (240, 220, 40), (250, 250, 250), (20, 20, 20), (200, 80, 200), (60, 210, 210)]
    for y in range(h):
        for x in range(w):
            c = cols[((x // 4) + (y // 3) + seed) % len(cols)]
            if (x + y + seed) % 7 == 0:
                c = cols[(seed + 3) % len(cols)]
            a = 255
            if holes and ((x - w // 2) ** 2 + (y - h // 2) ** 2) < (min(w, h) // 3) ** 2:
                a = 0
            if alpha and a:
                a = 255 if (x // 5 + y // 5) % 3 else 0
            px[x, y] = c + (a,)
    return im

def save(im, name):
    p = os.path.join(tmp, name)
    im.save(p)
    return p

def report(name, frames, delays):
    print('%s: %d frames' % (name, len(frames)))
    for i, (f, d) in enumerate(zip(frames, delays)):
        print('  frame %d delay %d fnv %d' % (i, d, fnv(canon(f))))

# ---- GIF: frames of different sizes and places, the three disposals, transparency, a local colour table ----------
def magick(*args):
    subprocess.run(['magick'] + list(args), check=True)

f0 = save(frame(40, 30, 0).convert('RGB'), 'g0.png')
f1 = save(frame(16, 12, 1, holes=True), 'g1.png')
f2 = save(frame(20, 10, 2), 'g2.png')
f3 = save(frame(12, 20, 3, holes=True), 'g3.png')
f4 = save(frame(30, 8, 4), 'g4.png')
gif = os.path.join(out, 'disposal.gif')
def gframe(f, disp, delay, x, y):
    return ['(', f, '-set', 'dispose', disp, '-set', 'delay', str(delay), '-repage', '40x30+%d+%d' % (x, y), ')']
# the first frame covers the canvas (what the screen is before it, transparent or the background colour, is a
# choice decoders make differently; it is anim_test's own case, not this one's)
magick(*(gframe(f0, 'none', 10, 0, 0) + gframe(f1, 'background', 20, 4, 3) + gframe(f2, 'previous', 30, 18, 15) +
         gframe(f3, 'none', 0, 25, 5) + gframe(f4, 'previous', 5, 2, 20) + ['-loop', '3', gif]))
gifi = os.path.join(out, 'interlaced.gif')
magick(gif, '-interlace', 'GIF', gifi)

def gif_frames(p):
    n = int(subprocess.run(['magick', 'identify', '-format', '%n\n', p], capture_output=True, check=True, text=True).stdout.split()[0])
    raw = subprocess.run(['magick', p, '-coalesce', '-depth', '8', 'rgba:-'], capture_output=True, check=True).stdout
    sz = len(raw) // n
    delays = [int(x) * 10 for x in subprocess.run(['magick', 'identify', '-format', '%T\n', p], capture_output=True, check=True, text=True).stdout.split()]
    delays = [d if d > 10 else 100 for d in delays]
    return [raw[i * sz:(i + 1) * sz] for i in range(n)], delays

for p in (gif, gifi):
    fr, de = gif_frames(p)
    report(os.path.basename(p), fr, de)

# ---- APNG: Pillow writes it (disposal and blend per frame); the default image in the animation and out of it ------
def apng_frames(p):
    im = Image.open(p)
    fr, de = [], []
    for i in range(im.n_frames):
        im.seek(i)
        fr.append(im.convert('RGBA').tobytes())
        de.append(im.info.get('duration', 0))
    return fr, de

a0 = frame(32, 24, 0)
a1 = frame(32, 24, 1, holes=True)
a2 = frame(32, 24, 2, alpha=True)
a3 = frame(32, 24, 5, holes=True)
ap = os.path.join(out, 'blend.png')
# disposal: 0 none, 1 background, 2 previous; blend: 0 source, 1 over
a0.save(ap, save_all=True, append_images=[a1, a2, a3], duration=[100, 50, 200, 40], loop=0,
        disposal=[0, 1, 2, 0], blend=[0, 1, 1, 0])
ap2 = os.path.join(out, 'hidden_default.png')
a0.save(ap2, save_all=True, append_images=[a1, a2], duration=[70, 80, 90], loop=2, default_image=True,
        disposal=[1, 0, 2], blend=[0, 1, 0])
for p in (ap, ap2):
    fr, de = apng_frames(p)
    report(os.path.basename(p), fr, de)

# ---- WebP: lossless and lossy animations (img2webp), lossy and lossless stills with alpha (cwebp) -----------------
w0 = save(frame(48, 32, 0), 'w0.png')
w1 = save(frame(48, 32, 1, holes=True), 'w1.png')
w2 = save(frame(48, 32, 2, alpha=True), 'w2.png')
w3 = save(frame(48, 32, 3), 'w3.png')
wl = os.path.join(out, 'lossless.webp')
subprocess.run(['img2webp', '-loop', '4', '-lossless', '-d', '80', w0, '-d', '120', w1, '-d', '60', w2, '-d', '30', w3, '-o', wl], check=True, capture_output=True)
# a lossy animation with alpha: VP8 frames with ALPH chunks, blended over each other
def photo_frame(w, h, seed, holes):
    im = Image.new('RGBA', (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            a = 0 if holes and ((x - w // 2) ** 2 + (y - h // 2) ** 2) < (h // 3) ** 2 else 255
            px[x, y] = ((x * 5 + y * 3 + seed * 40) & 255, (y * 7 - x * 2 + seed * 90) & 255, ((x ^ y) * 4 + seed * 20) & 255, a)
    return im
m0 = save(photo_frame(48, 32, 0, False), 'm0.png')
m1 = save(photo_frame(48, 32, 1, True), 'm1.png')
m2 = save(photo_frame(48, 32, 2, True), 'm2.png')
wm = os.path.join(out, 'lossy_anim.webp')
subprocess.run(['img2webp', '-loop', '0', '-lossy', '-q', '50', '-d', '100', m0, m1, m2, '-o', wm], check=True, capture_output=True)

def webp_anim_frames(p):
    d = tempfile.mkdtemp()
    subprocess.run(['anim_dump', '-folder', d, '-prefix', 'f', '-pam', p], check=True, capture_output=True)
    fr = []
    for name in sorted(os.listdir(d)):
        data = open(os.path.join(d, name), 'rb').read()
        fr.append(data[data.index(b'ENDHDR\n') + 7:])
    return fr

report('lossless.webp', webp_anim_frames(wl), [80, 120, 60, 30])
lf = webp_anim_frames(wm)
print('lossy_anim.webp: %d frames' % len(lf))
for i, f in enumerate(lf):
    print('  frame %d alpha fnv %d' % (i, fnv(f[3::4])))

# stills: lossy (no alpha, and with alpha: ALPH lossless-compressed, filtered), lossless with alpha
big = frame(100, 70, 4, holes=True)
big.convert('RGB').save(os.path.join(tmp, 'big.png'))
big.save(os.path.join(tmp, 'biga.png'))
# something like a photograph: gradients, waves and noise (every prediction mode gets used)
import random
random.seed(7)
noise = Image.new('RGB', (230, 150))
npx = noise.load()
for y in range(150):
    for x in range(230):
        v = int(128 + 60 * __import__('math').sin(x / 9.0) * __import__('math').cos(y / 13.0))
        npx[x, y] = ((v + random.randint(-20, 20)) & 255, (x + y + random.randint(-10, 10)) & 255, (255 - v + (x * y) % 37) & 255)
noise.save(os.path.join(tmp, 'noise.png'))
stills = [('lossy.webp', ['-q', '60', os.path.join(tmp, 'big.png')]),
          ('lossy_seg.webp', ['-q', '30', '-segments', '4', '-sns', '100', '-f', '60', '-sharpness', '3', os.path.join(tmp, 'big.png')]),
          ('lossy_nostrong.webp', ['-q', '80', '-nostrong', '-f', '40', '-partition_limit', '50', os.path.join(tmp, 'big.png')]),
          ('lossy_photo.webp', ['-q', '75', '-segments', '4', '-sns', '60', os.path.join(tmp, 'noise.png')]),
          ('lossy_alpha.webp', ['-q', '70', '-alpha_filter', 'best', '-alpha_method', '1', os.path.join(tmp, 'biga.png')]),
          ('lossless_alpha.webp', ['-lossless', '-z', '9', os.path.join(tmp, 'biga.png')])]
for name, args in stills:
    p = os.path.join(out, name)
    subprocess.run(['cwebp', '-quiet'] + args + ['-o', p], check=True)
    if 'lossy' in name:
        y = os.path.join(tmp, name + '.yuv')
        subprocess.run(['dwebp', '-quiet', '-nofancy', '-yuv', p, '-o', y], check=True)
        raw = open(y, 'rb').read()
        w, h = Image.open(p).size
        cw, ch = (w + 1) // 2, (h + 1) // 2
        print('%s: %dx%d yuv fnv Y %d U %d V %d' % (name, w, h, fnv(raw[:w * h]), fnv(raw[w * h:w * h + cw * ch]), fnv(raw[w * h + cw * ch:w * h + 2 * cw * ch])))
        if 'alpha' in name:
            alpha = Image.open(p).convert('RGBA').tobytes()[3::4]
            print('  alpha fnv %d' % fnv(alpha))
    else:
        print('%s: rgba fnv %d' % (name, fnv(canon(Image.open(p).convert('RGBA').tobytes()))))

# a simple lossless still (VP8L chunk only) with every transform the encoder likes at its slowest setting
sl = os.path.join(out, 'lossless_photo.webp')
photo = Image.new('RGB', (64, 48))
pp = photo.load()
for y in range(48):
    for x in range(64):
        pp[x, y] = ((x * 4 + y) & 255, (y * 5 + x * 2) & 255, ((x * y) // 8) & 255)
photo.save(os.path.join(tmp, 'photo.png'))
subprocess.run(['cwebp', '-quiet', '-lossless', '-z', '9', os.path.join(tmp, 'photo.png'), '-o', sl], check=True)
print('lossless_photo.webp: rgba fnv %d' % fnv(canon(Image.open(sl).convert('RGBA').tobytes())))

# ---- broken files: truncated, sizes that lie, sizes too large ---------------------------------------------------
def write(name, b):
    open(os.path.join(out, name), 'wb').write(b)
g = open(gif, 'rb').read()
write('bad_truncated.gif', g[:len(g) * 2 // 3])
write('bad_huge.gif', g[:6] + struct.pack('<HH', 65535, 65535) + g[10:])
gl = bytearray(g)
# the second image descriptor's width made 60000 (it lies: the data is a 16-pixel-wide frame's)
def gif_descriptors(b):
    pos, found = 13 + (3 * (2 << (b[10] & 7)) if b[10] & 128 else 0), []
    while pos < len(b) and b[pos] != 0x3b:
        if b[pos] == 0x21:
            pos += 2
            while b[pos]: pos += 1 + b[pos]
            pos += 1
        elif b[pos] == 0x2c:
            found.append(pos)
            f = b[pos + 9]
            pos += 10 + (3 * (2 << (f & 7)) if f & 128 else 0) + 1
            while b[pos]: pos += 1 + b[pos]
            pos += 1
        else:
            break
    return found
struct.pack_into('<H', gl, gif_descriptors(gl)[1] + 5, 60000)
write('bad_lying.gif', bytes(gl))
a = open(ap, 'rb').read()
write('bad_truncated.png', a[:len(a) // 2])
al = bytearray(a)
j = al.index(b'fcTL', al.index(b'IDAT'))
struct.pack_into('>I', al, j + 8, 5000)              # a frame wider than the canvas
write('bad_lying.png', bytes(al))
w = open(wl, 'rb').read()
write('bad_truncated.webp', w[:len(w) * 3 // 5])
wb = bytearray(w)
k = wb.index(b'VP8X')
wb[k + 12:k + 15] = b'\xff\xff\xff'                   # a canvas 16 million pixels wide
write('bad_huge.webp', bytes(wb))
x = open(os.path.join(out, 'lossy.webp'), 'rb').read()
write('bad_truncated_lossy.webp', x[:len(x) // 2])
print('broken files written')
