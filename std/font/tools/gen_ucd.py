#!/usr/bin/env python3
"""gen_ucd.py: build std/font's Unicode property tables from the Unicode 16.0.0 data files.

    python3 std/font/tools/gen_ucd.py        (downloads the UCD files into ~/.cache/ucd16 or $UCD_DIR)

Writes
  std/font/data/ucd.bin   one blob, read with include_str (the only escape include_str applies is
                          backslash, so every 0x5C byte is written doubled);
  std/font/ucd_gen.nori   the value constants for every enumerated property, generated from the
                          same lists the blob was encoded with, so the two can never disagree.

The blob holds, for every code point, one record of eight bytes:
  general category, bidi class, script, line break class, canonical combining class, joining type,
  east asian width, flags (Extended_Pictographic, Emoji_Presentation, Emoji_Modifier,
  Emoji_Modifier_Base, Default_Ignorable_Code_Point, Emoji, Grapheme_Extend, Bidi_Mirrored),
found through a three-level trie (cp>>11, then 32 blocks of 64), and four sorted tables:
mirroring pairs, paired brackets, canonical decompositions (as HarfBuzz uses them: at most two
code points, recursively), and canonical compositions (pairs, without Full_Composition_Exclusion);
then the Indic shaper's categories, and the ranges of Grapheme_Cluster_Break and
Indic_Conjunct_Break (UAX #29) as (lo, hi, gcb, incb).
"""
import os, sys, struct
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ucdparse import *

HERE = os.path.dirname(os.path.abspath(__file__))
FONT = os.path.dirname(HERE)

def enum_order(prop, extra_first=()):
    al = aliases(prop)
    names = [s for s, _ in al]
    long2short = {l: s for s, l in al}
    for s, _ in al: long2short[s] = s
    return names, long2short

# HarfBuzz's Indic categories (hb-ot-shaper-indic-machine) and positions (hb-ot-shaper-indic.hh)
I_CAT = {"X": 0, "C": 1, "V": 2, "N": 3, "H": 4, "ZWNJ": 5, "ZWJ": 6, "M": 7, "SM": 8, "A": 9, "VD": 9,
         "PLACEHOLDER": 10, "DOTTEDCIRCLE": 11, "RS": 12, "MPst": 13, "Repha": 14, "Ra": 15, "CM": 16,
         "Symbol": 17, "CS": 18}
I_POS = {"START": 0, "RA_TO_BECOME_REPH": 1, "PRE_M": 2, "PRE_C": 3, "BASE_C": 4, "AFTER_MAIN": 5,
         "ABOVE_C": 6, "BEFORE_SUB": 7, "BELOW_C": 8, "AFTER_SUB": 9, "BEFORE_POST": 10, "POST_C": 11,
         "AFTER_POST": 12, "SMVD": 13, "END": 14}

def indic_table():
    """(cp, category, position) for the code points the Indic shaper classifies, following HarfBuzz's
    gen-indic-table.py mapping and the overrides in its set_indic_properties."""
    cat_map = {"Other": "X", "Avagraha": "Symbol", "Bindu": "SM", "Brahmi_Joining_Number": "PLACEHOLDER",
        "Cantillation_Mark": "A", "Consonant": "C", "Consonant_Dead": "C", "Consonant_Final": "CM",
        "Consonant_Head_Letter": "C", "Consonant_Initial_Postfixed": "C", "Consonant_Killer": "M",
        "Consonant_Medial": "CM", "Consonant_Placeholder": "PLACEHOLDER", "Consonant_Preceding_Repha": "Repha",
        "Consonant_Prefixed": "X", "Consonant_Subjoined": "CM", "Consonant_Succeeding_Repha": "CM",
        "Consonant_With_Stacker": "CS", "Gemination_Mark": "SM", "Invisible_Stacker": "H", "Joiner": "ZWJ",
        "Modifying_Letter": "X", "Non_Joiner": "ZWNJ", "Nukta": "N", "Number": "PLACEHOLDER",
        "Number_Joiner": "PLACEHOLDER", "Pure_Killer": "M", "Register_Shifter": "RS", "Syllable_Modifier": "SM",
        "Tone_Letter": "X", "Tone_Mark": "N", "Virama": "H", "Visarga": "SM", "Vowel": "V",
        "Vowel_Dependent": "M", "Vowel_Independent": "V", "Reordering_Killer": "M"}
    pos_map = {"Not_Applicable": "END", "Left": "PRE_C", "Top": "ABOVE_C", "Bottom": "BELOW_C", "Right": "POST_C",
        "Bottom_And_Right": "POST_C", "Left_And_Right": "POST_C", "Top_And_Bottom": "BELOW_C",
        "Top_And_Bottom_And_Left": "BELOW_C", "Top_And_Bottom_And_Right": "POST_C", "Top_And_Left": "ABOVE_C",
        "Top_And_Left_And_Right": "POST_C", "Top_And_Right": "POST_C", "Overstruck": "AFTER_MAIN",
        "Visual_Order_Left": "PRE_M"}
    def want(cp):
        return cp < 0x2100 or 0xA8E0 <= cp <= 0xA8FF or 0x1CD0 <= cp <= 0x1CFF or 0x11300 <= cp <= 0x1137F
    cat = {}
    for lo, hi, f, miss in ranges("IndicSyllabicCategory.txt"):
        if miss: continue
        for c in range(lo, hi + 1):
            if want(c): cat[c] = cat_map[f[0]]
    pos = {}
    for lo, hi, f, miss in ranges("IndicPositionalCategory.txt"):
        if miss: continue
        for c in range(lo, hi + 1):
            if want(c): pos[c] = pos_map[f[0]]
    # HarfBuzz's category overrides
    for c in (0x0953, 0x0954): cat[c] = "SM"
    for c in (0x0A72, 0x0A73, 0x1CF5, 0x1CF6): cat[c] = "C"
    for c in range(0x1CE2, 0x1CE9): cat[c] = "A"
    cat[0x1CED] = "A"
    for c in list(range(0xA8F2, 0xA8F8)) + list(range(0x1CE9, 0x1CED)) + list(range(0x1CEE, 0x1CF2)): cat[c] = "Symbol"
    cat[0x0A51] = "M"; pos[0x0A51] = "BELOW_C"
    for c in (0x11301, 0x11303): cat[c] = "SM"
    cat[0x1133C] = "N"
    cat[0x0AFB] = "N"; cat[0x0B55] = "N"
    for c in (0x0980, 0x09FC, 0x0C80, 0x2010, 0x2011): cat[c] = "PLACEHOLDER"
    cat[0x25CC] = "DOTTEDCIRCLE"
    cat[0x200C] = "ZWNJ"; cat[0x200D] = "ZWJ"
    ra = {0x0930, 0x09B0, 0x09F0, 0x0A30, 0x0AB0, 0x0B30, 0x0BB0, 0x0C30, 0x0CB0, 0x0D30}
    def block(c):
        for name, lo in (("Deva", 0x0900), ("Beng", 0x0980), ("Guru", 0x0A00), ("Gujr", 0x0A80), ("Orya", 0x0B00),
                         ("Taml", 0x0B80), ("Telu", 0x0C00), ("Knda", 0x0C80), ("Mlym", 0x0D00), ("Sinh", 0x0D80)):
            if lo <= c < lo + 0x80: return name
        return ""
    def matra(c, side):
        b = block(c)
        if side == "PRE_C": return "PRE_M"
        if side == "POST_C":
            if b == "Deva": return "AFTER_SUB"
            if b in ("Beng", "Guru", "Gujr", "Orya", "Taml", "Mlym"): return "AFTER_POST"
            if b == "Telu": return "BEFORE_SUB" if c <= 0x0C42 else "AFTER_SUB"
            if b == "Knda": return "BEFORE_SUB" if (c < 0x0CC3 or c > 0x0CD6) else "AFTER_SUB"
            return "AFTER_SUB"
        if side == "ABOVE_C":
            if b in ("Deva", "Gujr", "Taml", "Sinh"): return "AFTER_SUB"
            if b == "Guru": return "AFTER_POST"
            if b == "Orya": return "AFTER_MAIN"
            if b in ("Telu", "Knda"): return "BEFORE_SUB"
            return "AFTER_SUB"
        if side == "BELOW_C":
            if b in ("Deva", "Beng", "Orya", "Sinh"): return "AFTER_SUB"
            if b in ("Guru", "Gujr", "Taml", "Mlym"): return "AFTER_POST"
            if b in ("Telu", "Knda"): return "BEFORE_SUB"
            return "AFTER_SUB"
        return side
    out = []
    cons = {"C", "CS", "Ra", "CM", "V", "PLACEHOLDER", "DOTTEDCIRCLE"}
    for c in sorted(set(cat) | set(pos)):
        k = cat.get(c, "X")
        p = pos.get(c, "END")
        if k == "X" and c not in pos: continue
        if k in cons:
            p = "BASE_C"
            if c in ra: k = "Ra"
        elif k == "M":
            if c != 0x0A51: p = matra(c, p)
        elif k in ("SM", "A", "Symbol"):
            p = "SMVD"
        if c == 0x0B01: p = "BEFORE_SUB"
        out.append((c, I_CAT[k], I_POS[p]))
    return out

def main():
    ud = unicode_data()
    # ---- general category: the 30 two-letter values, in UCD order -------------------------------
    gc_names = ["Cc", "Cf", "Cn", "Co", "Cs", "Ll", "Lm", "Lo", "Lt", "Lu", "Mc", "Me", "Mn", "Nd", "Nl", "No",
                "Pc", "Pd", "Pe", "Pf", "Pi", "Po", "Ps", "Sc", "Sk", "Sm", "So", "Zl", "Zp", "Zs"]
    gc = [gc_names.index("Cn")] * N
    for c, r in ud.items(): gc[c] = gc_names.index(r[1])
    # ---- bidi class --------------------------------------------------------------------------------
    bc_names = ["L", "R", "AL", "EN", "ES", "ET", "AN", "CS", "NSM", "BN", "B", "S", "WS", "ON",
                "LRE", "LRO", "RLE", "RLO", "PDF", "LRI", "RLI", "FSI", "PDI"]
    _, bc_l2s = enum_order("bc")
    bct = prop_table("DerivedBidiClass.txt", "L", "extracted", value_map=lambda v: bc_l2s[v])
    bc = [bc_names.index(v) for v in bct]
    # ---- script (ISO 15924 short names, PropertyValueAliases order) -----------------------------
    sc_names, sc_l2s = enum_order("sc")
    sct = prop_table("Scripts.txt", "Zzzz", value_map=lambda v: sc_l2s[v])
    sc = [sc_names.index(v) for v in sct]
    # ---- line break --------------------------------------------------------------------------------
    lb_names, lb_l2s = enum_order("lb")
    lbt = prop_table("LineBreak.txt", "XX", value_map=lambda v: lb_l2s[v])
    lb = [lb_names.index(v) for v in lbt]
    # ---- combining class --------------------------------------------------------------------------
    ccc = [0] * N
    for c, r in ud.items(): ccc[c] = r[2]
    # ---- joining type --------------------------------------------------------------------------------
    jt_names = ["U", "R", "D", "C", "L", "T"]
    _, jt_l2s = enum_order("jt")
    jtt = prop_table("DerivedJoiningType.txt", "U", "extracted", value_map=lambda v: jt_l2s[v])
    jt = [jt_names.index(v) for v in jtt]
    # ---- east asian width --------------------------------------------------------------------------
    ea_names = ["N", "A", "F", "H", "Na", "W"]
    _, ea_l2s = enum_order("ea")
    eat = prop_table("EastAsianWidth.txt", "N", value_map=lambda v: ea_l2s[v])
    ea = [ea_names.index(v) for v in eat]
    # ---- flags ------------------------------------------------------------------------------------------
    flag_names = ["Extended_Pictographic", "Emoji_Presentation", "Emoji_Modifier", "Emoji_Modifier_Base",
                  "Default_Ignorable_Code_Point", "Emoji", "Grapheme_Extend", "Bidi_Mirrored"]
    flags = [0] * N
    def setflag(fn, sub, name, bit):
        for lo, hi, f, miss in ranges(fn, sub):
            if miss or f[0] != name: continue
            for c in range(lo, hi + 1): flags[c] |= bit
    setflag("emoji-data.txt", "emoji", "Extended_Pictographic", 1)
    setflag("emoji-data.txt", "emoji", "Emoji_Presentation", 2)
    setflag("emoji-data.txt", "emoji", "Emoji_Modifier", 4)
    setflag("emoji-data.txt", "emoji", "Emoji_Modifier_Base", 8)
    setflag("DerivedCoreProperties.txt", "", "Default_Ignorable_Code_Point", 16)
    setflag("emoji-data.txt", "emoji", "Emoji", 32)
    setflag("DerivedCoreProperties.txt", "", "Grapheme_Extend", 64)
    # Bidi_Mirrored is field 9 of UnicodeData
    for line in open(fetch("UnicodeData.txt"), encoding="utf-8"):
        f = line.split(";")
        if f[9] == "Y": flags[int(f[0], 16)] |= 128
    # ---- records + trie ------------------------------------------------------------------------------
    recs = {}
    rec_list = []
    idx = [0] * N
    for c in range(N):
        t = (gc[c], bc[c], sc[c], lb[c], ccc[c], jt[c], ea[c], flags[c])
        r = recs.get(t)
        if r is None:
            r = len(rec_list); recs[t] = r; rec_list.append(t)
        idx[c] = r
    assert len(rec_list) < 65536
    B3 = 64; B2 = 32
    s3_blocks = {}; s3 = []; s2_entries = []
    for i in range(0, N, B3):
        b = tuple(idx[i:i + B3])
        if b not in s3_blocks: s3_blocks[b] = len(s3_blocks); s3.append(b)
        s2_entries.append(s3_blocks[b])
    s2_blocks = {}; s2 = []; s1 = []
    for i in range(0, len(s2_entries), B2):
        b = tuple(s2_entries[i:i + B2])
        if b not in s2_blocks: s2_blocks[b] = len(s2_blocks); s2.append(b)
        s1.append(s2_blocks[b])
    # ---- side tables ----------------------------------------------------------------------------------
    mirror = []
    for lo, hi, f, miss in ranges("BidiMirroring.txt"):
        if not miss: mirror.append((lo, int(f[0], 16)))
    mirror.sort()
    brackets = []
    for lo, hi, f, miss in ranges("BidiBrackets.txt"):
        if not miss: brackets.append((lo, int(f[0], 16), 0 if f[1] == "o" else 1))
    brackets.sort()
    decomp = []
    for c, r in sorted(ud.items()):
        d = r[4]
        if not d or d.startswith("<"): continue
        parts = [int(x, 16) for x in d.split()]
        assert len(parts) <= 2
        decomp.append((c, parts[0], parts[1] if len(parts) > 1 else 0))
    excl = set()
    for lo, hi, f, miss in ranges("DerivedNormalizationProps.txt"):
        if not miss and f[0] == "Full_Composition_Exclusion":
            for c in range(lo, hi + 1): excl.add(c)
    comp = sorted((a, b, c) for (c, a, b) in decomp if b != 0 and c not in excl)
    # ---- Indic categories (HarfBuzz's Indic shaper categories and positions) ---------------------
    indic = indic_table()
    # ---- grapheme clusters: Grapheme_Cluster_Break and Indic_Conjunct_Break (UAX #29) ------------
    gcb_names = ["Other", "CR", "LF", "Control", "Extend", "ZWJ", "Regional_Indicator", "Prepend",
                 "SpacingMark", "L", "V", "T", "LV", "LVT"]
    gcbt = prop_table("GraphemeBreakProperty.txt", "Other", "auxiliary")
    incb_names = ["None", "Linker", "Consonant", "Extend"]
    incb = ["None"] * N
    for lo, hi, f, miss in ranges("DerivedCoreProperties.txt"):
        if miss or f[0] != "InCB": continue
        for c in range(lo, hi + 1): incb[c] = f[1]
    gcb_ranges = []
    c = 0
    while c < N:
        k = (gcb_names.index(gcbt[c]), incb_names.index(incb[c]))
        e = c
        while e + 1 < N and (gcb_names.index(gcbt[e + 1]), incb_names.index(incb[e + 1])) == k: e += 1
        # Hangul syllables (LV/LVT, alternating every 28) are computed, not stored: ucd_gcb knows them
        if k != (0, 0) and not (0xAC00 <= c <= 0xD7A3): gcb_ranges.append((c, e, k[0], k[1]))
        c = e + 1
    # ---- blob -----------------------------------------------------------------------------------------
    out = bytearray()
    hdr_n = 18
    out += b"\0" * (hdr_n * 4)
    def put_hdr(i, v): struct.pack_into(">I", out, i * 4, v)
    put_hdr(0, len(out)); out += b"".join(struct.pack(">H", v) for v in s1)
    put_hdr(1, len(out))
    for b in s2: out += b"".join(struct.pack(">H", v) for v in b)
    put_hdr(2, len(out))
    for b in s3: out += b"".join(struct.pack(">H", v) for v in b)
    put_hdr(3, len(out)); put_hdr(4, len(rec_list))
    for t in rec_list: out += bytes(t)
    put_hdr(5, len(out)); put_hdr(6, len(mirror))
    for a, b in mirror: out += struct.pack(">II", a, b)
    put_hdr(7, len(out)); put_hdr(8, len(brackets))
    for a, b, t in brackets: out += struct.pack(">III", a, b, t)
    put_hdr(9, len(out)); put_hdr(10, len(decomp))
    for c, a, b in decomp: out += struct.pack(">III", c, a, b)
    put_hdr(11, len(out)); put_hdr(12, len(comp))
    for a, b, c in comp: out += struct.pack(">III", a, b, c)
    put_hdr(14, len(out)); put_hdr(15, len(indic))
    for cp, cat, pos in indic: out += struct.pack(">IBB", cp, cat, pos)
    put_hdr(16, len(out)); put_hdr(17, len(gcb_ranges))
    for lo, hi, g, ib in gcb_ranges: out += struct.pack(">IIBB", lo, hi, g, ib)
    put_hdr(13, len(out))
    os.makedirs(os.path.join(FONT, "data"), exist_ok=True)
    open(os.path.join(FONT, "data", "ucd.bin"), "wb").write(bytes(out).replace(b"\\", b"\\\\"))
    # ---- constants ------------------------------------------------------------------------------------
    L = []
    L.append("// GENERATED by std/font/tools/gen_ucd.py from the Unicode 16.0.0 data files. Do not edit.")
    L.append("// The values are the encodings std/font/data/ucd.bin was written with; ucd.nori reads it.")
    L.append("")
    L.append("global g_ucd_blob: Str")
    L.append("global g_ucd_on: Int = 0")
    L.append("// the property blob, made once (include_str is a literal; a global given one keeps a single copy)")
    L.append("fn ucd_blob() -> view Str {")
    L.append("    if g_ucd_on == 0 { g_ucd_blob = include_str(\"data/ucd.bin\")  g_ucd_on = 1 }")
    L.append("    return g_ucd_blob")
    L.append("}")
    L.append("/// the size in bytes of the encoded property blob (a test checks it against the header)")
    L.append("pub fn ucd_blob_size() -> Int { return %d }" % len(out))
    def consts(prefix, names, doc):
        L.append("")
        L.append("// " + doc)
        for i, n in enumerate(names):
            ident = n.replace("-", "_")
            L.append("pub fn %s_%s() -> Int { return %d }" % (prefix, ident, i))
    consts("GC", gc_names, "General_Category")
    consts("BC", bc_names, "Bidi_Class")
    consts("LB", lb_names, "Line_Break")
    consts("JT", jt_names, "Joining_Type")
    consts("EA", ea_names, "East_Asian_Width")
    consts("GCB", gcb_names, "Grapheme_Cluster_Break")
    consts("INCB", incb_names, "Indic_Conjunct_Break")
    consts("SC", sc_names, "Script (ISO 15924 codes, in PropertyValueAliases order)")
    L.append("")
    L.append("/// the ISO 15924 code of script value `s` (\"Zzzz\" when out of range)")
    L.append("pub fn script_code(s: Int) -> Str {")
    L.append("    let names = \"" + "".join(sc_names) + "\"")
    L.append("    if s < 0 || s >= %d { return \"Zzzz\" }" % len(sc_names))
    L.append("    return names.slice(s * 4, s * 4 + 4)")
    L.append("}")
    L.append("/// the script value of an ISO 15924 code (case as written, e.g. \"Arab\"); SC_Zzzz when unknown")
    L.append("pub fn script_from_code(code: Str) -> Int {")
    L.append("    let names = \"" + "".join(sc_names) + "\"")
    L.append("    var i = 0")
    L.append("    while i < %d { if names.slice(i * 4, i * 4 + 4) == code { return i }  i = i + 1 }" % len(sc_names))
    L.append("    return %d" % sc_names.index("Zzzz"))
    L.append("}")
    L.append("pub fn SC_COUNT() -> Int { return %d }" % len(sc_names))
    L.append("")
    open(os.path.join(FONT, "ucd_gen.nori"), "w").write("\n".join(L))
    print("ucd.bin %d bytes; %d records, s2 %d blocks, s3 %d blocks; mirror %d brackets %d decomp %d comp %d" %
          (len(out), len(rec_list), len(s2), len(s3), len(mirror), len(brackets), len(decomp), len(comp)))

main()
