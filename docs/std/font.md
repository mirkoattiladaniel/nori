# std/font

```nori
import "std/font" as font
```

### `fn BIDI_AUTO() -> Int`

paragraph direction from the text (P2/P3)

### `fn BIDI_LTR() -> Int`

left-to-right paragraph

### `fn BIDI_RTL() -> Int`

right-to-left paragraph

### `struct BidiResult`

a resolved paragraph: per code point its level (-1 for a character removed by X9) and its
original bidi class; `para_level` is the paragraph embedding level (0 or 1).

### `fn bidi_paragraph_level(cps: Vec<Int>) -> Int`

the paragraph level P2/P3 gives `cps` (0 when there is no strong character)

### `fn bidi_resolve(cps: Vec<Int>, dir: Int) -> BidiResult`

resolve the levels of `cps` (one or more paragraphs, split at paragraph separators) with
paragraph direction `dir` (BIDI_AUTO/BIDI_LTR/BIDI_RTL). Each paragraph is also treated as one
line for L1; call bidi_line_levels for the levels of a narrower line.

### `fn bidi_line_levels(r: BidiResult, from: Int, to: Int) -> Vec<Int>`

the levels of the line [from, to) of a resolved paragraph, with L1 applied at the line's end
(whitespace before a line break returns to the paragraph level).

### `fn bidi_visual_order(r: BidiResult, from: Int, to: Int) -> Vec<Int>`

L2: the logical indices of the line [from, to) in visual (left-to-right) order; characters
removed by X9 are left out.

### `fn bidi_reorder(lv: Vec<Int>, base: Int) -> Vec<Int>`

L2 over an explicit level list (entries of -1 are skipped): indices `base + i` in visual order.




### `struct ColorBitmap`

a color glyph: `w` x `h` pixels of premultiplied RGBA packed as 0xRRGGBBAA (row-major), placed
like a GlyphBitmap: `left` px right of the pen, `top` px above the baseline.

### `fn font_has_color(f: Font) -> Bool`

does the face carry color glyphs (CBDT/CBLC or COLR/CPAL)?

### `fn glyph_is_color(f: Font, gid: Int) -> Bool`

is glyph `gid` a color glyph (a CBDT bitmap or a COLR base glyph)?

### `fn cpal_color(f: Font, idx: Int) -> Int`

palette entry `idx` of palette 0 as packed straight RGBA (0xRRGGBBAA); CPAL stores BGRA.

### `fn colr_layers(f: Font, gid: Int, fg: Int) -> Vec<Int>`

the COLR v0 layers of `gid` as (glyph, packed straight RGBA) pairs, bottom first; `fg` is the
color for layers that ask for the foreground. Empty for a glyph that is not a COLR base glyph.

### `fn rasterize_color(f: Font, gid: Int, px_size: Float, fg: Int) -> ColorBitmap`

draw color glyph `gid` at `px` pixels per em: a CBDT bitmap scaled from its strike, or COLR v0
layers composited (`fg` is the packed straight RGBA for foreground layers). A 0x0 bitmap when
the glyph has no color form.

### `fn color_at(cb: ColorBitmap, x: Int, y: Int) -> Int`

the premultiplied RGBA (packed 0xRRGGBBAA) at (x, y) of a color bitmap; 0 outside it.

### `struct ColorSpan`

a cached color glyph addressed in the cache: read pixels with color_span_at.

### `fn rasterize_color_span(f: Font, gid: Int, px_size: Float, fg: Int) -> ColorSpan`

`rasterize_color`, memoized per (face, glyph, size, foreground), as a span.

### `fn color_span_at(s: ColorSpan, i: Int) -> Int`

premultiplied RGBA pixel `i` (row-major) of a cached color glyph.

### `fn color_cache_stats() -> Vec<Int>`

the color cache's hit, miss, reset and held-pixel counts so far (for measuring).

### `fn color_cache_clear()`

drop every cached color glyph span.

### `fn color_cache_set_budget(b: Int)`

the pixel budget for the color glyph cache (a smaller value makes budget resets observable in tests).


### `struct FontFaceInfo`

one face found on disk: where it is, what it is called, and its style.

### `fn system_font_dirs() -> Vec<Str>`

the directories fonts live in on this system: /usr/share/fonts, /usr/local/share/fonts, and the
user's ~/.local/share/fonts and ~/.fonts: the ones that exist.

### `fn scan_font_dirs(dirs: Vec<Str>) -> Vec<FontFaceInfo>`

every face in every font file under `dirs` (recursively), sorted by path then face index.

### `fn scan_font_file(path: Str) -> Vec<FontFaceInfo>`

the faces of one font file (several for a collection; none if it is not a font).

### `fn font_face_info(f: Font) -> FontFaceInfo`

what a loaded face says about itself (family, style, weight, slant, monospace, colour) as
discovery would report it (no path; coverage questions need a scanned face).

### `fn name_table_string(nm: Str, name_id: Int) -> Str`

string `name_id` of a `name` table held at the start of `nm`: a Windows English (3,1,0x409)
record first, then any Windows Unicode record, then Mac Roman. UTF-16 is decoded to UTF-8 in
full (surrogate pairs included), so non-Latin family names come through. "" when absent.

### `fn match_face(faces: Vec<FontFaceInfo>, family: Str, weight: Int, italic: Bool) -> Int`

the face of `family` (case-insensitive) closest to `weight` and `italic`, or -1 when the family
is not there. Closeness is CSS's: the right slant first, then the nearest weight.

### `fn face_covers(fi: FontFaceInfo, cp: Int) -> Bool`

does face `fi` map code point `cp` to a glyph? Reads and keeps the face's cmap on first use.

### `fn load_face(fi: FontFaceInfo) -> FontResult`

load face `fi` (its whole file) as a Font.

### `fn fontset_for_text(faces: Vec<FontFaceInfo>, primary: Int, text: Str) -> FontSet`

a FontSet for `text`: the primary face `primary` (an index into `faces`, or -1 for none), then,
for each character no face so far has, the best face that does: a color face first for emoji,
else the one closest to the primary's weight and slant, preferring the primary's family name
prefix (so "Noto Sans" pulls in "Noto Sans Arabic" before an unrelated family).

### `fn utf8_encode_cp(cp: Int) -> Str`

one code point as UTF-8.


std/font: a text engine that takes fonts in and gives shaped, rasterized text out.

  import "std/font" as font
  var fr = font::font_load(read_file("DejaVuSans.ttf"))
  if fr.ok {
    var gid = font::font_glyph_index(fr.font, ord("A"))
    var gb  = font::rasterize(fr.font, gid, 32.0)     // 8-bit coverage bitmap at 32px
    let gs  = font::shape(fr.font, "office", "Latn", font::DIR_LTR(), "en", "")   // HarfBuzz-model shaping
  }

Loading: TrueType (`glyf`) and OpenType/CFF outlines (Type 2 charstrings, CID-keyed too),
`.ttc` collections (font_load_index), WOFF and WOFF2. Outlines rasterize to grayscale coverage
via a signed-area accumulator, with light vertical autohinting; color glyphs (CBDT/CBLC PNG
strikes, COLR/CPAL v0 layers) rasterize to premultiplied RGBA (color.nori).

Text: shape.nori is HarfBuzz's OpenType pipeline (GSUB/GPOS in otl_apply.nori, the plan in
shape_plan.nori; default, Arabic, Hebrew, Hangul and Indic shapers), bidi.nori is UAX #9,
linebreak.nori UAX #14, textrun.nori itemizes a line into bidi/script/font runs over a FontSet and
lays them out (layout_run and measure_run are thin wrappers over it), fontdb.nori discovers the
system's fonts without fontconfig.
Out of scope: variable fonts beyond the default instance, vertical text, hinting bytecode.

Bytes: the font is held as a `Str` (length-prefixed in the runtime, so embedded NULs are fine)
and read big-endian via ord(char_at) + band/bor/shl; reads past the end are zero, so a malformed
font draws nothing rather than trapping.
### `fn rd_u8(d: Str, p: Int) -> Int`

unsigned byte at `p` (0..255).

### `fn rd_u16(d: Str, p: Int) -> Int`

big-endian u16 at `p`.

### `fn rd_i16(d: Str, p: Int) -> Int`

big-endian i16 at `p` (two's complement).

### `fn rd_u32(d: Str, p: Int) -> Int`

big-endian u32 at `p`.

### `fn rd_tag(d: Str, p: Int) -> Str`

a 4-byte table tag as a Str.

### `fn rd_f2dot14(d: Str, p: Int) -> Float`

F2Dot14 (signed 2.14 fixed point) at `p` -> Float (composite-glyph transforms).

### `struct Font`

a loaded font face: the raw bytes + parsed header values + table offsets (-1 = absent).
cmap is pre-resolved to the best Unicode subtable (`cmap_sub` offset + `cmap_fmt` 4 or 12).
Offsets are into `d`, the whole file: for a face of a `.ttc` collection too, whose tables are
shared between faces and addressed from the start of the file.

### `struct FontResult`

the load outcome: `ok=false` + `err` on a malformed or unsupported font.

### `fn font_empty() -> Font`

an empty placeholder Font (no glyphs): for holding a Font-typed field before a real load.

### `fn woff_to_sfnt(d: Str) -> Str`

reconstruct a plain SFNT (ttf/otf) byte string from a WOFF 1.0 container, inflating any zlib-
compressed tables. Returns "" if the input isn't a WOFF or is malformed.

### `fn woff2_to_sfnt(d: Str) -> Str`

reconstruct a plain SFNT from a WOFF2 container (Brotli decompress + glyf/loca transform reversal).
Returns "" on malformed input or unsupported features (CFF, TTC, hmtx transform).

### `fn sfnt_name_extent(d: Str) -> Int`

how many leading bytes of an sfnt file cover the whole `name` table (for partial reads);
0 if the header is malformed / not sfnt.

### `fn sfnt_name(d: Str, nameId: Int) -> Str`

read one string from an SFNT `name` table: nameId 1 is the family, 2 the subfamily
("Regular", "Bold Italic"), 4 the full name, 6 the PostScript name. Reads the table directly
from the bytes, with no full parse, so it works on a face you have not loaded.

A Windows (3,1) record wins, then any Windows record, then Mac Roman (1,0). UTF-16BE is read
by taking the low byte of each BMP unit, which is exact for the ASCII that family names are in
practice. "" if the id is absent or the data is not an SFNT.

### `fn font_load(d: Str) -> FontResult`

load a face from its bytes: TrueType/OpenType (glyf or CFF outlines), the first face of a
`.ttc` collection, or WOFF/WOFF2, which are decompressed to SFNT first. The result carries `ok`
and, when it is false, an `err` naming what was wrong: a font is data from elsewhere, so
failing to parse is an ordinary outcome rather than a trap.

### `fn font_face_count(d: Str) -> Int`

how many faces the file holds: the count of a `.ttc` collection, 1 for a single font, 0 for
something that is not a font.

### `fn font_load_index(d: Str, index: Int) -> FontResult`

load face `index` of a font file: of a `.ttc`/`.otc` collection, or 0 for a single font.

### `fn font_units_per_em(f: Font) -> Float`

units per em (the font's coordinate grid; scale = px / units_per_em).

### `fn font_num_glyphs(f: Font) -> Int`

number of glyphs in the font.

### `struct VMetrics`

vertical metrics in font units (scale by px/units_per_em for pixels).

### `fn font_v_metrics(f: Font) -> VMetrics`

the font's vertical metrics (font units).

### `fn font_glyph_index(f: Font, cp: Int) -> Int`

map a Unicode codepoint to a glyph id (0 = .notdef / not found). cmap format 4 (BMP) + 12 (full).

### `fn font_variation_glyph(f: Font, cp: Int, vs: Int) -> Int`

the glyph for code point `cp` under variation selector `vs` (cmap format 14), or -1 when the
font has no such variation sequence (a default sequence answers with the ordinary glyph).

### `fn font_h_advance(f: Font, gid: Int) -> Float`

horizontal advance width of glyph `gid` in font units. hmtx: numberOfHMetrics longHorMetric
records (advanceWidth u16, lsb i16), then leftSideBearing[] for the remaining glyphs (all sharing
the last advance width).

### `fn font_lsb(f: Font, gid: Int) -> Float`

left side bearing of glyph `gid` in font units.

### `struct FSeg`

one path segment inside a contour: kind 0 = line to (x,y); kind 1 = quadratic to (x,y) with
control (cx,cy); kind 2 = cubic to (x,y) with controls (cx,cy) and (c2x,c2y); CFF outlines.
Coordinates are in font units.

### `struct Contour`

a closed contour: a start point (sx,sy) followed by segments back around to it.

### `struct Outline`

a glyph outline: contours + the glyph bounding box (font units).

### `fn font_outline(f: Font, gid: Int) -> Outline`

extract glyph `gid`'s outline (contours of line/quad segments, font units). Empty glyphs
(space, or out-of-range) return an outline with no contours.

### `fn outline_num_contours(o: Outline) -> Int`

number of contours in an outline.

### `struct GlyphBitmap`

a rasterized glyph: an `w`x`h` 8-bit coverage bitmap (`cov`, row-major, 0..255) plus placement
offsets: `left` = pixels right of the pen origin, `top` = pixels above the baseline. Place at
screen (pen_x + left, baseline_y - top).

### `struct Blues`

the y values (font units) a face aligns its horizontal edges to. `xr`/`cr`/`rb` are the round
zones: `o` and `O` are drawn slightly taller than `x` and `H`, and dip slightly below the
baseline, so the curve does not read as short or as floating. That overshoot is worth keeping at
display sizes and worth suppressing when it is a fraction of a pixel.

### `fn font_blues(f: Font) -> Blues`

measure a face's Latin blue zones. Cached per font, this outlines ~25 glyphs.

### `fn font_identity(f: Font) -> Int`

a number that identifies a face for caching: two loads (or copies) of the same face agree on it,
and two different faces agree only by an improbable accident of their table layout.

### `fn set_hinting(on: Bool)`

turn light autohinting off (or back on) for subsequent rasterization. On by default; off gives
the raw outline, which is what a transformed or very large render wants.

### `fn hinting() -> Bool`

is light autohinting on?

### `fn rasterize(f: Font, gid: Int, px_size: Float) -> GlyphBitmap`

rasterize glyph `gid` at `px_size` pixels/em into a grayscale coverage bitmap. Empty glyphs
(space, out-of-range) return a 0x0 bitmap.

### `struct GlyphSpan`

A cached glyph addressed rather than copied: `off` is where its coverage starts in the cache's own
store, read back through `glyph_span_cov`. Every field is a scalar, so a span costs no allocation.

The offset stays valid because the cache only ever appends: a later miss can move the underlying
buffer, but never the position of what is already in it, and every read goes through the global.

### `fn rasterize_span(f: Font, gid: Int, px_size: Float) -> GlyphSpan`

`rasterize`, memoized per (face, glyph, size), handed back as a span into the cache: no copy.
Text repaint rasterizes every visible glyph from outlines on every frame, which is what dominates
scroll cost; this collapses that to one rasterization per distinct glyph, and unlike
`rasterize_cached` it does not rebuild the coverage buffer for the caller each time.

### `fn glyph_span_cov(s: GlyphSpan, i: Int) -> Int`

coverage byte `i` (row-major, i = y * w + x) of a cached glyph.

### `fn rasterize_cached(f: Font, gid: Int, px_size: Float) -> GlyphBitmap`

`rasterize`, memoized, as an owned `GlyphBitmap`: the copying form, for callers that want a
bitmap they can keep. The paint path uses `rasterize_span` instead, which does not rebuild the
coverage buffer on every call.

### `fn glyph_cov_at(g: GlyphBitmap, x: Int, y: Int) -> Int`

coverage at pixel (x,y) in a glyph bitmap (0 if out of range).

### `fn raster_new(w: Int, h: Int) -> Rast`

a fresh w*h coverage accumulation grid.

### `fn raster_line(inout r: Rast, x0: Float, y0: Float, x1: Float, y1: Float)`

add a straight edge p0->p1 (pixel coords).

### `fn raster_quad(inout r: Rast, x0: Float, y0: Float, cx: Float, cy: Float, x1: Float, y1: Float)`

add a quadratic bezier p0->p2 with control p1.

### `fn raster_cubic(inout r: Rast, x0: Float, y0: Float, c1x: Float, c1y: Float, c2x: Float, c2y: Float, x1: Float, y1: Float)`

add a cubic bezier p0->p3 with controls p1,p2 (flattened by adaptive subdivision).

### `fn raster_cover(r: Rast) -> Vec<Int>`

resolve the grid to per-pixel coverage (row-major w*h, 0..255).

### `fn font_scale(f: Font, px_size: Float) -> Float`

pixels-per-font-unit for a target em size (multiply font-unit values by this to get pixels).

### `fn font_v_metrics_px(f: Font, px_size: Float) -> VMetrics`

vertical metrics scaled to pixels for `px_size`.

### `fn font_line_height_px(f: Font, px_size: Float) -> Float`

recommended line advance in pixels (ascent - descent + line_gap).

### `fn font_asc_desc_px(f: Font, px_size: Float) -> Float`

the height that `line-height: normal` resolves to in a browser: the hhea ascender minus the
descender, without the line gap: Chromium lays a 16px Liberation Sans line 18px tall because it
uses (1854 + 434)/2048 em, and a +line_gap line (18.4) or a flat 1.2x em line (19.2) both differ.

### `fn font_kern(f: Font, left: Int, right: Int) -> Float`

kerning adjustment between two glyphs in font units (0 when absent).

### `fn utf8_codepoints(s: Str) -> Vec<Int>`

decode a UTF-8 `Str` to Unicode codepoints (malformed trailing bytes are skipped).

### `struct PositionedGlyph`

a glyph positioned along a text run: `gid`, the pen position `x` (baseline-relative, pixels,
left to right), the pen `advance` to the next glyph, and the offset to draw the glyph at relative
to the pen (`x_off`, `y_off`, y up: non-zero for marks and kerned or attached glyphs). Place the
glyph bitmap at (x + x_off + bitmap.left, baseline_y - y_off - bitmap.top). `cluster` is the UTF-8
byte offset of the first character the glyph came from and `char_index` that character's index.

### `fn layout_run(f: Font, text: Str, px_size: Float) -> Vec<PositionedGlyph>`

lay out a single line of text with one font: the text is shaped (OpenType GSUB/GPOS, or the
`kern` table when there is no GPOS kerning), split into bidi and script runs, and the runs placed
in visual order. Returns the positioned glyphs left to right; the total width is
measure_run's. A thin wrapper over shape_line_font.

### `fn measure_run(f: Font, text: Str, px_size: Float) -> Float`

total advance width of a text run in pixels (shaped: GPOS or kern-table kerning, ligatures).

### `struct AtlasEntry`

a cached glyph in the atlas: its pixel rect (`ux,uy,w,h`) into the atlas + placement offsets
(`left`,`top`, as in GlyphBitmap) + the scaled pen `advance`. `color` marks a color glyph (an
emoji): its pixels are in the atlas's `rgba` plane, and `pix` holds its alpha as coverage.

### `struct Atlas`

a CPU glyph atlas: a `w`x`h` coverage buffer packed by shelves, with a cache keyed by
face, glyph and size. Upload `pix` to a GPU texture; `AtlasEntry.ux/uy/w/h` give each glyph's sub-rect.
Color glyphs share the shelves: their premultiplied RGBA (packed 0xRRGGBBAA) goes to the `rgba`
plane (made, w*h, on the first color glyph, `has_color`) and their alpha to `pix`, so a
coverage-only consumer still draws their silhouette.

### `fn atlas_new(w: Int, h: Int) -> Atlas`

a new empty atlas of the given pixel dimensions (grayscale, zero-filled).

### `fn atlas_glyph(inout a: Atlas, f: Font, gid: Int, px_size: Float) -> AtlasEntry`

get (or rasterize + pack) glyph `gid` at `px_size` in the atlas. On a cache hit returns the
stored entry; on a miss rasterizes, shelf-packs, and blits. If the atlas is full the glyph is
still returned (with ux=uy=-1) so text can lay out without it: grow the atlas or evict.

### `fn atlas_glyph_color(inout a: Atlas, f: Font, gid: Int, px_size: Float, fg: Int) -> AtlasEntry`

get (or draw + pack) color glyph `gid` at `px_size`, with `fg` (packed straight RGBA) for COLR's
foreground layers. A glyph with no color form is packed as coverage, like atlas_glyph.

### `fn atlas_full(a: Atlas) -> Bool`

true if the atlas ran out of room for the last requested glyph (grow or rebuild).

### `fn atlas_count(a: Atlas) -> Int`

number of glyphs cached in the atlas.


### `fn grapheme_breaks(cps: Vec<Int>) -> Vec<Int>`

a cluster boundary before every code point of `cps` (and after the last): `n + 1` entries, 1 where
a cluster starts (entry 0 and entry `n` are always 1 for a non-empty run), 0 inside a cluster.

### `fn grapheme_offsets(text: Str) -> Vec<Int>`

the byte offsets in `text` where grapheme clusters start, followed by `text.len()`: a caret may
stand at exactly these offsets. Malformed UTF-8 is one cluster per bad byte.


### `fn LBK_NONE() -> Int`

no break before this position

### `fn LBK_ALLOWED() -> Int`

a break opportunity before this position

### `fn LBK_MANDATORY() -> Int`

a mandatory break before this position (after a hard line break, and at the end of text)

### `fn line_breaks(cps: Vec<Int>) -> Vec<Int>`

the break opportunities of `cps`: n + 1 entries, entry i for the position before character i
(entry 0 is the start of text, never a break; entry n the end, always mandatory).

### `fn line_break_offsets(text: Str) -> Vec<Int>`

the break opportunities of UTF-8 `text` as byte offsets: every offset where a line may wrap
(allowed or mandatory), excluding 0, including text.len().




### `fn ot_tag(s: Str) -> Int`

a 4-character OpenType tag ("kern", "latn", "dev2", "DFLT", "ENG ") as an Int.

### `fn ot_tag_str(t: Int) -> Str`

an Int tag back to its four characters.

### `fn ot_coverage(d: Str, off: Int, gid: Int) -> Int`

the coverage index of `gid` in the Coverage table at `off`, or -1 when not covered.

### `fn ot_class(d: Str, off: Int, gid: Int) -> Int`

the class of `gid` in the ClassDef table at `off` (0 when absent or unlisted).

### `fn font_has_glyph_classes(f: Font) -> Bool`

does the font classify its glyphs (GDEF GlyphClassDef)?

### `fn font_glyph_class(f: Font, gid: Int) -> Int`

GDEF glyph class of `gid`: 1 base, 2 ligature, 3 mark, 4 component, 0 unclassified.








### `struct ShapedGlyph`

one shaped glyph: its glyph id, the cluster (UTF-8 byte offset of the first character it
came from), and its advance and offset in font units (scale by px / units_per_em).

### `fn DIR_AUTO() -> Int`

direction: pick from the script (right to left for Arabic, Hebrew, ...)

### `fn DIR_LTR() -> Int`

direction: left to right

### `fn DIR_RTL() -> Int`

direction: right to left

### `fn script_is_rtl(script: Int) -> Bool`

does `script` (ISO 15924) run right to left?

### `fn text_script(text: Str) -> Int`

the script of a text: the first character's script that is neither Common nor Inherited
(Zyyy, "Common", when there is none).

### `fn shape(f: Font, text: Str, script: Str, dir: Int, lang: Str, features: Str) -> Vec<ShapedGlyph>`

shape `text` with face `f`: `script` is an ISO 15924 code ("Latn", "Arab", "Deva"; "" picks the
text's own), `dir` DIR_LTR/DIR_RTL/DIR_AUTO, `lang` a BCP 47 tag ("" for none), `features` in
HarfBuzz's syntax ("-kern,+smcp,aalt=2"; "" for the defaults). The glyphs come out in visual
order, clusters are UTF-8 byte offsets into `text`, positions are in font units.




### `struct FontSet`

an ordered list of fonts: the first that covers a cluster draws it.

### `fn fontset_new() -> FontSet`

an empty font set.

### `fn fontset_add(inout fs: FontSet, sink f: Font)`

append a font (it is tried after the ones already there).

### `fn fontset_len(fs: FontSet) -> Int`

how many fonts the set holds.

### `fn fontset_font(fs: FontSet, i: Int) -> view Font`

font `i` of the set.

### `struct TextRun`

a run of a line: bytes [start, end) of the text, its bidi level (odd = right to left), its script
(an SC_ value) and the index of its font in the set.

### `struct LineGlyph`

a glyph of a shaped line: `font` indexes the FontSet; `cluster` is the UTF-8 byte offset of the
first character it came from and `char_index` that character's index; `x` is the pen position
(px from the line start, left to right), (`x_off`, `y_off`) the offset to draw at (y up), and
`advance` how far the pen moves. `level` is the bidi level of its run.

### `struct ShapedLine`

a shaped line: glyphs in visual (left-to-right) order, and its total advance in px.

### `fn itemize(fs: FontSet, text: Str, dir: Int) -> Vec<TextRun>`

split `text` into runs of one bidi level, script and font, in logical order (bidi_visual_runs
gives their display order). `dir` is BIDI_AUTO/BIDI_LTR/BIDI_RTL.

### `fn itemize_pair(primary: Font, fallback: Font, has_fallback: Bool, text: Str, dir: Int) -> Vec<TextRun>`

`itemize` over a primary font and, when `has_fallback`, a fallback.

### `fn itemize_pool(pool: Vec<Font>, idx: Vec<Int>, text: Str, dir: Int) -> Vec<TextRun>`

`itemize` over fonts `idx` (tried in that order) of a pool the caller holds; a run's `font` is a
position in `idx`.

### `fn itemize_one(text: Str, dir: Int) -> Vec<TextRun>`

`itemize` for a single font: runs of one bidi level and script.

### `fn bidi_visual_runs(runs: Vec<TextRun>) -> Vec<Int>`

the display order of runs (indices into `runs`), from their bidi levels (L2).

### `fn shape_cached(f: Font, text: Str, script: Str, dir: Int, lang: Str, features: Str) -> Vec<ShapedGlyph>`

`shape`, memoized: the same text in the same face, script, direction, language and features is
shaped once.

### `fn shape_cache_stats() -> Vec<Int>`

the shaped-run cache's hit and miss counts so far (for measuring).

### `fn shape_cache_clear()`

drop every cached shaped run.

### `fn shape_line(fs: FontSet, text: Str, px: Float, dir: Int, lang: Str, features: Str) -> ShapedLine`

shape one line of `text` with fallback through `fs` at `px` pixels per em: itemize, shape each
run (cached), and lay the runs out in visual order. `dir` is the paragraph direction
(BIDI_AUTO/BIDI_LTR/BIDI_RTL); `lang` and `features` go to every run.

### `fn shape_line_pool(pool: Vec<Font>, idx: Vec<Int>, text: Str, px: Float, dir: Int, lang: Str, features: Str) -> ShapedLine`

`shape_line` over fonts `idx` of a pool the caller holds (tried in that order, nothing copied):
a glyph's `font` is its position in `idx`. With `idx` empty the whole pool is tried in order.

### `fn shape_line_pair(primary: Font, fallback: Font, has_fallback: Bool, text: Str, px: Float, dir: Int, lang: Str, features: Str) -> ShapedLine`

`shape_line` over a primary font and, when `has_fallback`, one fallback font (both held by the
caller): a glyph's `font` is 0 for the primary, 1 for the fallback.

### `fn shape_line_font(f: Font, text: Str, px: Float, dir: Int, lang: Str, features: Str) -> ShapedLine`

`shape_line` with one font (no fallback): bidi and script runs, shaped and laid out.


### `fn ucd_blob_size() -> Int`

the size in bytes of the encoded property blob (a test checks it against the header)

### `fn script_code(s: Int) -> Str`

the ISO 15924 code of script value `s` ("Zzzz" when out of range)

### `fn script_from_code(code: Str) -> Int`

the script value of an ISO 15924 code (case as written, e.g. "Arab"); SC_Zzzz when unknown


### `fn ucd_gc(cp: Int) -> Int`

General_Category of `cp` (one of the GC_* constants).

### `fn ucd_bidi_class(cp: Int) -> Int`

Bidi_Class of `cp` (BC_*), with the UCD's defaults for unassigned code points (R/AL blocks).

### `fn ucd_script(cp: Int) -> Int`

Script of `cp` (SC_*; script_code gives its ISO 15924 code).

### `fn ucd_line_break(cp: Int) -> Int`

Line_Break class of `cp` (LB_*), with the UCD's defaults for unassigned code points.

### `fn ucd_ccc(cp: Int) -> Int`

Canonical_Combining_Class of `cp` (0..240).

### `fn ucd_joining_type(cp: Int) -> Int`

Joining_Type of `cp` (JT_*: U R D C L T).

### `fn ucd_east_asian_width(cp: Int) -> Int`

East_Asian_Width of `cp` (EA_*).

### `fn ucd_is_extended_pictographic(cp: Int) -> Bool`

Extended_Pictographic (emoji-data.txt).

### `fn ucd_is_emoji_presentation(cp: Int) -> Bool`

Emoji_Presentation: shown as emoji by default.

### `fn ucd_is_emoji_modifier(cp: Int) -> Bool`

Emoji_Modifier: the five skin tones U+1F3FB..1F3FF.

### `fn ucd_is_emoji_modifier_base(cp: Int) -> Bool`

Emoji_Modifier_Base: takes a skin tone.

### `fn ucd_is_default_ignorable(cp: Int) -> Bool`

Default_Ignorable_Code_Point (DerivedCoreProperties.txt).

### `fn ucd_is_emoji(cp: Int) -> Bool`

Emoji (emoji-data.txt): has an emoji form at all.

### `fn ucd_is_grapheme_extend(cp: Int) -> Bool`

Grapheme_Extend.

### `fn ucd_is_bidi_mirrored(cp: Int) -> Bool`

Bidi_Mirrored.

### `fn gc_is_mark(gc: Int) -> Bool`

is `gc` one of the mark categories (Mn, Mc, Me)?

### `fn ucd_mirror(cp: Int) -> Int`

Bidi_Mirroring_Glyph of `cp`, or `cp` itself when it has none.

### `fn ucd_bracket_type(cp: Int) -> Int`

Bidi_Paired_Bracket_Type of `cp`: 0 none, 1 open, 2 close.

### `fn ucd_bracket_pair(cp: Int) -> Int`

Bidi_Paired_Bracket of `cp` (the other bracket of the pair), or `cp` when it is not a bracket.

### `fn ucd_decompose(ab: Int, inout out: Vec<Int>) -> Bool`

the canonical decomposition of `ab` as at most two code points (the UCD's own one-level form;
apply it again to `a` for the full decomposition). `out[0]`/`out[1]` get a and b (b = 0 for a
singleton). Returns false when `ab` does not decompose.

### `fn ucd_compose(a: Int, b: Int) -> Int`

the primary composite of `a` + `b`, or 0 when they do not compose canonically.

### `fn ucd_gcb(cp: Int) -> Int`

Grapheme_Cluster_Break of `cp` (one of the GCB_* constants).

### `fn ucd_incb(cp: Int) -> Int`

Indic_Conjunct_Break of `cp` (one of the INCB_* constants).

### `fn ucd_blob_for_test() -> view Str`

the raw property blob, for the tests' integrity check.


