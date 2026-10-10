#!/usr/bin/env python3
# std/video/make_vp9_txfm.py: writes std/video/vp9_txfm.nori: the VP9 inverse transforms as straight-line code, made by
# running the specification's own procedures (v0.7 §8.7.1: the butterflies B, H, SB, SH, the inverse DCT array
# permutation and the recursive inverse DCT process, the ADST input/output permutations, ADST4, ADST8, ADST16) on
# symbolic values: every step the specification lists becomes one assignment, in its order. Nothing here is taken
# from another decoder; the program below is §8.7.1 transcribed.
#
#   python3 std/video/make_vp9_txfm.py > std/video/vp9_txfm.nori
#
# Each generated function transforms n values in place, at address p, p + st, p + 2*st, … (64-bit integers; st is
# in bytes), so the 2D transform (§8.7.2) runs it over rows (st = 8) and then columns (st = 8 * n).

COS64 = [16384, 16364, 16305, 16207, 16069, 15893, 15679, 15426, 15137, 14811, 14449, 14053, 13623, 13160, 12665, 12140,
         11585, 11003, 10394, 9760, 9102, 8423, 7723, 7005, 6270, 5520, 4756, 3981, 3196, 2404, 1606, 804, 0]

def cos64(angle):
    a2 = angle & 127
    if a2 <= 32: return COS64[a2]
    if a2 <= 64: return -COS64[64 - a2]
    if a2 <= 96: return -COS64[a2 - 64]
    return COS64[128 - a2]
def sin64(angle): return cos64(angle - 32)

def brev(nb, x):
    t = 0
    for i in range(nb):
        t += ((x >> i) & 1) << (nb - 1 - i)
    return t

ROUND14 = '((%s + 4611686018427396096) >> 14) - 281474976710656'   # Round2(x, 14), x of either sign (|x| < 2^62)

class Gen:
    def __init__(self, n):
        self.T = ['t%d' % i for i in range(n)]
        self.S = [None] * n
        self.lines = []
        self.k = 0
    def tmp(self, expr):
        name = 'v%d' % self.k
        self.k += 1
        self.lines.append('let %s = %s' % (name, expr))
        return name
    @staticmethod
    def mul(v, c):
        if c == 0: return None
        if c == 1: return v
        if c == -1: return '(0 - %s)' % v
        return '%s * %d' % (v, c) if c > 0 else '%s * (0 - %d)' % (v, -c)
    def lin(self, terms):
        """sum of (value, coefficient) terms as an expression"""
        parts = []
        for v, c in terms:
            if c == 0: continue
            if not parts:
                parts.append(self.mul(v, c))
            elif c > 0:
                parts.append('+ ' + (v if c == 1 else '%s * %d' % (v, c)))
            else:
                parts.append('- ' + (v if c == -1 else '%s * %d' % (v, -c)))
        return ' '.join(parts) if parts else '0'
    # §8.7.1.1
    def B(self, a, b, angle, flip):
        ta, tb = self.T[a], self.T[b]
        x = self.tmp(ROUND14 % ('(' + self.lin([(ta, cos64(angle)), (tb, -sin64(angle))]) + ')'))
        y = self.tmp(ROUND14 % ('(' + self.lin([(ta, sin64(angle)), (tb, cos64(angle))]) + ')'))
        self.T[a], self.T[b] = x, y
        if flip: self.T[a], self.T[b] = self.T[b], self.T[a]
    def H(self, a, b, flip):
        if flip: a, b = b, a
        x, y = self.T[a], self.T[b]
        s = self.tmp('%s + %s' % (x, y))
        d = self.tmp('%s - %s' % (x, y))
        self.T[a], self.T[b] = s, d
    def SB(self, a, b, angle, flip):
        ta, tb = self.T[a], self.T[b]
        sa = self.tmp(self.lin([(ta, cos64(angle)), (tb, -sin64(angle))]))
        sb = self.tmp(self.lin([(ta, sin64(angle)), (tb, cos64(angle))]))
        self.S[a], self.S[b] = sa, sb
        if flip: self.S[a], self.S[b] = self.S[b], self.S[a]
    def SH(self, a, b):
        sa, sb = self.S[a], self.S[b]
        x = self.tmp(ROUND14 % ('(%s + %s)' % (sa, sb)))
        y = self.tmp(ROUND14 % ('(%s - %s)' % (sa, sb)))
        self.T[a], self.T[b] = x, y
    # §8.7.1.2
    def dct_permute(self, n):
        c = list(self.T)
        for i in range(1 << n): self.T[i] = c[brev(n, i)]
    # §8.7.1.3
    def idct(self, n):
        n0, n1, n2, n3 = 1 << n, 1 << (n - 1), 1 << (n - 2), (1 << (n - 3)) if n >= 3 else 0
        if n == 2: self.B(0, 1, 16, 1)
        else: self.idct(n - 1)
        for i in range(n2): self.B(n1 + i, n0 - 1 - i, 32 - brev(5, n1 + i), 0)
        if n >= 3:
            for i in range(n3):
                for j in range(2): self.H(n1 + 4 * i + 2 * j, n1 + 1 + 4 * i + 2 * j, j)
        if n == 5:
            for i in range(2):
                for j in range(2): self.B(n0 - n + 3 - n2 * j - 4 * i, n1 + n - 4 + n2 * j + 4 * i, 28 - 16 * i + 56 * j, 1)
            for i in range(2):
                for j in range(4): self.H(n1 + n3 * j + i, n1 + n2 - 5 + n3 * j - i, j & 1)
        if n >= 4:
            for i in range(2 if n == 5 else 1):
                for j in range(2): self.B(n0 - n + 2 - i - n2 * j, n1 + n - 3 + i + n2 * j, 24 + 48 * j, 1)
            for i in range(2 * n - 6):
                for j in range(2): self.H(n1 + n2 * j + i, n1 + n2 - 1 + n2 * j - i, j & 1)
        if n >= 3:
            for i in range(n3): self.B(n0 - n3 - 1 - i, n1 + n3 + i, 16, 1)
        for i in range(n1): self.H(i, n0 - 1 - i, 0)
    # §8.7.1.4, §8.7.1.5
    def adst_in(self, n):
        n0, n1 = 1 << n, 1 << (n - 1)
        c = list(self.T)
        for i in range(n1):
            self.T[2 * i] = c[n0 - 1 - 2 * i]
            self.T[2 * i + 1] = c[2 * i]
    def adst_out(self, n):
        c = list(self.T)
        if n == 4:
            for a in range(2):
                for b in range(2):
                    for cc in range(2):
                        for d in range(2):
                            self.T[8 * a + 4 * b + 2 * cc + d] = c[8 * (d ^ cc) + 4 * (cc ^ b) + 2 * (b ^ a) + a]
        else:
            for a in range(2):
                for b in range(2):
                    for cc in range(2):
                        self.T[4 * a + 2 * b + cc] = c[4 * (cc ^ b) + 2 * (b ^ a) + a]
    def neg(self, i):
        self.T[i] = self.tmp('0 - %s' % self.T[i])
    # §8.7.1.6
    def adst4(self):
        S1, S2, S3, S4 = 5283, 9929, 13377, 15212
        t = self.T
        s0 = self.tmp('%s * %d' % (t[0], S1))
        s1 = self.tmp('%s * %d' % (t[0], S2))
        s2 = self.tmp('%s * %d' % (t[1], S3))
        s3 = self.tmp('%s * %d' % (t[2], S4))
        s4 = self.tmp('%s * %d' % (t[2], S1))
        s5 = self.tmp('%s * %d' % (t[3], S2))
        s6 = self.tmp('%s * %d' % (t[3], S4))
        v = self.tmp('%s - %s + %s' % (t[0], t[2], t[3]))
        s7 = self.tmp('%s * %d' % (v, S3))
        x0 = self.tmp('%s + %s + %s' % (s0, s3, s5))
        x1 = self.tmp('%s - %s - %s' % (s1, s4, s6))
        x2, x3 = s7, s2
        o0 = self.tmp('%s + %s' % (x0, x3))
        o1 = self.tmp('%s + %s' % (x1, x3))
        o2 = x2
        o3 = self.tmp('%s + %s - %s' % (x0, x1, x3))
        self.T = [self.tmp(ROUND14 % ('(' + o + ')')) for o in (o0, o1, o2, o3)]
    # §8.7.1.7
    def adst8(self):
        self.adst_in(3)
        for i in range(4): self.SB(2 * i, 1 + 2 * i, 30 - 8 * i, 1)
        for i in range(4): self.SH(i, 4 + i)
        for i in range(2): self.SB(4 + 3 * i, 5 + i, 24 - 16 * i, 1)
        for i in range(2): self.SH(4 + i, 6 + i)
        for i in range(2): self.H(i, 2 + i, 0)
        for i in range(2): self.B(2 + 4 * i, 3 + 4 * i, 16, 1)
        self.adst_out(3)
        for i in range(4): self.neg(1 + 2 * i)
    # §8.7.1.8
    def adst16(self):
        self.adst_in(4)
        for i in range(8): self.SB(2 * i, 1 + 2 * i, 31 - 4 * i, 1)
        for i in range(8): self.SH(i, 8 + i)
        for i in range(4): self.SB(8 + 2 * i, 9 + 2 * i, 28 - 16 * i, 1)
        for i in range(4): self.SH(8 + i, 12 + i)
        for i in range(4): self.H(i, 4 + i, 0)
        for i in range(2):
            for j in range(2): self.SB(4 + 8 * i + 3 * j, 5 + 8 * i + j, 24 - 16 * j, 1)
        for i in range(2):
            for j in range(2): self.SH(4 + 8 * j + i, 6 + 8 * j + i)
        for i in range(2):
            for j in range(2): self.H(8 * j + i, 2 + 8 * j + i, 0)
        for i in range(2):
            for j in range(2): self.B(2 + 4 * j + 8 * i, 3 + 4 * j + 8 * i, 48 + 64 * (i ^ j), 0)
        self.adst_out(4)
        for i in range(2):
            for j in range(2): self.neg(1 + 12 * j + 2 * i)

def emit(name, n, run, what):
    g = Gen(1 << n)
    run(g)
    out = ['/// %s' % what, 'fn %s(p: Int, st: Int) {' % name, '    unsafe {']
    for i in range(1 << n):
        out.append('        let t%d = peek(p + %s)' % (i, '0' if i == 0 else ('st' if i == 1 else 'st * %d' % i)))
    for l in g.lines: out.append('        ' + l)
    for i in range(1 << n):
        out.append('        poke(p + %s, %s)' % ('0' if i == 0 else ('st' if i == 1 else 'st * %d' % i), g.T[i]))
    out += ['    }', '}', '']
    return '\n'.join(out)

print('// std/video/vp9_txfm.nori — the VP9 inverse transforms (§8.7.1), written by std/video/make_vp9_txfm.py from the')
print('// specification\'s procedures: each 1D transform as straight-line code, in place over n 64-bit values at p, p + st, …')
print('// Round2(x, 14) is written ((x + 2^62 + 2^13) >> 14) - 2^48: Nori\'s >> is a logical shift, the offset makes it an')
print('// arithmetic one for |x| < 2^62.')
print()
for n in (2, 3, 4, 5):
    def run(g, n=n):
        g.dct_permute(n)
        g.idct(n)
    print(emit('v9_idct%d' % (1 << n), n, run, 'the inverse DCT of %d values: the array permutation (§8.7.1.2), then §8.7.1.3' % (1 << n)))
print(emit('v9_iadst4', 2, lambda g: g.adst4(), 'the inverse ADST of 4 values (§8.7.1.6)'))
print(emit('v9_iadst8', 3, lambda g: g.adst8(), 'the inverse ADST of 8 values (§8.7.1.7)'))
print(emit('v9_iadst16', 4, lambda g: g.adst16(), 'the inverse ADST of 16 values (§8.7.1.8)'))

# §8.8.5.3, the wide filter, for log2Size 3 and 4: F[i] = Round2(s[i] + sum over j = -n..n of s[Clip3(-(n+1), n, i+j)],
# log2Size) for i = -n..n-1, written out (s[k] is the sample k steps past the edge: q0 at 0, p0 at -1)
def off(k):
    if k == 0: return '+ 0'
    if k == 1: return '+ st'
    if k == -1: return '- st'
    return ('+ st * %d' % k) if k > 0 else ('- st * %d' % -k)

def wide(log2):
    n = (1 << (log2 - 1)) - 1
    name = lambda k: ('q%d' % k) if k >= 0 else ('p%d' % (-k - 1))
    out = ['/// the wide filter of §8.8.5.3 with log2Size %d (%d samples changed on each side), written out' % (log2, n),
           'fn v9_lf_wide%d(p: Int, st: Int) {' % (1 << log2), '    unsafe {']
    for k in range(-n - 1, n + 1):
        out.append('        let %s = peek8(p %s)' % (name(k), off(k)))
    for i in range(-n, n):
        terms = {}
        terms[i] = terms.get(i, 0) + 1
        for j in range(-n, n + 1):
            k = max(-(n + 1), min(n, i + j))
            terms[k] = terms.get(k, 0) + 1
        expr = ' + '.join((name(k) if c == 1 else '%s * %d' % (name(k), c)) for k, c in sorted(terms.items()))
        out.append('        poke8(p %s, (%s + %d) >> %d)' % (off(i), expr, 1 << (log2 - 1), log2))
    out += ['    }', '}', '']
    return '\n'.join(out)
print(wide(3))
print(wide(4))
