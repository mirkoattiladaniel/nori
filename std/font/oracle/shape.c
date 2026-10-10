// shape.c: HarfBuzz as the reference shaper for std/font's tests.
//
//   shape CASES_FILE > expected.txt
//
// CASES_FILE has one case per line, fields separated by TABs:
//   name  font-path  face-index  script(ISO 15924, e.g. Arab)  dir(ltr|rtl)  lang(BCP 47 or -)
//   features(comma list in hb_feature_from_string syntax, or -)  text(hex codepoints, space separated)
// Lines starting with '#' are comments. For each case it prints
//   case NAME
//   text CP CP ...                      (hex)
//   glyph GID CLUSTER X_ADVANCE Y_ADVANCE X_OFFSET Y_OFFSET   (one per output glyph, visual order)
//   end
// The text is added as UTF-8 so clusters are UTF-8 byte offsets, the unit std/font reports. The font
// is used at its own units per em, unhinted (hb_ot_font), so every number is in font units.
#include <hb.h>
#include <hb-ot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int utf8_put(char *o, unsigned cp) {
    if (cp < 0x80) { o[0] = cp; return 1; }
    if (cp < 0x800) { o[0] = 0xC0 | (cp >> 6); o[1] = 0x80 | (cp & 63); return 2; }
    if (cp < 0x10000) { o[0] = 0xE0 | (cp >> 12); o[1] = 0x80 | ((cp >> 6) & 63); o[2] = 0x80 | (cp & 63); return 3; }
    o[0] = 0xF0 | (cp >> 18); o[1] = 0x80 | ((cp >> 12) & 63); o[2] = 0x80 | ((cp >> 6) & 63); o[3] = 0x80 | (cp & 63); return 4;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: shape CASES\n"); return 2; }
    FILE *cf = fopen(argv[1], "r"); if (!cf) { perror(argv[1]); return 1; }
    static char line[1 << 16];
    while (fgets(line, sizeof line, cf)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = 0;
        char *f[8]; int nf = 0; char *p = line;
        while (nf < 8) { f[nf++] = p; char *t = strchr(p, '\t'); if (!t) break; *t = 0; p = t + 1; }
        if (nf != 8) { fprintf(stderr, "bad case line (%d fields)\n", nf); return 1; }
        hb_blob_t *blob = hb_blob_create_from_file_or_fail(f[1]);
        if (!blob) { fprintf(stderr, "cannot read %s\n", f[1]); return 1; }
        hb_face_t *face = hb_face_create(blob, atoi(f[2]));
        hb_font_t *font = hb_font_create(face);
        hb_ot_font_set_funcs(font);
        hb_font_set_scale(font, hb_face_get_upem(face), hb_face_get_upem(face));
        hb_buffer_t *buf = hb_buffer_create();
        char utf8[1 << 16]; int ul = 0; char tx[1 << 16]; strcpy(tx, f[7]);
        printf("case %s\ntext", f[0]);
        for (char *tok = strtok(tx, " "); tok; tok = strtok(NULL, " ")) {
            unsigned cp = strtoul(tok, NULL, 16); ul += utf8_put(utf8 + ul, cp); printf(" %X", cp);
        }
        printf("\n");
        hb_buffer_add_utf8(buf, utf8, ul, 0, ul);
        hb_buffer_set_script(buf, hb_script_from_string(f[3], -1));
        hb_buffer_set_direction(buf, strcmp(f[4], "rtl") == 0 ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        if (strcmp(f[5], "-") != 0) hb_buffer_set_language(buf, hb_language_from_string(f[5], -1));
        hb_feature_t feats[32]; unsigned nfeat = 0;
        if (strcmp(f[6], "-") != 0) {
            char fs[1024]; strcpy(fs, f[6]);
            for (char *tok = strtok(fs, ","); tok && nfeat < 32; tok = strtok(NULL, ","))
                if (hb_feature_from_string(tok, -1, &feats[nfeat])) nfeat++;
        }
        hb_shape(font, buf, feats, nfeat);
        unsigned n; hb_glyph_info_t *gi = hb_buffer_get_glyph_infos(buf, &n);
        hb_glyph_position_t *gp = hb_buffer_get_glyph_positions(buf, &n);
        for (unsigned i = 0; i < n; i++)
            printf("glyph %u %u %d %d %d %d\n", gi[i].codepoint, gi[i].cluster, gp[i].x_advance, gp[i].y_advance, gp[i].x_offset, gp[i].y_offset);
        printf("end\n");
        hb_buffer_destroy(buf); hb_font_destroy(font); hb_face_destroy(face); hb_blob_destroy(blob);
    }
    return 0;
}
