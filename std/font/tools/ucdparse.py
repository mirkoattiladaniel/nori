# ucdparse.py: shared Unicode Character Database readers for std/font's generators.
import os, re
N = 0x110000

def ucd_dir():
    d = os.environ.get("UCD_DIR") or os.path.expanduser("~/.cache/ucd16")
    return d

def fetch(name, sub=""):
    """path to a UCD 16.0.0 file, downloading it into the cache directory when absent."""
    d = ucd_dir()
    os.makedirs(d, exist_ok=True)
    p = os.path.join(d, name)
    if not os.path.exists(p):
        import urllib.request
        url = "https://www.unicode.org/Public/16.0.0/ucd/" + (sub + "/" if sub else "") + name
        urllib.request.urlretrieve(url, p)
    return p

def ranges(fn, sub=""):
    """yield (lo, hi, fields) for each data line; `# @missing:` lines first, as (lo, hi, fields, True)."""
    for line in open(fetch(fn, sub), encoding="utf-8"):
        m = re.match(r"#\s*@missing:\s*([0-9A-Fa-f]+)\.\.([0-9A-Fa-f]+)\s*;\s*(.*)$", line)
        if m:
            yield int(m.group(1), 16), int(m.group(2), 16), [x.strip() for x in m.group(3).split(";")], True
            continue
        line = line.split("#")[0].strip()
        if not line: continue
        parts = [x.strip() for x in line.split(";")]
        r = parts[0]
        if ".." in r:
            a, b = r.split("..")
            lo, hi = int(a, 16), int(b, 16)
        else:
            lo = hi = int(r, 16)
        yield lo, hi, parts[1:], False

def prop_table(fn, default, sub="", field=0, value_map=None):
    """a full codepoint -> value list for a property file, honouring @missing defaults."""
    t = [default] * N
    missing = []
    explicit = []
    for lo, hi, f, miss in ranges(fn, sub):
        v = f[field]
        if value_map is not None: v = value_map(v)
        (missing if miss else explicit).append((lo, hi, v))
    for lo, hi, v in missing:
        for c in range(lo, hi + 1): t[c] = v
    for lo, hi, v in explicit:
        for c in range(lo, hi + 1): t[c] = v
    return t

def unicode_data():
    """codepoint -> (name, gc, ccc, bidi, decomposition) from UnicodeData.txt, with First/Last ranges."""
    out = {}
    first = None
    for line in open(fetch("UnicodeData.txt"), encoding="utf-8"):
        f = line.rstrip("\n").split(";")
        cp = int(f[0], 16)
        rec = (f[1], f[2], int(f[3]), f[4], f[5])
        if f[1].endswith(", First>"): first = cp; continue
        if f[1].endswith(", Last>"):
            for c in range(first, cp + 1): out[c] = rec
            continue
        out[cp] = rec
    return out

def aliases(prop):
    """short <-> long value names of a property from PropertyValueAliases.txt: list of (short, long)."""
    res = []
    for line in open(fetch("PropertyValueAliases.txt"), encoding="utf-8"):
        line = line.split("#")[0].strip()
        if not line: continue
        f = [x.strip() for x in line.split(";")]
        if f[0] == prop: res.append((f[1], f[2]))
    return res
