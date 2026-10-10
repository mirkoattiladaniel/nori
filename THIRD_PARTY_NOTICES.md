# Third-party material

Nori's own source is licensed under either the Apache License 2.0 or the MIT license, at your option
(see `LICENSE-APACHE` and `LICENSE-MIT`). This repository also contains the files below, which come from
other projects and keep their own licenses.

## Fonts

Used by `std/font` and `std/nori_ui` as test and fallback fonts, in `std/font/assets/`. Most are small
subsets of the original fonts, reduced to the glyphs and tables the tests need.

| Files | Project | License |
|---|---|---|
| `NotoSans-subset.ttf`, `NotoSansArabic-subset.ttf`, `NotoNaskhArabic-subset.ttf`, `NotoNastaliqUrdu-subset.ttf`, `NotoSansBengali-subset.ttf`, `NotoSansDevanagari-subset.ttf`, `NotoSansHebrew-subset.ttf`, `NotoSansThai-subset.ttf`, `ColrTest.ttf` (a Noto Sans subset with a colour table added) | Noto fonts, https://github.com/notofonts/notofonts.github.io | SIL Open Font License 1.1 |
| `NotoSansCJK-subset.ttc`, `NotoSansCJKjp-subset.otf`, `NotoSansCJKsc-subset.otf` | Noto Sans CJK, https://github.com/notofonts/noto-cjk | SIL Open Font License 1.1 |
| `NotoColorEmoji-subset.ttf` | Noto Color Emoji, https://github.com/googlefonts/noto-emoji | SIL Open Font License 1.1 |
| `AdwaitaMono-subset.ttf` | Adwaita Fonts, https://gitlab.gnome.org/GNOME/adwaita-fonts | SIL Open Font License 1.1 |
| `DejaVuSans*.ttf`, `DejaVuSansMono.ttf`, `DejaVuSans.woff2` | DejaVu fonts, https://dejavu-fonts.github.io/ | Bitstream Vera license (text in `std/font/assets/DejaVu-LICENSE`) |

The text of the SIL Open Font License 1.1 is in `licenses/OFL-1.1.txt`. The fonts are redistributed
unmodified apart from subsetting, as the license allows, and are not sold on their own.

## GPU headers

`std/nori_ui/paint_gpu/webgpu.h` comes from the WebGPU native headers
(https://github.com/webgpu-native/webgpu-headers, BSD-3-Clause; the license is stated at the top of the
file). `std/nori_ui/paint_gpu/wgpu.h` comes from wgpu-native (https://github.com/gfx-rs/wgpu-native,
MIT or Apache-2.0; see the project for the exact terms).

## Wayland protocol files

The `*-protocol.c`, `*-client-protocol.h` and `*.xml` files in `std/window/` are the protocol descriptions
of wayland-protocols (https://gitlab.freedesktop.org/wayland/wayland-protocols) and the C code the
`wayland-scanner` tool generates from them. They carry their copyright and permission notices in their
headers (MIT-style).

## Unicode data

`std/font/data/ucd.bin` and the tables in `std/unicode` are generated from the Unicode Character Database,
and the `*Test.txt.zst` files in `std/font/testdata/` are Unicode's own conformance test files, compressed.
The Unicode Character Database is distributed under the Unicode License v3
(https://www.unicode.org/license.txt).
