#!/usr/bin/env python3
"""gen_testdata.py: regenerate std/font's vendored test fonts and its HarfBuzz oracle data.

    python3 std/font/oracle/gen_testdata.py          (from the repository root)

Builds the C oracles in std/font/oracle/ with gcc against the system HarfBuzz, then:
  1. cuts a small SUBSET of each system font below (std/font/assets/*.ttf), keeping every glyph the
     test strings for that font need plus their GSUB closure;
  2. shapes every test case with HarfBuzz against the SUBSET and writes the glyph ids, clusters,
     advances and offsets to std/font/testdata/shape_expected.txt.
The Nori tests read only the committed outputs; nothing here runs at test time.
"""
import os, subprocess, sys, tempfile, struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
FONTDIR = os.path.join(ROOT, "std/font")
ASSETS = os.path.join(FONTDIR, "assets")
TESTDATA = os.path.join(FONTDIR, "testdata")
ORACLE = os.path.join(FONTDIR, "oracle")
SYS = "/usr/share/fonts"
TMP = os.environ.get("NORI_SCRATCH") or tempfile.mkdtemp(prefix="font_oracle_")

def sh(*a):
    subprocess.run(a, check=True)

def build(name, pkgs):
    out = os.path.join(TMP, name)
    flags = subprocess.run(["pkg-config", "--cflags", "--libs"] + pkgs, check=True, capture_output=True, text=True).stdout.split()
    sh("gcc", "-O2", "-Wall", "-o", out, os.path.join(ORACLE, name + ".c"), *flags)
    return out

# ---------------------------------------------------------------------------------------------
# test strings, per font. (case name, script, direction, language, features, text)
# ---------------------------------------------------------------------------------------------
LATIN = [
    ("latin_hello", "Latn", "ltr", "en", "-", "Hello, World!"),
    ("latin_kern", "Latn", "ltr", "en", "-", "AVATAR To Ty Wa LT Yo P. F, \"A\" 'V'"),
    ("latin_nokern", "Latn", "ltr", "en", "-kern", "AVATAR To Ty Wa"),
    ("latin_liga", "Latn", "ltr", "en", "-", "office affluent fjord flag ffi ffl"),
    ("latin_noliga", "Latn", "ltr", "en", "-liga", "office affluent"),
    ("latin_precomposed", "Latn", "ltr", "en", "-", "naïve café résumé Ångström"),
    ("latin_marks", "Latn", "ltr", "en", "-", "q́ x̣̂ ñ̄ ģ ı̈"),
    ("latin_decompose", "Latn", "ltr", "en", "-", "é ạ̀ ö"),
    ("latin_vietnamese", "Latn", "ltr", "vi", "-", "Tiếng Việt ở đây ẩn ặ"),
    ("latin_numbers", "Latn", "ltr", "en", "-", "0123456789 1/2 3.14 -42%"),
    ("latin_onum", "Latn", "ltr", "en", "+onum,+tnum", "0123456789"),
    ("latin_smcp", "Latn", "ltr", "en", "+smcp", "Small Caps"),
    ("latin_frac", "Latn", "ltr", "en", "-", "1⁄2 3⁄4"),
    ("latin_turkish", "Latn", "ltr", "tr", "-", "fi İstanbul ıi"),
    ("cyrillic", "Cyrl", "ltr", "ru", "-", "Привет, мир! Ёлка йод"),
    ("cyrillic_serbian", "Cyrl", "ltr", "sr", "-", "бгдпт"),
    ("greek", "Grek", "ltr", "el", "-", "Γειά σου κόσμε ΆΈΉ ᾶ ῷ"),
    ("latin_zwj", "Latn", "ltr", "en", "-", "f‌i f‍i a­b"),
    ("latin_space", "Latn", "ltr", "en", "-", "a b c d"),
    ("latin_aalt1", "Latn", "ltr", "en", "aalt=1", "agy1"),
    ("latin_aalt2", "Latn", "ltr", "en", "aalt=2", "agy1"),
    ("latin_salt", "Latn", "ltr", "en", "+salt", "agy"),
    ("latin_case", "Latn", "ltr", "en", "+case,+c2sc", "(Hi) [AB] -X-"),
    ("latin_sups", "Latn", "ltr", "en", "+sups", "x2 H1"),
    ("latin_mixed_marks", "Latn", "ltr", "en", "-", "\u01D8 \u1E17 \u1EA1\u0300 \u1E97 s\u0323\u0307"),
    ("latin_odd_chars", "Latn", "ltr", "en", "-", "\uFFFD\u00AD-\u2011x"),
]
ARABIC = [
    ("arabic_salam", "Arab", "rtl", "ar", "-", "السلام عليكم"),
    ("arabic_lamalef", "Arab", "rtl", "ar", "-", "لا الا لإ لأ لآ"),
    ("arabic_basmala", "Arab", "rtl", "ar", "-", "بِسْمِ ٱللَّهِ ٱلرَّحْمَٰنِ ٱلرَّحِيمِ"),
    ("arabic_joining", "Arab", "rtl", "ar", "-", "ب بب ببب ء بءب دب بد"),
    ("arabic_persian", "Arab", "rtl", "fa", "-", "می‌خواهم سلام دنیا ۱۲۳"),
    ("arabic_urdu", "Arab", "rtl", "ur", "-", "اردو زبان ہے"),
    ("arabic_digits", "Arab", "rtl", "ar", "-", "١٢٣ ٤٥٦"),
    ("arabic_tatweel", "Arab", "rtl", "ar", "-", "بـــب"),
    ("arabic_marks_order", "Arab", "rtl", "ar", "-", "بّ بَّ بِّ بُّ"),
    ("arabic_norlig", "Arab", "rtl", "ar", "-rlig", "لا"),
    ("arabic_zwj", "Arab", "rtl", "ar", "-", "ب‍ ‍ب ب‌ب"),
    ("arabic_lamalef_marks", "Arab", "rtl", "ar", "-", "\u0644\u064E\u0627 \u0644\u0627\u064B \u0644\u0650\u0625"),
    ("arabic_mixed_digits", "Arab", "rtl", "ar", "-", "\u0639\u0627\u0645 2024 (\u0661\u0662)"),
    ("arabic_ltr_digits", "Arab", "ltr", "ar", "-", "123"),
]
NASTALIQ = [
    ("nastaliq_urdu", "Arab", "rtl", "ur", "-", "\u0627\u0631\u062F\u0648 \u0632\u0628\u0627\u0646 \u06C1\u06D2"),
    ("nastaliq_pakistan", "Arab", "rtl", "ur", "-", "\u067E\u0627\u06A9\u0633\u062A\u0627\u0646"),
    ("nastaliq_word", "Arab", "rtl", "ur", "-", "\u0646\u0633\u062A\u0639\u0644\u06CC\u0642 \u062E\u0637"),
    ("nastaliq_marks", "Arab", "rtl", "ur", "-", "\u0628\u0650\u0633\u0652\u0645\u0650"),
]
ADWAITA_MONO = [
    ("mono_auto_frac", "Latn", "ltr", "en", "-", "1\u20442 12\u2044345"),
    ("mono_frac", "Latn", "ltr", "en", "+frac", "1/2 3/45 x/y"),
    ("mono_code", "Latn", "ltr", "en", "-", "a != b -> c <= d === e"),
]
HEBREW = [
    ("hebrew_shalom", "Hebr", "rtl", "he", "-", "שָׁלוֹם עוֹלָם"),
    ("hebrew_dagesh", "Hebr", "rtl", "he", "-", "בּ כּ פּ שּׁ תּ"),
    ("hebrew_plain", "Hebr", "rtl", "he", "-", "עברית"),
]
DEVANAGARI = [
    ("deva_namaste", "Deva", "ltr", "hi", "-", "नमस्ते दुनिया"),
    ("deva_conjunct", "Deva", "ltr", "hi", "-", "क्षत्रिय ज्ञान श्र द्व"),
    ("deva_reph", "Deva", "ltr", "hi", "-", "र्क कर्म धर्म"),
    ("deva_matra", "Deva", "ltr", "hi", "-", "कि कु कृ के कै को कौ"),
    ("deva_hindi", "Deva", "ltr", "hi", "-", "हिन्दी"),
    ("deva_nukta", "Deva", "ltr", "hi", "-", "क़ ज़ ड़"),
    ("deva_half", "Deva", "ltr", "hi", "-", "स्त प्य त्त"),
    ("deva_marks", "Deva", "ltr", "hi", "-", "हँ हं हः ॐ"),
    ("deva_zwj_half", "Deva", "ltr", "hi", "-", "\u0915\u094D\u200D\u0937 \u0915\u094D\u200C\u0937 \u0930\u094D\u200D\u092F"),
    ("deva_vowel_constraint", "Deva", "ltr", "hi", "-", "\u0905\u093E \u090F\u0947"),
    ("deva_broken", "Deva", "ltr", "hi", "-", "\u093F \u094D\u0915 \u0901"),
    ("deva_reph_matra", "Deva", "ltr", "hi", "-", "\u0930\u094D\u0915\u093F \u0930\u094D\u0915\u0940 \u0915\u0930\u094D\u0924\u094D\u0924\u0935\u094D\u092F"),
    ("deva_sanskrit", "Deva", "ltr", "sa", "-", "\u0936\u094D\u0930\u0940\u092E\u0926\u094D\u092D\u0917\u0935\u0926\u094D\u0917\u0940\u0924\u093E"),
    ("deva_stacks", "Deva", "ltr", "hi", "-", "\u0926\u094D\u0927 \u0939\u094D\u092E \u0939\u094D\u092F \u091F\u094D\u091F \u0919\u094D\u0915"),
    ("deva_vattu", "Deva", "ltr", "hi", "-", "\u092A\u094D\u0930\u0947\u092E \u0924\u094D\u0930 \u0915\u094D\u0930 \u090B\u0937\u093F"),
    ("deva_misc", "Deva", "ltr", "hi", "-", "\u091A\u093E\u0901\u0926 \u0950 \u0928\u092E\u0903 \u0967\u0968\u0969 \u0915\u093C\u094D"),
    ("deva_nbsp_matra", "Deva", "ltr", "hi", "-", "\u00A0\u093F \u25CC\u0947 \u0915\u093F\u0902"),
]
BENGALI = [
    ("beng_text", "Beng", "ltr", "bn", "-", "\u09AC\u09BE\u0982\u09B2\u09BE \u09AD\u09BE\u09B7\u09BE"),
    ("beng_split_matra", "Beng", "ltr", "bn", "-", "\u0995\u09CB \u0995\u09CC \u0995\u09C7\u09BE"),
    ("beng_reph", "Beng", "ltr", "bn", "-", "\u09B0\u09CD\u0995 \u0995\u09B0\u09CD\u09AE \u09B8\u09B0\u09CD\u09AC\u09CB"),
    ("beng_yaphala", "Beng", "ltr", "bn", "-", "\u09B0\u09CD\u09AF \u0995\u09CD\u09AF \u0995\u09CD\u09B0\u09BF"),
    ("beng_khanda", "Beng", "ltr", "bn", "-", "\u09CE \u0995\u09CD\u09B7 \u099C\u09CD\u099E"),
]
THAI = [
    ("thai_text", "Thai", "ltr", "th", "-", "\u0E2A\u0E27\u0E31\u0E2A\u0E14\u0E35\u0E04\u0E23\u0E31\u0E1A \u0E20\u0E32\u0E29\u0E32\u0E44\u0E17\u0E22"),
    ("thai_sara_am", "Thai", "ltr", "th", "-", "\u0E19\u0E49\u0E33 \u0E14\u0E4B\u0E33 \u0E01\u0E33"),
    ("thai_marks", "Thai", "ltr", "th", "-", "\u0E1B\u0E39\u0E48 \u0E1D\u0E31\u0E48\u0E07 \u0E0D\u0E39"),
]
CJK = [
    ("cjk_ja", "Hani", "ltr", "ja", "-", "日本語のテキスト、です。"),
    ("cjk_kana", "Kana", "ltr", "ja", "-", "カタカナ"),
    ("cjk_zh", "Hani", "ltr", "zh", "-", "中文字体"),
    ("cjk_ko", "Hang", "ltr", "ko", "-", "한국어"),
    ("cjk_mixed", "Hani", "ltr", "ja", "-", "漢字（かんじ）"),
    ("cjk_palt", "Hani", "ltr", "ja", "+palt", "\uFF08\u304B\u3093\u3058\uFF09\u3001\u3067\u3059\u3002"),
    ("cjk_halt", "Hani", "ltr", "ja", "+halt", "\uFF08\u6F22\u5B57\uFF09"),
    ("cjk_fwid", "Latn", "ltr", "ja", "+fwid", "AB12"),
]
EMOJI = [
    ("emoji_simple", "Zyyy", "ltr", "-", "-", "😀👍"),
    ("emoji_skin", "Zyyy", "ltr", "-", "-", "👍🏽"),
    ("emoji_zwj", "Zyyy", "ltr", "-", "-", "👨‍👩‍👧"),
    ("emoji_flag", "Zyyy", "ltr", "-", "-", "🇭🇺"),
    ("emoji_vs", "Zyyy", "ltr", "-", "-", "❤️"),
    ("emoji_keycap", "Zyyy", "ltr", "-", "-", "1\uFE0F\u20E3"),
    ("emoji_rainbow", "Zyyy", "ltr", "-", "-", "\U0001F3F3\uFE0F\u200D\U0001F308"),
]

FONTS = [
    # asset name, system font, face index, cases, extra text, keep hints
    ("NotoSans-subset.ttf", "noto/NotoSans-Regular.ttf", 0, LATIN, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 .,;:!?'\"()-/", False),
    ("NotoSansArabic-subset.ttf", "noto/NotoSansArabic-Regular.ttf", 0, ARABIC, " ", False),
    ("NotoNaskhArabic-subset.ttf", "noto/NotoNaskhArabic-Regular.ttf", 0, ARABIC, " ", False),
    ("NotoNastaliqUrdu-subset.ttf", "noto/NotoNastaliqUrdu-Regular.ttf", 0, NASTALIQ, " ", False),
    ("AdwaitaMono-subset.ttf", "Adwaita/AdwaitaMono-Regular.ttf", 0, ADWAITA_MONO, "0123456789/ ", False),
    ("NotoSansHebrew-subset.ttf", "noto/NotoSansHebrew-Regular.ttf", 0, HEBREW, " ", False),
    ("NotoSansDevanagari-subset.ttf", "noto/NotoSansDevanagari-Regular.ttf", 0, DEVANAGARI, " ", False),
    ("NotoSansThai-subset.ttf", "noto/NotoSansThai-Regular.ttf", 0, THAI, " ", False),
    ("NotoSansBengali-subset.ttf", "noto/NotoSansBengali-Regular.ttf", 0, BENGALI, " ", False),
    ("NotoSansCJKjp-subset.otf", "noto-cjk/NotoSansCJK-Regular.ttc", 0, CJK, " 一二三永", True),
    ("NotoSansCJKsc-subset.otf", "noto-cjk/NotoSansCJK-Regular.ttc", 2, CJK, " 一二三永", True),
    ("NotoColorEmoji-subset.ttf", "noto/NotoColorEmoji.ttf", 0, EMOJI, "", False),
]

def make_ttc(paths, out):
    """join single-face SFNTs into one .ttc collection (tables copied per face, offsets rewritten)."""
    fonts = [open(p, "rb").read() for p in paths]
    n = len(fonts)
    hdr = bytearray(b"ttcf" + struct.pack(">HHI", 1, 0, n))
    pos = 12 + 4 * n
    face_offs = []
    for f in fonts:
        face_offs.append(pos); pos += 12 + 16 * struct.unpack(">H", f[4:6])[0]
    for o in face_offs: hdr += struct.pack(">I", o)
    dirs = bytearray(); data = bytearray(); data_start = pos
    for f in fonts:
        nt = struct.unpack(">H", f[4:6])[0]
        d = bytearray(f[:12])
        for i in range(nt):
            rec = f[12 + 16 * i: 28 + 16 * i]
            tag, cks, off, ln = struct.unpack(">4sIII", rec)
            while len(data) % 4: data += b"\0"
            d += struct.pack(">4sIII", tag, cks, data_start + len(data), ln)
            data += f[off:off + ln]
        dirs += d
    open(out, "wb").write(bytes(hdr + dirs + data))

BIDI = [
    ("en_ar", "auto", "Hello \u0645\u0631\u062D\u0628\u0627 world"),
    ("ar_en", "auto", "\u0645\u0631\u062D\u0628\u0627 Hello \u0639\u0627\u0644\u0645"),
    ("he_numbers", "auto", "\u05E9\u05DC\u05D5\u05DD 123 \u05E2\u05D5\u05DC\u05DD 4.5%"),
    ("ar_numbers", "rtl", "\u0627\u0644\u0633\u0639\u0631 \u0661\u0662\u0663 \u062F\u0648\u0644\u0627\u0631 (99.5)"),
    ("brackets", "rtl", "a (b) [\u05D0 \u05D1] {c}"),
    ("brackets_ltr", "ltr", "\u05D0 (\u05D1 c) d"),
    ("isolates", "ltr", "x \u2067\u05D0\u05D1 1\u2069 y \u2068\u0627 b\u2069 z"),
    ("embeddings", "ltr", "a\u202B\u05D0 b\u202C c \u202E\u05D1 d\u202C"),
    ("override", "rtl", "\u202Dabc\u202C \u05D0"),
    ("url", "rtl", "\u05E8\u05D0\u05D4 http://example.com/a-b \u05E2\u05DB\u05E9\u05D9\u05D5"),
    ("marks", "auto", "\u05E9\u05B8\u05C1\u05DC\u05D5\u05B9\u05DD abc\u0301"),
    ("mixed_ws", "ltr", "abc \u05D0\u05D1\u05D2  \t def  "),
    ("phone", "rtl", "\u05D8\u05DC\u05E4\u05D5\u05DF: +972-3-555-1234"),
    ("persian", "auto", "\u0627\u06CC\u0646 \u06CC\u06A9 \u0645\u062A\u0646 \u0641\u0627\u0631\u0633\u06CC \u0627\u0633\u062A (Persian) \u06F1\u06F2\u06F3."),
    ("empty_auto", "auto", "123 !?"),
]

def sfnt_tables(data):
    n = struct.unpack(">H", data[4:6])[0]
    t = {}
    for i in range(n):
        tag, ck, off, ln = struct.unpack(">4sIII", data[12 + 16 * i: 28 + 16 * i])
        t[tag] = data[off:off + ln]
    return t

def sfnt_build(tables, flavor=0x00010000):
    tags = sorted(tables)
    n = len(tags)
    out = bytearray(struct.pack(">IHHHH", flavor, n, 0, 0, 0))
    off = 12 + 16 * n
    data = bytearray()
    for t in tags:
        body = tables[t]
        out += struct.pack(">4sIII", t, 0, off + len(data), len(body))
        data += body
        while len(data) % 4: data += b"\0"
    return bytes(out + data)

def cmap_glyph(data, cp):
    t = sfnt_tables(data)[b"cmap"]
    for i in range(struct.unpack(">H", t[2:4])[0]):
        pl, en, off = struct.unpack(">HHI", t[4 + 8 * i: 12 + 8 * i])
        sub = t[off:]
        if struct.unpack(">H", sub[:2])[0] == 4:
            segx2 = struct.unpack(">H", sub[6:8])[0]
            for s_ in range(segx2 // 2):
                e = struct.unpack(">H", sub[14 + 2 * s_: 16 + 2 * s_])[0]
                st = struct.unpack(">H", sub[16 + segx2 + 2 * s_: 18 + segx2 + 2 * s_])[0]
                de = struct.unpack(">h", sub[16 + 2 * segx2 + 2 * s_: 18 + 2 * segx2 + 2 * s_])[0]
                ro = struct.unpack(">H", sub[16 + 3 * segx2 + 2 * s_: 18 + 3 * segx2 + 2 * s_])[0]
                if st <= cp <= e and ro == 0: return (cp + de) & 0xFFFF
    return 0

def make_colr_font(src, out):
    """NotoSans-subset with a COLR v0 / CPAL table added: 'A' becomes A in red under o in translucent
    blue, 'B' a single foreground layer: a small layered color font FreeType can render too."""
    data = open(src, "rb").read()
    t = sfnt_tables(data)
    gA, gB, go = cmap_glyph(data, ord("A")), cmap_glyph(data, ord("B")), cmap_glyph(data, ord("o"))
    bases = sorted([(gA, 0, 2), (gB, 2, 1)])
    layers = [(gA, 0), (go, 1), (gB, 0xFFFF)]
    colr = struct.pack(">HHIIH", 0, len(bases), 14, 14 + 6 * len(bases), len(layers))
    colr += b"".join(struct.pack(">HHH", *b) for b in bases)
    colr += b"".join(struct.pack(">HH", *l) for l in layers)
    cpal = struct.pack(">HHHHIH", 0, 2, 1, 2, 14, 0) + bytes([0x20, 0x20, 0xE0, 0xFF, 0xF0, 0x40, 0x10, 0x99])
    t[b"COLR"] = colr
    t[b"CPAL"] = cpal
    open(out, "wb").write(sfnt_build(t))
    return gA, gB

def hexcps(s):
    return " ".join("%X" % ord(c) for c in s)

def main():
    os.makedirs(TESTDATA, exist_ok=True)
    subset = build("subset", ["harfbuzz-subset", "harfbuzz"])
    shape = build("shape", ["harfbuzz"])
    cases = []
    for asset, src, face, cs, extra, hints in FONTS:
        text = extra + "".join(c[5] for c in cs)
        tf = os.path.join(TMP, asset + ".txt")
        open(tf, "w", encoding="utf-8").write(text)
        out = os.path.join(ASSETS, asset)
        args = [subset, os.path.join(SYS, src), str(face), out, tf]
        if hints: args.append("keep-hints")
        sh(*args)
        rel = os.path.relpath(out, ROOT)
        for (name, script, d, lang, feats, txt) in cs:
            cases.append("\t".join([asset.split("-")[0] + ":" + name, rel, "0", script, d, lang, feats, hexcps(txt)]))
    make_ttc([os.path.join(ASSETS, "NotoSansCJKjp-subset.otf"), os.path.join(ASSETS, "NotoSansCJKsc-subset.otf")],
             os.path.join(ASSETS, "NotoSansCJK-subset.ttc"))
    # FreeType coverage references: a few CFF (CJK) and glyf glyphs at several sizes, unhinted
    ft = build("ftraster", ["freetype2"])
    rcases = []
    for asset, gids in [("NotoSansCJKjp-subset.otf", [5, 20, 40, 60]), ("NotoSans-subset.ttf", [25, 46, 66, 76]),
                        ("NotoSansArabic-subset.ttf", [10, 30])]:
        for g in gids:
            for size in (12, 16, 24, 48):
                rcases.append("\t".join([os.path.relpath(os.path.join(ASSETS, asset), ROOT), "0", str(g), str(size)]))
    rcf = os.path.join(TMP, "raster_cases.txt")
    open(rcf, "w").write("\n".join(rcases) + "\n")
    res = subprocess.run([ft, rcf], check=True, capture_output=True, text=True, cwd=ROOT).stdout
    open(os.path.join(TESTDATA, "raster_expected.txt"), "w").write(res)
    bidi = build("bidi", ["fribidi"])
    bcf = os.path.join(TESTDATA, "bidi_cases.txt")
    open(bcf, "w").write("# generated by gen_testdata.py — name, direction, text\n" + "\n".join("\t".join([n, d, hexcps(t)]) for n, d, t in BIDI) + "\n")
    res = subprocess.run([bidi, bcf], check=True, capture_output=True, text=True).stdout
    open(os.path.join(TESTDATA, "bidi_expected.txt"), "w").write(res)
    # color: the emoji subset's CBDT glyphs at their strike, and a synthetic COLR v0 font
    gA, gB = make_colr_font(os.path.join(ASSETS, "NotoSans-subset.ttf"), os.path.join(ASSETS, "ColrTest.ttf"))
    ftc = build("ftcolor", ["freetype2"])
    ccases = []
    for g in (4, 20):
        ccases.append("\t".join(["std/font/assets/NotoColorEmoji-subset.ttf", "0", str(g), "0"]))
    for g in (gA, gB):
        for size in (16, 40):
            ccases.append("\t".join(["std/font/assets/ColrTest.ttf", "0", str(g), str(size)]))
    ccf = os.path.join(TMP, "color_cases.txt")
    open(ccf, "w").write("\n".join(ccases) + "\n")
    res = subprocess.run([ftc, ccf], check=True, capture_output=True, text=True, cwd=ROOT).stdout
    # the strike bitmaps are big as hex: commit them zstd-compressed (std/archive reads it back)
    zst = subprocess.run(["zstd", "-19", "-q", "-c"], input=res.encode(), check=True, capture_output=True).stdout
    open(os.path.join(TESTDATA, "color_expected.txt.zst"), "wb").write(zst)
    cf = os.path.join(TESTDATA, "shape_cases.txt")
    open(cf, "w").write("# generated by gen_testdata.py — name, font, face, script, dir, lang, features, text\n" + "\n".join(cases) + "\n")
    # shape from the repository root so the font paths in the cases resolve
    res = subprocess.run([shape, cf], check=True, capture_output=True, text=True, cwd=ROOT).stdout
    open(os.path.join(TESTDATA, "shape_expected.txt"), "w").write(res)
    print("wrote", len(cases), "cases")

if __name__ == "__main__":
    main()
