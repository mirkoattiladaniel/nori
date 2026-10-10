# std/archive

```nori
import "std/archive" as archive
```

std/archive: gunzip (DEFLATE / RFC 1951 + gzip / RFC 1952) and tar extraction.
  import "std/archive" as ar
  ar::unpack("foo.tar.gz", "dir")   // gunzip + untar into dir (which must exist); 0 = ok
Decompression runs into a raw heap buffer (unsafe malloc/peek8) so LZ77 back-references and the tar
parser have O(1) random access. Handles .tar.gz and plain .tar (gzip magic 0x1f 0x8b is auto-detected).
### `fn inflate_err_text(code: Int) -> Str`

what an inflate error code means.

### `fn inflated_ptr(b: Inflated) -> Int`

the blob's heap pointer: read it with `peek8`, and hand it back with `inflated_free`.

### `fn inflated_len(b: Inflated) -> Int`

how many bytes the blob holds.

### `fn inflated_free(b: Inflated)`

release the blob. The buffer is raw memory, so nothing else will.

### `struct InflateResult`

an inflate's outcome: `ok`, or `err` saying what was wrong; `data` holds what was decoded (on an
error, the bytes before the fault) and must be freed with inflated_free either way.

### `fn inflate_raw_checked(data: Str, max_out: Int) -> InflateResult`

inflate a raw DEFLATE stream (RFC 1951), checked: any malformed stream is an error, never a trap
or a read outside the input. `max_out` > 0 caps the output (a stream that would inflate past it
is an error, which keeps a hostile stream from asking for gigabytes); 0 is no cap.

### `fn inflate_zlib_checked(data: Str, max_out: Int) -> InflateResult`

inflate a zlib stream (RFC 1950), checked: the 2-byte header (compression method 8, window, the
header check, no preset dictionary), the DEFLATE data, and the Adler-32 of the output.

### `fn gunzip_checked(data: Str, max_out: Int) -> InflateResult`

gunzip one gzip member (RFC 1952), checked: the header, the DEFLATE data, the CRC-32 and the
length in the trailer.

### `fn inflate_zlib(data: Str) -> Inflated`

inflate a zlib stream (RFC 1950: 2-byte header + DEFLATE + adler32), e.g. a PNG IDAT stream.

Safe on any input (a malformed stream stops where it goes wrong and the bytes decoded before
that are returned), but it does not say so, and it checks neither the header nor the Adler-32.
A caller that must know whether the data is whole uses inflate_zlib_checked.

### `fn unpack(apath: Str, dir: Str) -> Int`

unpack archive `apath` into directory `dir` (which must exist). Handles .tar.gz and plain .tar. 0 = ok.




std/archive: a Brotli decoder (RFC 7932), for WOFF2 web fonts (which Brotli-compress the
SFNT table data). Built in layers: bit reader + prefix codes, then meta-block
decode (literals/commands/distances + context modeling), then the static dictionary. WOFF2's table
directory + glyf/loca transform reversal live in std/font.

Brotli reads bits LSB-first; its prefix codes are canonical Huffman read low-bit-first, so a code's
bits are the reverse of the DEFLATE-style canonical code. We build a flat lookup table indexed by the
next `maxlen` bits (with the reversed code filled across all high-bit combinations): the standard
constant-time Brotli symbol decode.
### `fn brotli_decode(data: Str) -> Str`

decompress a raw Brotli stream to its bytes. A stream that needs a static dictionary word
is resolved against the embedded RFC 7932 dictionary.


std/archive: browsing an archive without unpacking it, one file at a time.

  import "std/archive" as ar
  let kind = ar::archive_kind("game.zip")                  // 4 = zip
  let list = ar::archive_list("game.zip")                  // every entry (dirs synthesized)
  let data = ar::archive_read("game.zip", "art/logo.png")  // one file's bytes
  let rc   = ar::archive_extract("game.iso", "assets", "out")  // one entry or subtree

A file explorer can treat `.zip`, `.tar`, `.tar.gz`, `.tar.zst` and `.iso`
as folders: list them, then drag a single file out without unpacking everything first.
That is the shape of this module. It reuses the module's decoders (`gunzip_checked`,
`zstd_decompress_checked`, `inflate_raw_checked` from the sibling files) but writes
its own parsers from the format specifications (IEEE 1003.1 ustar/pax + the GNU
`L`/`K` long-name records, PKWARE's APPNOTE.TXT for zip, ECMA-119 for ISO 9660, the
Joliet UCS-2 tree, SUSP/Rock-Ridge NM/PX/TF/SL).

The API:
  Entry { path, size, mtime, mode, kind, link }
    kind: 0 file, 1 directory, 2 symlink, 3 hard link, 4 other/unknown.
    path: `/`-separated, no leading `./` or `/`, no trailing slash on directories
    (a directory entry and a file entry never share a path).
    Directories that exist only implicitly (a tar holding `a/b/c.txt` with no `a/`
    record) are synthesized by archive_list, so a caller can walk the result as a
    tree: it is sorted, and a directory sorts before everything inside it.
  archive_kind(path) -> Int: sniffs by magic bytes, never the extension:
    1 tar   (ustar magic at 257, or a valid old-style header checksum)
    2 gzip  (1f 8b, normally a gzip-wrapped tar)
    3 zstd  (28 b5 2f fd, normally a zstd-wrapped tar)
    4 zip   (PK\3\4, or the empty-archive PK\5\6)
    5 iso9660 (CD001 at sector 16 + 1)
    -1 otherwise.
    A `.tar.gz` file sniffs as gzip: the gzip code means "compressed, wraps a tar",
    and archive_list/archive_read follow it.
  archive_list(path) -> Vec<Entry>: every entry; empty Vec on any error (missing or
    damaged archive, or decompression failure). Listing a zip or an ISO reads only
    the archive's directory records, never the file data.
  archive_read(path, entry) -> Str: exactly that entry's bytes, "" if the archive
    cannot be opened, the entry is missing, or the archive is damaged: the error
    convention the rest of std uses ("" on failure). For zip only the requested
    entry is decoded; no other entry's data is touched. A directory, symlink or
    hard-link entry has no byte content of its own: "" (the target is `link`).
  archive_extract(path, entry, dest) -> Int: materializes one entry into `dest`
    (dest must exist). A file is written to dest; a directory entry recreates the
    whole subtree under dest; a symlink/hard link is recreated when it stays inside
    dest. Returns: 0 ok · 1 refused (path traversal, absolute name, a link target
    escaping dest, or the file is not an archive) · 2 entry not found · 3 fs error.
    A refused path fails the whole call: one bad member, nothing written.
### `struct Entry`

one archive entry as a Files explorer would show it: `path` is `/`-separated
without a leading `./` or `/`; `kind` is 0 file · 1 directory · 2 symlink ·
3 hard link · 4 other; `link` is the target for kinds 2/3 ("" otherwise);
`mtime` is unix seconds; `mode` is the format's own mode (tar/zip: permission
bits; ISO: the Rock Ridge st_mode when present, else a default).

### `fn archive_kind(path: Str) -> Int`

the format of `path`, sniffed from magic bytes (see the module doc for codes).

### `fn archive_list(path: Str) -> Vec<Entry>`

all entries of `path` (implicit directories synthesized, sorted). Empty on error.

### `fn archive_read(path: Str, entry: Str) -> Str`

one entry's bytes ("" = missing / damaged / not a file). See the module doc.

### `fn archive_extract(path: Str, entry: Str, dest: Str) -> Int`

materialize one entry into `dest`. 0 ok; see the module doc for error codes.


std/archive: DEFLATE compression (RFC 1951) and the zlib wrapper (RFC 1950).

The mirror of this module's `inflate_zlib`. `deflate(data)` produces a raw DEFLATE stream and
`deflate_zlib(data)` wraps it in zlib's 2-byte header and Adler-32 trailer, which is the form a
PNG IDAT wants (see std/image::save_png).

How it compresses. LZ77 over a 32 KB window with a hash-chain match finder (3-byte hash, 128-deep
chains, zlib's lazy-match rule: a match is only taken when the next position cannot do better),
then Huffman coding. Every block is costed three ways (stored, fixed-Huffman, dynamic-Huffman)
in bits, and the cheapest one is emitted. That is what makes incompressible input safe: random
bytes cost more under either Huffman tree than they do raw, so they come out stored, and the
stream grows by 5 bytes per 64 KB instead of by a third.

Code lengths are capped at 15 bits (7 for the code-length alphabet) by halving the frequencies and
rebuilding until the tree fits, which is slower than the package-merge algorithm and much shorter.
Symbols are buffered 32768 at a time, so a long input is many blocks and each block gets a tree
fitted to its own statistics.
### `fn adler32(data: Str) -> Int`

Adler-32 of `data`, the checksum zlib streams carry in their trailer.

### `fn deflate(data: Str) -> Str`

compress `data` to a raw DEFLATE stream (RFC 1951), the inverse of this module's `inflate`.

The result is what a zlib or gzip payload holds with the wrapper stripped off; for a PNG IDAT or
anything else that wants the checksummed form, use `deflate_zlib`. Blocks are chosen per 32768
symbols between stored, fixed-Huffman and dynamic-Huffman by counting the bits each would take, so
incompressible input is stored rather than expanded.

### `fn deflate_zlib_stored(data: Str) -> Str`

wrap `data` in a zlib stream without compressing it: stored DEFLATE blocks of at most 65,535
bytes each, with the zlib header and Adler-32. Valid for anything that reads zlib, including a
PNG IDAT, and as fast as copying.

For when the bytes matter less than the time, such as short-lived screenshots written to a
directory that is deleted at the end, where compression was a large share of the cost.

### `fn gzip(data: Str) -> Str`

compress `data` to a gzip member (RFC 1952: 10-byte header + DEFLATE + CRC-32 + size). It is what
`gunzip_checked` reads, what a `.gz` file holds, and what HTTP's `Content-Encoding: gzip` means.
The header carries no name and no time (MTIME 0), so the same input always gives the same bytes.

### `fn deflate_zlib(data: Str) -> Str`

compress `data` to a zlib stream (RFC 1950: 2-byte header + DEFLATE + Adler-32), the inverse of
`inflate_zlib`, and the shape a PNG IDAT chunk holds.


std/archive: writing zip archives.

Reading a zip is not what this is for. The operation is `zip_add_stored`: take an archive that
already exists and hand back one with a file added, leaving every existing entry byte for byte as
it was. That is the shape the job usually has (an APK is an aapt2-produced zip that a build
must drop a native library into), and it keeps this small: entries already deflated
are copied, never recompressed, so nothing here needs a deflate encoder. Only the new entry is
written, and it is stored.

Stored is not a shortcut. Android's loader maps a native library straight out of the APK, which
it can only do when the bytes are uncompressed; a deflated one installs fine and then fails
at launch.
### `fn zip_add_stored(z: Str, name: Str, data: Str) -> Str`

Add `name` to the zip in `z` as an uncompressed entry holding `data`, returning the new archive
("" if `z` is not a zip). Existing entries keep their bytes, their compression and their order.

Offsets are what make this more than appending: every central-directory entry
records where its local header sits, so copying the records forward means rewriting each of those
numbers. An archive with stale offsets unzips on a forgiving tool and fails on a strict one.

### `fn zip_names(z: Str) -> Vec<Str>`

The names an archive holds, in central-directory order ("" entries never appear). Empty when `z`
is not a zip. Enough to check what a build produced without shelling out to a tool.


std/archive: a Zstandard decompressor (RFC 8878 / the zstd format specification).

The counterpart to this module's Brotli decoder, built in the same layers: bit readers first,
then the two entropy coders zstd uses (FSE and Huffman), then the format's own frame/block/sequence
structure. There is no compressor here: storing a block raw is format-legal and that is what the
one in-tree writer does; see `zstd_store` at the bottom, which exists only so round-trips can be
tested without the CLI.

Why there are three bit readers. zstd mixes conventions, and getting one of them backwards is the classic way
to produce a decoder that works on tiny inputs and dies on real ones:
  · frame/block/section headers are little-endian byte fields, read forward (`zrd_*`).
  · the FSE table description (`Normalized_Counts`) is a forward, LSB-first bitstream (`ZFwd`),
    the same convention DEFLATE and Brotli use.
  · every FSE and Huffman payload is a backward, MSB-first bitstream (`ZBwd`). It is read from the
    last byte of the section towards the first. The last byte is non-zero and its highest set bit
    is a padding marker that is skipped, which is what fixes the total bit count.
`ZBwd` therefore tracks `pos` = how many valid bits are still unread, and a k-bit read takes bits
[pos-k, pos-1] with bit pos-1 as the MSB. Running off the front is normal at the very end of a
stream (the tail is zero-padded); running off it early is corruption, and `bad` records it.

What is covered. Frame magic and the full frame header (descriptor, window descriptor, dictionary
ID, frame content size, single-segment); skippable frames; Raw, RLE and Compressed blocks; Raw, RLE,
Huffman and Treeless literals in both the 1-stream and 4-stream forms; FSE table descriptions and
the three predefined distributions; Predefined / RLE / FSE_Compressed / Repeat modes for each of the
three sequence symbol streams; sequence execution with the repeat-offset state; and XXH64 content
checksum verification (not merely skipping; see `zxx64`).

What is refused by name, rather than mis-decoded: dictionary-compressed frames (a non-zero
Dictionary_ID), and reserved/invalid encodings (block type 3, reserved header bits, a bad magic).
Long-distance matching and multi-threading are encoder-side and leave no trace in the format, so
there is nothing to refuse: streams produced with them decode normally here.

The repeat offsets are easy to get subtly wrong, so, explicitly: `r0,r1,r2` start
at 1,4,8 at the start of every frame and persist across blocks. An `Offset_Value` above 3 is a
literal offset of `value - 3` and pushes onto the list. A value of 1..3 selects a repeat, and which
one depends on whether this sequence's literal length is zero: the effective index is
`value - 1 + (literal_length == 0)`, and index 3 means "r0 minus one". See `zupdate_rep`.
### `struct ZstdResult`

a decode result: the bytes, or the reason they could not be produced.

### `fn zstd_decompress_checked(data: Str) -> ZstdResult`

decompress a Zstandard stream (RFC 8878), reporting why on failure.

The input may hold several frames back to back; their outputs are concatenated, and skippable
frames contribute nothing. When a frame carries a content checksum it is verified (XXH64), so a
silently corrupted payload is an error rather than wrong bytes. Dictionary-compressed frames are
refused by name.

### `fn zstd_decompress(data: Str) -> Str`

decompress a Zstandard stream, or return "" if it cannot be decoded. Use
`zstd_decompress_checked` when the reason matters: an empty result is also what a valid stream of
zero bytes produces.

### `fn zstd_store(data: Str) -> Str`

wrap `data` in a single Zstandard frame of Raw blocks. Not a compressor, just the format-legal
"store" path, so a round-trip can be exercised without the CLI. Blocks are 65535 bytes; the frame
carries a Frame_Content_Size and no checksum.


