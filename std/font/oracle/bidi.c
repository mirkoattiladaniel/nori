// bidi.c: FriBidi as the reference for std/font's bidi tests.
//
//   bidi CASES > expected.txt
//
// CASES: one per line, TAB-separated: name  direction(ltr|rtl|auto)  text(hex code points).
// For each, FriBidi resolves the paragraph (fribidi_get_par_embedding_levels_ex) and reorders it as
// one line (fribidi_reorder_line, flags 0: plain UAX #9 L2, no NSM reordering), and prints
//   case NAME
//   para LEVEL
//   levels L L L ...        (one per code point)
//   order I I I ...         (logical indices in visual order)
//   end
#include <fribidi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: bidi CASES\n"); return 2; }
    FILE *cf = fopen(argv[1], "r"); if (!cf) { perror(argv[1]); return 1; }
    static char line[1 << 16];
    while (fgets(line, sizeof line, cf)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        line[strcspn(line, "\n")] = 0;
        char *name = strtok(line, "\t"); char *dir = strtok(NULL, "\t"); char *tx = strtok(NULL, "\t");
        FriBidiChar str[4096]; int n = 0;
        for (char *t = strtok(tx, " "); t && n < 4096; t = strtok(NULL, " ")) str[n++] = strtoul(t, NULL, 16);
        FriBidiCharType types[4096]; FriBidiBracketType bt[4096]; FriBidiLevel lev[4096]; FriBidiStrIndex map[4096];
        fribidi_get_bidi_types(str, n, types);
        fribidi_get_bracket_types(str, n, types, bt);
        FriBidiParType pdir = strcmp(dir, "rtl") == 0 ? FRIBIDI_PAR_RTL : strcmp(dir, "ltr") == 0 ? FRIBIDI_PAR_LTR : FRIBIDI_PAR_ON;
        FriBidiLevel max = fribidi_get_par_embedding_levels_ex(types, bt, n, &pdir, lev);
        if (!max) { fprintf(stderr, "fribidi failed on %s\n", name); return 1; }
        for (int i = 0; i < n; i++) map[i] = i;
        if (!fribidi_reorder_line(0, types, n, 0, pdir, lev, NULL, map)) { fprintf(stderr, "reorder failed on %s\n", name); return 1; }
        printf("case %s\npara %d\nlevels", name, FRIBIDI_DIR_TO_LEVEL(pdir));
        for (int i = 0; i < n; i++) printf(" %d", lev[i]);
        printf("\norder");
        for (int i = 0; i < n; i++) printf(" %d", map[i]);
        printf("\nend\n");
    }
    return 0;
}
