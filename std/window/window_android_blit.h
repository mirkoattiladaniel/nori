/* window_android_blit.h — the pixel conversion behind Android's present path, on its own.
 *
 * Split out of window_android.c for one reason: this is the part that goes wrong quietly. A wrong
 * channel order or a mishandled stride does not fail, it just draws — with red and blue traded, or
 * with every row walking sideways — and neither is visible without a screen to look at. Here it can
 * be checked against known bytes on the host, with no NDK, no device and no surface.
 *
 * Nothing else in the file depends on Android headers, which is what makes that possible.
 */
#ifndef NORI_ANDROID_BLIT_H
#define NORI_ANDROID_BLIT_H
#include <stdint.h>
#include <string.h>
#include <stddef.h>

/* Nori's canvas pixel -> Android's WINDOW_FORMAT_RGBX_8888 pixel.
 *
 * The canvas is 0x00RRGGBB held in a 32-bit word, so in memory the bytes run B,G,R,X — the same
 * layout Wayland calls XRGB8888, which is why the desktop path can memcpy. Android's RGBX_8888 is
 * bytes R,G,B,X, i.e. the word 0xXXBBGGRR. The two are byte-reversed in the colour channels, and no
 * ANativeWindow format matches the canvas, so every pixel is rebuilt rather than copied.
 *
 * The alpha byte is forced to 0xFF: the canvas carries no alpha (its top byte is 0) and a buffer
 * posted with alpha 0 is fully transparent, which on a device reads as "the app drew nothing". */
static inline uint32_t nori_and_px(uint32_t p) {
    return 0xFF000000u | ((p & 0x000000FFu) << 16) | (p & 0x0000FF00u) | ((p >> 16) & 0x000000FFu);
}

/* Convert w*h pixels from `src` (packed, row pitch = w) into `dst` (row pitch = dst_stride pixels).
 *
 * ANativeWindow_Buffer.stride is measured in pixels rather than bytes and is >= width — the pitch
 * the graphics allocator chose, not the one the caller has. Treating the destination as packed skews
 * the image by (stride - width) pixels per row, which looks like a shear rather than an error. */
static inline void nori_and_blit(uint32_t* dst, int dst_stride, const uint32_t* src, int w, int h) {
    for (int y = 0; y < h; y++) {
        const uint32_t* s = src + (size_t)y * (size_t)w;
        uint32_t* d = dst + (size_t)y * (size_t)dst_stride;
        int x = 0;
        /* Two pixels at a time. This runs over the whole window for every frame — four million
         * pixels on a phone, sixty times a second if anything is moving — so the loop's own
         * overhead is a real part of the frame. Both rows are 8-byte aligned here (a canvas comes
         * from malloc, and the graphics buffer's stride is in whole pixels from an allocator that
         * aligns generously), and reading a pair as one 64-bit word halves the loads, the stores
         * and the iterations for the same arithmetic. The odd pixel at the end goes on its own. */
        for (; x + 1 < w; x += 2) {
            uint64_t two;
            memcpy(&two, s + x, 8);
            uint64_t lo = (uint64_t)nori_and_px((uint32_t)two);
            uint64_t hi = (uint64_t)nori_and_px((uint32_t)(two >> 32));
            uint64_t out = lo | (hi << 32);
            memcpy(d + x, &out, 8);
        }
        for (; x < w; x++) d[x] = nori_and_px(s[x]);
    }
}

#endif /* NORI_ANDROID_BLIT_H */
