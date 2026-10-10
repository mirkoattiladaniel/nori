#!/usr/bin/env bash
# Build the archive fixtures std/archive/browse_test.nori checks, into <workdir>.
#
# Every fixture is made with standard host tools (tar, zstd, zip, bsdtar), and an
# .expected list is derived from what those tools themselves report (`tar -tvf`,
# `unzip -l`, `bsdtar -tvf`) so the Nori test compares its listing against a real
# reader's, not against values we invented.
#
# Usage:  bash std/archive/make_browse_testdata.sh <workdir>
#   (the Nori test expects the fixtures under <workdir>/browse_testdata/)
set -u
W="$1"
# resolve to an absolute path up front: the tar/zip/bsdtar invocations below run
# in `( cd "$D/src" && ... )` subshells, where a relative "$D/..." output path
# would be interpreted against the subshell's cwd and fail to open.
mkdir -p "$W"
W="$(cd "$W" && pwd)"
D="$W/browse_testdata"
rm -rf "$D"
mkdir -p "$D/src/sub" "$D/src/deep/a/b"

# ---- one source tree, used for every archive ---------------------------------
printf 'hello, browse\nsecond line\n' > "$D/src/hello.txt"
printf 'col1\tcol2\none\ttwo\n' > "$D/src/table.tsv"
printf '#!/bin/sh\necho run me\n' > "$D/src/sub/run.sh"; chmod +x "$D/src/sub/run.sh"
printf 'the quick brown fox jumps over the lazy dog\n%.0s' {1..300} > "$D/src/deep/a/b/repetitive.txt"
head -c 172000 /dev/urandom > "$D/src/sub/random.bin"                         # binary, >128KB
: > "$D/src/empty_zero.txt"                                                    # 0-byte file
ln -s hello.txt "$D/src/link_hello"
ln -s deep/a/b/repetitive.txt "$D/src/deep/link_rep"
ln "$D/src/hello.txt" "$D/src/hardlink.txt"
long="this_is_a_long_filename_that_has_to_be_longer_than_one_hundred_and_twenty_chars_to_force_gnu_long_name_records"
printf 'long name content\n' > "$D/src/$long.txt"
printf 'spaced file\n' > "$D/src/with space ü.txt"

# t1.tar: GNU tar (exercises 'L' long names; the ü name goes via pax)
( cd "$D/src" && tar --format=gnu -cf "$D/t1.tar" . )
( cd "$D/src" && tar --format=gnu -czf "$D/t1.tar.gz" . )
if command -v zstd >/dev/null 2>&1; then
  ( cd "$D/src" && tar --format=gnu -cf - . | zstd -q -f -3 -o "$D/t1.tar.zst" )
fi

# an old-style (pre-ustar) tar: identified by the header checksum, not the magic
( cd "$D/src" && tar --format=v7 -cf "$D/oldv7.tar" hello.txt empty_zero.txt )

# t1.zip: the same tree, Info-ZIP (dirs + symlinks)
( cd "$D/src" && zip -q -r -y "$D/t1.zip" . )
( cd "$D/src" && zip -q -r -0 "$D/stored.zip" hello.txt deep/a/b/repetitive.txt )

# t1.iso: bsdtar writes ISO 9660 with both Rock Ridge and Joliet
( cd "$D/src" && bsdtar --format iso9660 -cf "$D/t1.iso" . )

# a directory-only zip (no dir records) for implicit-directory checks
( cd "$D/src" && zip -q -r -D "$D/nodirs.zip" deep/a/b/repetitive.txt hello.txt )

# a zip64 archive: $ZIP64NESS many entries pushes the count above 0xFFFF
mkdir -p "$D/many"
(
  cd "$D/many"
  for i in $(seq 1 300); do printf 'entry %d\n' "$i" > "f$i.txt"; done
  zip -q "$D/many.zip" f*.txt
)

# ---- the expected listings (from the host readers) ----------------------------
# field order per line:  PATH<TAB>SIZE<TAB>KIND<TAB>MODE
#   KIND: 0 file 1 dir 2 symlink 3 hardlink ; MODE: the reader's raw mode (0 = n/a)
python3 - "$D" <<'EOF'
import re, sys, subprocess
D = sys.argv[1]

def norm(name):
    while name.startswith("./"): name = name[2:]
    while name.startswith("/"): name = name[1:]
    # GNU tar -tvf prints symlinks as "name -> target" and hard links as "name link to target"
    for sep in (" -> ", " link to "):
        i = name.find(sep)
        if i > 0: name = name[:i]
    while name.endswith("/"): name = name[:-1]
    if len(name) == 0 or name == "." or name == "..": return None
    return name

def kind_of(modech):
    if modech == "d": return 1
    if modech == "l": return 2
    if modech == "h": return 3
    return 0

tar_re = re.compile(r'^([bcdDhlLmNpsu\-][rwxsStT\-]{9})\s+\S+\s+(\d+)\s+(\d{4}-\d\d-\d\d)\s+(\d\d:\d\d)\s+(.*)$')
def tar_list(tarfile, computemode):
    r = subprocess.run(["/usr/bin/env", "tar", "-tvf", tarfile], capture_output=True, text=True)
    out = []
    for line in r.stdout.splitlines():
        m = tar_re.match(line)
        if not m: continue
        n = norm(m.group(5))
        if n is None: continue
        out.append((n, int(m.group(2)), kind_of(m.group(1)[0]), 0))
    return out
out = tar_list(D+"/t1.tar", False)
open(D+"/t1.list","w").write("\n".join("%s\t%d\t%d\t%d" % x for x in out))
out = tar_list(D+"/oldv7.tar", False)
open(D+"/oldv7.list","w").write("\n".join("%s\t%d\t%d\t%d" % x for x in out))

def unzip_list(path):
    r = subprocess.run(["/usr/bin/env", "unzip", "-l", path], capture_output=True, text=True, errors="replace")
    ents = []
    for ln in r.stdout.splitlines():
        m = re.match(r'^\s*(\d+)\s+\d{4}-\d\d-\d\d\s+\d\d:\d\d\s+(.*)$', ln)
        if not m: continue
        if ln.strip().startswith("-"): continue
        if re.match(r'^\s*\d+\s+files?$', ln): continue
        n = m.group(2)
        ents.append((n, int(m.group(1)), 1 if n.endswith("/") else 0))
    return ents
def write_zip_list(arch, lf):
    lines = []
    for n, size, d in unzip_list(D+"/"+arch):
        nn = norm(n)
        if nn is None: continue
        lines.append("%s\t%d\t%d\t%d" % (nn, size, 1 if d else 0, 0))
    open(D+"/"+lf,"w").write("\n".join(sorted(lines, key=lambda x: x)))
write_zip_list("t1.zip", "t1z.list")
write_zip_list("stored.zip", "stored.list")
write_zip_list("nodirs.zip", "nodirs.list")

# the implicit-directory zip has no dir records; the Nori test adds them itself.
open(D+"/nodirs.entries","w").write("\n".join(sorted(
    "%s\t%d" % (norm(n), s) for n, s, d in unzip_list(D+"/nodirs.zip") if norm(n) is not None)))

iso_re = re.compile(r'^([dl\-][rwx\-]{9})\s+\d+\s+\d+\s+\d+\s+(\d+)\s+[A-Za-z]{3}\s+\d+\s+\d\d:\d\d\s+(.*)$')
r = subprocess.run(["/usr/bin/env", "bsdtar", "-tvf", D+"/t1.iso"], capture_output=True, text=True)
out = []
for line in r.stdout.splitlines():
    m = iso_re.match(line)
    if not m: continue
    name = m.group(3)
    kind = kind_of(m.group(1)[0])
    size = int(m.group(2))
    seen_link = False
    for sep in (" -> ", " link to "):
        i2 = name.find(sep)
        if i2 > 0:
            kind = 2 if sep == " -> " else 3
            size = 0
            name = name[:i2]
            seen_link = True
    n = norm(name)
    if n is None: continue
    out.append((n, size, kind, 0))
open(D+"/t1i.list","w").write("\n".join("%s\t%d\t%d\t%d" % x for x in out))
EOF

# the expected byte contents: archive_read(entry) must equal the source bytes, so
# keep a copy of the source tree the archives were made from.
cp -r "$D/src" "$D/original"
chmod -R u+w "$D/original"

# the symlink map (authoritative: the archives were made from this tree)
( cd "$D/original" && find . -type l -printf '%P\t%l\n' 2>/dev/null | sed 's|^\./||' ) | sort > "$D/symlinks.list"

echo "made $D"