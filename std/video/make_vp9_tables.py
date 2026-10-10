#!/usr/bin/env python3
# std/video/make_vp9_tables.py: writes std/video/vp9_tables.nori from the text of the VP9 specification
# ("VP9 Bitstream & Decoding Process Specification v0.7", 22 February 2017): every constant table the decoder uses,
# taken from the specification's own text (pdftotext -layout of the PDF), so no number is typed by hand. Symbolic
# entries (BLOCK_*, TX_*, the transform types, the mode-context names) are the specification's own enumerations.
#
#   pdftotext -layout vp9-bitstream-specification-v0.7-20170222-draft.pdf vp9.txt
#   python3 std/video/make_vp9_tables.py vp9.txt > std/video/vp9_tables.nori
import re, sys

text = open(sys.argv[1]).read()
lines = [l for l in text.split('\n') if 'Copyright © 2017 Google' not in l and 'VP9 Bitstream & Decoding Process Specification - v0.7' not in l]
text = '\n'.join(lines)

SYM = {'BLOCK_INVALID': 13}
for i, n in enumerate(['4X4', '4X8', '8X4', '8X8', '8X16', '16X8', '16X16', '16X32', '32X16', '32X32', '32X64', '64X32', '64X64']):
    SYM['BLOCK_' + n] = i
for i, n in enumerate(['TX_4X4', 'TX_8X8', 'TX_16X16', 'TX_32X32']): SYM[n] = i
for i, n in enumerate(['DCT_DCT', 'ADST_DCT', 'DCT_ADST', 'ADST_ADST']): SYM[n] = i
for i, n in enumerate(['BOTH_ZERO', 'ZERO_PLUS_PREDICTED', 'BOTH_PREDICTED', 'NEW_PLUS_NON_INTRA', 'BOTH_NEW',
                       'INTRA_PLUS_NON_INTRA', 'BOTH_INTRA', 'INVALID_CASE']): SYM[n] = i
for i, n in enumerate(['EIGHTTAP', 'EIGHTTAP_SMOOTH', 'EIGHTTAP_SHARP', 'BILINEAR']): SYM[n] = i

def body(name, nth=0):
    """the initializer of table `name` (its nth definition), comments removed"""
    pos = -1
    for _ in range(nth + 1):
        m = re.compile(r'\b' + re.escape(name) + r'(\s*\[[^\]=]*\])+\s*=\s*\{').search(text, pos + 1)
        if not m: raise SystemExit('no table ' + name)
        pos = m.start()
    j = text.index('{', pos)
    depth, k = 0, j
    while True:
        if text[k] == '{': depth += 1
        elif text[k] == '}':
            depth -= 1
            if depth == 0: break
        k += 1
    b = text[j:k + 1]
    b = re.sub(r'/\*.*?\*/', '', b, flags=re.S)
    b = re.sub(r'//[^\n]*', '', b)
    return b

def values(b):
    out = []
    for t in re.findall(r'-?\s*[A-Za-z_][A-Za-z_0-9]*|-?\d+', b):
        t = re.sub(r'\s', '', t)
        if re.fullmatch(r'-?\d+', t): out.append(int(t))
        else:
            neg = t.startswith('-')
            v = SYM[t.lstrip('-')]
            out.append(-v if neg else v)
    return out

def flat(name, n=None, nth=0):
    v = values(body(name, nth))
    if n is not None and len(v) != n: raise SystemExit('%s: %d values, not %d' % (name, len(v), n))
    return v

def coef_probs():
    """default_coef_probs[4][2][2][6][6][3]: band 0 has 3 contexts in the text; padded to 6 (the others unused)"""
    b = body('default_coef_probs')
    # the innermost groups are the 3-probability triples; bands are the groups of triples
    out = []
    # walk the nesting: depth of '{' tells tx size / block type / ref type / band / context
    depth = 0
    band = []
    for tok in re.findall(r'\{|\}|\d+', b):
        if tok == '{':
            depth += 1
            if depth == 5: band = []
        elif tok == '}':
            if depth == 5:
                triples = [band[i:i + 3] for i in range(0, len(band), 3)]
                while len(triples) < 6: triples.append([128, 128, 128])
                for t in triples: out.extend(t)
            depth -= 1
        else:
            band.append(int(tok))
    if len(out) != 4 * 2 * 2 * 6 * 6 * 3: raise SystemExit('coef probs: %d' % len(out))
    return out

T = {}
for n, c in [('default_scan_4x4', 16), ('col_scan_4x4', 16), ('row_scan_4x4', 16), ('default_scan_8x8', 64), ('col_scan_8x8', 64),
             ('row_scan_8x8', 64), ('default_scan_16x16', 256), ('col_scan_16x16', 256), ('row_scan_16x16', 256),
             ('default_scan_32x32', 1024), ('b_width_log2_lookup', 13), ('b_height_log2_lookup', 13),
             ('num_4x4_blocks_wide_lookup', 13), ('num_4x4_blocks_high_lookup', 13), ('mi_width_log2_lookup', 13),
             ('num_8x8_blocks_wide_lookup', 13), ('mi_height_log2_lookup', 13), ('num_8x8_blocks_high_lookup', 13),
             ('size_group_lookup', 13), ('tx_mode_to_biggest_tx_size', 5), ('subsize_lookup', 52), ('coefband_4x4', 16),
             ('coefband_8x8plus', 1024), ('energy_class', 12), ('mode2txfm_map', 14), ('pareto_table', 1024),
             ('kf_partition_probs', 48), ('kf_y_mode_probs', 900), ('kf_uv_mode_probs', 90),
             ('default_partition_probs', 48), ('default_y_mode_probs', 36), ('default_uv_mode_probs', 90),
             ('default_skip_prob', 3), ('default_is_inter_prob', 4), ('default_comp_mode_prob', 5),
             ('default_comp_ref_prob', 5), ('default_single_ref_prob', 10), ('default_mv_sign_prob', 2),
             ('default_mv_bits_prob', 20), ('default_mv_class0_bit_prob', 2), ('default_inter_mode_probs', 21),
             ('default_interp_filter_probs', 8), ('default_mv_joint_probs', 3), ('default_mv_class_probs', 20),
             ('default_mv_class0_fr_probs', 12), ('default_mv_class0_hp_prob', 2), ('default_mv_fr_probs', 6),
             ('default_mv_hp_prob', 2), ('inv_map_table', 255), ('dc_qlookup', 768), ('ac_qlookup', 768),
             ('cos64_lookup', 33), ('subpel_filters', 512), ('mv_ref_blocks', 208), ('mode_2_counter', 14),
             ('counter_to_context', 19), ('max_txsize_lookup', 13), ('ss_size_lookup', 52), ('literal_to_type', 4)]:
    T[n] = flat(n, c)
# default_tx_probs: [TX_SIZES][TX_SIZE_CONTEXTS][TX_SIZES-1] written as 8x8 {..1}, 16x16 {..2}, 32x32 {..3} per context
T['default_tx_probs'] = flat('default_tx_probs')
T['default_coef_probs'] = coef_probs()

print('// std/video/vp9_tables.nori — the VP9 specification\'s constant tables (v0.7: sections 6.2-6.5, 8.5-8.7, 10), written')
print('// by std/video/make_vp9_tables.py from the specification\'s own text: nothing here is typed by hand. Each table is a')
print('// function returning it flattened in C order (the last index fastest); the decoder copies the ones it reads in its')
print('// inner loops into raw memory once (vp9.nori, v9_tables_init).')
print()
for n, v in T.items():
    print('fn v9t_%s() -> Vec<Int> {' % n)
    print('    let t: Vec<Int> = [')
    for i in range(0, len(v), 24):
        print('        ' + ', '.join(map(str, v[i:i + 24])) + (',' if i + 24 < len(v) else ''))
    print('    ]')
    print('    return copy t')
    print('}')
print()
sizes = ', '.join('%s %d' % (n, len(v)) for n, v in T.items())
print('// sizes: ' + sizes)
