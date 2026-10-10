// subset.c: cut a small test font out of a system font with HarfBuzz's subsetter.
//
//   subset IN.ttf FACE_INDEX OUT.ttf TEXT_FILE [keep-hints]
//
// Keeps every glyph the UTF-8 text in TEXT_FILE maps to, plus the GSUB closure of those glyphs
// (ligatures, alternates, contextual forms), with all layout features and scripts retained. The
// tests shape against the subset, and expected values are generated from the subset itself, so the
// glyph renumbering the subsetter does is harmless. Hinting is dropped unless "keep-hints" is given
// (CFF tests keep it so the charstring parser sees hstem/hintmask operators).
#include <hb.h>
#include <hb-subset.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *slurp(const char *p, long *n) {
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(1); }
    fseek(f, 0, SEEK_END); *n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = malloc(*n + 1); if (fread(b, 1, *n, f) != (size_t)*n) { perror("read"); exit(1); }
    b[*n] = 0; fclose(f); return b;
}

int main(int argc, char **argv) {
    if (argc < 5) { fprintf(stderr, "usage: subset IN FACE OUT TEXT [keep-hints]\n"); return 2; }
    hb_blob_t *blob = hb_blob_create_from_file_or_fail(argv[1]);
    if (!blob) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    hb_face_t *face = hb_face_create(blob, atoi(argv[2]));
    long tn; char *text = slurp(argv[4], &tn);
    hb_subset_input_t *in = hb_subset_input_create_or_fail();
    hb_set_t *uni = hb_subset_input_unicode_set(in);
    // decode UTF-8 by hand: the text files are ours and well formed
    for (long i = 0; i < tn;) {
        unsigned char c = text[i]; unsigned cp; int k;
        if (c < 0x80) { cp = c; k = 1; } else if (c < 0xE0) { cp = c & 31; k = 2; }
        else if (c < 0xF0) { cp = c & 15; k = 3; } else { cp = c & 7; k = 4; }
        for (int j = 1; j < k; j++) cp = (cp << 6) | (text[i + j] & 63);
        if (cp != '\n') hb_set_add(uni, cp);
        i += k;
    }
    hb_set_t *feats = hb_subset_input_set(in, HB_SUBSET_SETS_LAYOUT_FEATURE_TAG);
    hb_set_clear(feats); hb_set_invert(feats);                 // every feature
    hb_set_t *scripts = hb_subset_input_set(in, HB_SUBSET_SETS_LAYOUT_SCRIPT_TAG);
    hb_set_clear(scripts); hb_set_invert(scripts);             // every script
    unsigned flags = HB_SUBSET_FLAGS_DEFAULT;
    if (!(argc > 5 && strcmp(argv[5], "keep-hints") == 0)) flags |= HB_SUBSET_FLAGS_NO_HINTING;
    hb_subset_input_set_flags(in, flags);
    hb_face_t *out = hb_subset_or_fail(face, in);
    if (!out) { fprintf(stderr, "subset failed for %s\n", argv[1]); return 1; }
    hb_blob_t *ob = hb_face_reference_blob(out);
    unsigned len; const char *data = hb_blob_get_data(ob, &len);
    FILE *f = fopen(argv[3], "wb"); fwrite(data, 1, len, f); fclose(f);
    fprintf(stderr, "%s: %u bytes, %u glyphs\n", argv[3], len, hb_face_get_glyph_count(out));
    return 0;
}
