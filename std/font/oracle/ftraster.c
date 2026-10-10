// ftraster.c: FreeType as the reference rasterizer for std/font's coverage tests.
//
//   ftraster CASES > expected.txt
//
// CASES: one per line, TAB-separated: font-path  face-index  glyph-id  pixel-size.
// For each, FreeType loads the glyph unhinted (FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP), renders it
// in FT_RENDER_MODE_NORMAL (8-bit anti-aliased coverage), and prints
//   raster FONT FACE GID SIZE W H LEFT TOP SUM
//   row HEX...   (H rows of W bytes)
// LEFT/TOP are bitmap_left/bitmap_top: pixels right of the origin and above the baseline, the same
// convention as std/font's GlyphBitmap.
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: ftraster CASES\n"); return 2; }
    FT_Library lib; if (FT_Init_FreeType(&lib)) return 1;
    FILE *cf = fopen(argv[1], "r"); if (!cf) { perror(argv[1]); return 1; }
    char line[4096];
    while (fgets(line, sizeof line, cf)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = 0;
        char *path = strtok(line, "\t"); int face_i = atoi(strtok(NULL, "\t"));
        int gid = atoi(strtok(NULL, "\t")); int size = atoi(strtok(NULL, "\t"));
        FT_Face face;
        if (FT_New_Face(lib, path, face_i, &face)) { fprintf(stderr, "cannot open %s\n", path); return 1; }
        FT_Set_Pixel_Sizes(face, 0, size);
        if (FT_Load_Glyph(face, gid, FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP)) { fprintf(stderr, "load %d failed\n", gid); return 1; }
        FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
        FT_Bitmap *b = &face->glyph->bitmap;
        long sum = 0;
        for (unsigned y = 0; y < b->rows; y++) for (unsigned x = 0; x < b->width; x++) sum += b->buffer[y * b->pitch + x];
        printf("raster %s %d %d %d %u %u %d %d %ld\n", path, face_i, gid, size, b->width, b->rows, face->glyph->bitmap_left, face->glyph->bitmap_top, sum);
        for (unsigned y = 0; y < b->rows; y++) {
            printf("row ");
            for (unsigned x = 0; x < b->width; x++) printf("%02x", b->buffer[y * b->pitch + x]);
            printf("\n");
        }
        FT_Done_Face(face);
    }
    FT_Done_FreeType(lib);
    return 0;
}
