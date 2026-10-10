// ftcolor.c: FreeType as the reference for std/font's color glyphs.
//
//   ftcolor CASES > expected.txt
//
// CASES: one per line, TAB-separated: font-path  face-index  glyph-id  pixel-size (0 = the first
// bitmap strike, for CBDT fonts). FreeType loads the glyph with FT_LOAD_COLOR (unhinted): a CBDT
// glyph comes back as its strike's BGRA bitmap, a COLR v0 glyph as its layers blended into BGRA.
// Prints
//   color FONT FACE GID SIZE W H LEFT TOP
//   row RRGGBBAA...   (premultiplied, one 8-hex-digit pixel each, H rows)
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: ftcolor CASES\n"); return 2; }
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
        if (size == 0) { if (FT_Select_Size(face, 0)) { fprintf(stderr, "no strike\n"); return 1; } size = face->available_sizes[0].y_ppem >> 6; }
        else FT_Set_Pixel_Sizes(face, 0, size);
        if (FT_Load_Glyph(face, gid, FT_LOAD_COLOR | FT_LOAD_NO_HINTING)) { fprintf(stderr, "load %d failed\n", gid); return 1; }
        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) { fprintf(stderr, "render %d failed\n", gid); return 1; }
        FT_Bitmap *b = &face->glyph->bitmap;
        if (b->pixel_mode != FT_PIXEL_MODE_BGRA) { fprintf(stderr, "glyph %d is not BGRA (mode %d)\n", gid, b->pixel_mode); return 1; }
        printf("color %s %d %d %d %u %u %d %d\n", path, face_i, gid, size, b->width, b->rows, face->glyph->bitmap_left, face->glyph->bitmap_top);
        for (unsigned y = 0; y < b->rows; y++) {
            printf("row ");
            for (unsigned x = 0; x < b->width; x++) {
                unsigned char *p = b->buffer + y * b->pitch + x * 4;
                printf("%02x%02x%02x%02x", p[2], p[1], p[0], p[3]);
            }
            printf("\n");
        }
        FT_Done_Face(face);
    }
    FT_Done_FreeType(lib);
    return 0;
}
