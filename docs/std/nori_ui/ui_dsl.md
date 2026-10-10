# The `.ui` DSL

`.ui` is the language editor plugins are written in. A plugin does not draw; it returns a
string of `.ui` source describing what it wants on screen, and the host parses that source,
resolves a stylesheet over it, lays it out against the panel's rectangle, paints it, and
hit-tests it. The plugin sees the result only as an action string coming back through
`nori_plugin_event`.

The implementation lives in `std/nori_ui/layout` ([its reference](layout.md)). This page
describes what it accepts.

---

## 1. The contract

`nori_plugin_render(id, w, h)` returns `.ui` source. That is the whole plugin-side of
rendering — see [Plugin ABI](plugin_abi.md).

```nori
import "std/os" as os

pub fn nori_plugin_render(idp: Int, w: Float, h: Float) -> Int {
    return os::os_cstr("column { padding: 12 gap: 6 text { text: \"hi\" size_px: 13 } }")
}
```

The host then runs the pipeline:

1. **Parse** — `ui::layout::dsl_parse_document(src)` returns a `DslParseResult`. On failure
   `err` is `"LINE:COLUMN: message"`, with the parts also split out into `eline` / `ecol` /
   `emsg`, and `src_empty` set instead when the source was blank. A malformed document costs
   only its own panel: the host reports the error and draws it there.
2. **Cascade** — if the document carried a `stylesheet { }` block, `apply_stylesheet` resolves
   each node's `tag` and `classes` against it and writes the resolved properties onto the tree.
3. **Layout** — `doc_layout(doc, viewport)` produces a `ComputedOverlay`: a list of
   `PaintPrimitive`s, a list of `HitRegion`s, a list of `SurfaceRegion`s, a list of
   `ScrollRegionMeta`s, and a separate popup layer that is drawn last and clipped by nothing.
4. **Hit** — `overlay_hit_test(overlay, point)` walks `hits` **backwards** and returns the
   first region containing the point, so the last-emitted region wins. Its `on_click` string
   is what the host hands back to `nori_plugin_event`.

The plugin owns none of that. It owns the string.

---

## 2. The shape of a document

A document is either wrapped in `document { … }` or written as bare top-level fields. Both
forms accept the same three things, in any order:

- `style: "name"` — a name the host may use to find an external stylesheet. The parser only
  records it; nothing in `std/nori_ui` resolves it.
- `stylesheet { … }` — an inline stylesheet (§6). A second one is
  `duplicate stylesheet block`.
- exactly one **root node**, written either as `KIND { … }` directly or as `tree KIND { … }`.

```ui
document {
  style: "inspector.style"
  stylesheet {
    tag text { size_px: 12 }
    class title { size_px: 16 bold: true }
  }
  column {
    padding: 8
    gap: 6
    text { classes: [title] text: "Inspector" }
    text { text: "no selection" }
  }
}
```

Without the wrapper, and using `tree`:

```ui
style: "inspector.style"
tree column {
  padding: 8
  text { text: "Inspector" }
}
```

A second root has **three** spellings, and which you get depends on the form you wrote:

| form | message |
|---|---|
| top-level `KIND { }` twice | `only one root node allowed` |
| top-level, where the second root is `tree KIND` | `only one root allowed` |
| anything inside `document { … }` | `only one root in document` |

Inside the wrapper there is one message for both cases, because there it is the *wrapper* that has
one root.

The duplicate-root text is not the only message that depends on the form: inside the wrapper the
subject is the *document*, outside it the *top level*, and the name is double-quoted in each:

| situation | bare top level | inside `document { … }` |
|---|---|---|
| `name: value` with an unknown `name` | `unknown top-level field "name"` | `unknown document field "name"` |
| a name followed by neither `:` nor `{` | `unexpected after "name"` | `bad document item "name"` |
| `tree:` used as a *field* | (not reachable — `tree` is only a keyword here) | ``use `tree column {` form: tree keyword expects a block`` |
| no root at all | `document needs a root column/panel/row/text/button/rect/surface block` | `document needs tree column/panel { ... } or root block` |
| content after the document | `trailing content after root` | `trailing content after document` |

Inside the wrapper, `tree` is also checked *before* the `{` branch, the reverse of the
top-level order. Bare top level, `trailing content after root` is largely theoretical: the
field loop only stops at end of input, so a stray *word* after the root is consumed as a field
name first and reports `unexpected after "junk"` instead. After a `document { … }` wrapper there
is a real tail to complain about, and `trailing content after document` is what you get.

An empty source is a distinct outcome: `ok = false`, `src_empty = true`,
`err = "empty overlay document"`.

`dsl_parse_stylesheet(src)` is the second entry point: it parses a standalone `.style` file,
which is `tag` / `class` rules at the top level with no surrounding block. Its empty case is
`empty overlay stylesheet`.

---

## 3. Lexical syntax

An element is a name followed by a brace-delimited body. Inside a body, a name followed by
`:` is an attribute, and a name followed by `{` is a child element. Which of the two a given
kind allows is per-kind, and strict — several kinds (§5) accept only attributes, and the
error for a nested block there is the missing-colon error, not a "no children" message.

```ui
column {
  gap: 6            // attribute
  text {            // child element
    text: "hello"
  }
}
```

Attributes are separated by nothing but whitespace. Commas are **not** whitespace: they are
array element separators and nowhere else.

### Comments

Three forms, all skipped anywhere whitespace is:

```ui
column {
  // line comment
  /* block
     comment */
  # hash comment
  gap: 4
}
```

A `/*` block that is never closed simply ends at EOF; that is not an error.

**`#` is only a comment when the next byte is not a hex digit.** `# note` is a comment;
`#3 items left` is a colour literal followed by garbage, and reports
`expected identifier`. If a hash comment might start with `0`–`9` or `a`–`f`, use `//`.

The scanner also silently eats a UTF-8 BOM, a non-breaking space, and the zero-width and
bidi marks in `U+200B`–`U+200F`, because editors put them in real files.

### Identifiers

An identifier starts with a letter or `_` and continues with letters, digits, `_`, **and
`/`**. The slash is deliberate: it is what lets an unquoted action lex as a single token.

```ui
column { on_click: p/go }
```

`-` is *not* an identifier character, so an unquoted `line-through` lexes as `line`. The
quoted `"line-through"` is accepted where that value is.

### Values

| form | example | notes |
|---|---|---|
| string | `"hello"` | escapes `\\` `\"` `\n` `\r` `\t`; any other `\x` yields `x` |
| number | `-1.5`, `1e3`, `42` | sign, digits, optional fraction, optional exponent |
| bool | `true`, `false` | |
| identifier | `center`, `p/go` | a bare word; also how bare `#hex` colours arrive |
| array | `[1, 2, 3]`, `["a", b]` | commas **required**; `[a b]` is `` expected `,` or `]` in array `` |

Where a value is documented as *string* below, most kinds accept a bare identifier as well;
the exceptions are called out (`button`'s `label`, `image`'s `path`).

Where a value is documented as *bool*, most kinds also accept a number and read it as
`n != 0.0`. `button`'s `fill_w` / `label_bold` / `label_italic`, `slider` and `dropdown`'s
`fill_w`, and `text`'s `bold` / `italic` / `wrap_text` / `wrap_hard_break` are **strict** and
reject `1`.

### Colours

Two spellings, interchangeable everywhere a colour is taken:

```ui
column {
  rect { color: #f80 }
  rect { color: #ff8800 }
  rect { color: #ff8800c0 }
  rect { color: [255, 136, 0, 192] }
}
```

`#RGB` expands each digit to a pair (`f` → `ff`). `#RRGGBB` implies alpha `255`. The array
form must have exactly four numeric elements, each `0..=255`. A hex literal may also be
quoted (`"#ff8800"`); it is recognised by the leading `#`, not by the token type.

### Lengths

A length is a number with an optional unit suffix read straight off the cursor:

| unit | meaning |
|---|---|
| *(none)* | pixels |
| `%` | percent of the parent's extent on that axis |
| `vw` | percent of the viewport width |
| `vh` | percent of the viewport height |
| `ui` | UI-scale units; the bare word `ui` alone is `1ui` |

The suffix must not be followed by a letter, digit or `_`, so `40vhx` is `40` px followed by
the identifier `vhx`.

`fill` is not a length. Writing it where a length is expected reports
``w: use fill_w / fill_h instead of `fill` ``. The one exception is `w:` / `h:` on `rect`,
`surface` and `image`, where the **bare** ident `fill` sets `fill_w` / `fill_h` (§5.4); a
quoted `"fill"` there still takes the error path.

Only some attributes take lengths at all. `spacer`'s `w`/`h`, `divider`'s `h`, `rect`,
`surface` and `image`'s `w`/`h` do. Everything else spelled `height`, `h`, `min_height`,
`min_width`, `max_width` or `track_h` is a plain pixel number, and a suffix there is a parse
error. **`w:` and `h:` on the containers and on `text` are the awkward case**: they accept a
length, but only in quoted form —

```ui
column { w: "50%" h: "2ui" }
```

— because the unquoted `column { w: 50% }` leaves the `%` in the stream and reports
`expected identifier`. See §11.

---

## 4. What the parser is strict about

There is no shared attribute table. Every element name routes to a parser that owns its own
accepted set, its own value strictness and its own error text. That is deliberate. The practical
consequence for a plugin author is that **an attribute working on one kind tells you nothing
about another kind**:

```ui
column { padding: 4 }
```

is fine, and `button { padding: 4 }` is `unknown button field "padding"`. `rect { bg: #333 }`
is `unknown rect field "bg"` — a rect is painted with `color`, not `bg`. `slider { padding: 4 }`
is `unknown slider field "padding"`.

Unknown attributes are always errors, in one spelling: `unknown KIND field "NAME"`, for every
node kind and for `span` too. The quotes are part of the message.

Required attributes are checked at the **closing brace**, so the reported line and column are
the ones just past the block, not the block's start.

| kind | requirement | message |
|---|---|---|
| `text` | `text:` or at least one `span { }` | `text node requires text: and/or span { … } children` |
| `button` | `label:` | `button requires label:` |
| `button` | `on_click:` | `button requires on_click:` |
| `checkbox` | `id:` | `checkbox requires id:` |
| `slider` | `id:` | `slider requires id:` |
| `dropdown` | `id:` | `dropdown requires id:` |
| `number` / `drag_value` | `id:` | `number requires id:` |
| `text_input` | `id:` | `text_input requires id:` |
| `text_area` | `id:` | `text_area requires id:` |
| `picker` | `id:` | `picker requires id:` |
| `image` | `path:` | `image requires path` — no trailing colon, unlike the rest. |

The missing-colon message also differs by kind, in three flavours. `column`, `row`,
`scroll_*` and `text` (the kinds that take children) say
`` expected `:` or `{` after "name" ``. `spacer`, `button`, `rect`, `surface`, `divider`,
`slider`, `dropdown`, `image` and `span` say `` expected `:` in KIND node ``. `checkbox`, `number`,
`text_input`, `text_area` and `picker` say `expected ':', found '"'` — the offending
character, quoted.

---

## 5. The node kinds

Seventeen kinds, reached by twenty-one element names. `panel` is `column`, `scroll` is
`scroll_column`, `drag_value` is `number`, and `divider` is sugar for a `rect`. Error text
always uses the canonical name, so `panel { bogus: 1 }` reports
`unknown column field "bogus"`.

Twelve attributes are accepted on the eight core kinds and nowhere else. They are
`tag`, `position`, `z_index`, `top`, `right`, `bottom`, `left`, `overflow_x`, `overflow_y`,
`line_height`, `w` and `h`, and a `stylesheet { }` rule can set them too. Below they are referred to as **the extension tail**, and the kinds that take it are
`column`/`panel`, `row`, `scroll_column`/`scroll`, `scroll_row`, `text`, `button`, `rect` and
`surface`.

### 5.0 Out-of-flow children and `z_index`

`position: absolute` takes a child out of its parent's flow and places it against the parent's
**padded** rect using whichever of `top` / `right` / `bottom` / `left` are set, at its natural
size; a missing side means the min edge. The parent lays its in-flow children out as if the
absolute ones were not there, and then paints the absolute ones — so an absolute child always
draws over its in-flow siblings, whatever its `z_index`.

**Among themselves, absolute siblings paint in ascending `z_index`**, and the sort is *stable*:
equal `z_index` keeps document order, and an unset `z_index` is `0`. A document that never writes
`z_index` therefore lays out exactly as it would have without the sort.

```ui
column {
  rect { position: absolute z_index: 5 w: 40 h: 40 color: #c33 }   // painted second
  rect { position: absolute z_index: 1 w: 60 h: 60 color: #333 }   // painted first
}
```

`z_index` orders **within the normal layer only**. It cannot lift a node out of an ancestor's
scroll or `overflow: hidden` clip, and it cannot beat the popup layer, which is appended after the
whole tree and clipped by nothing (§1, §8). Content that must be above everything belongs
there, and only `dropdown` and the colour `picker` can put it there.

### 5.1 `column` / `panel` and `row`

A flex container. Draws its `bg` (rounded by `radius`), then its border, then lays its
children out along the main axis with `gap` between them, inside `padding`.

| attribute | aliases | value | default |
|---|---|---|---|
| `gap` | | number (strict) | `0` |
| `padding` | `pad` | number (strict) | `0` |
| `padding_x` | `pad_x` | number (strict) | falls back to `padding` |
| `padding_y` | `pad_y` | number (strict) | falls back to `padding` |
| `cross_align` | `align_items` | `stretch` \| `start` \| `center` \| `end` | `stretch` |
| `main_align` | `justify_content` | `start` \| `center` \| `end` \| `space_between` | `start` |
| `bg` | | colour | unset |
| `radius` | | number (strict) | `0` |
| `border_color` | | colour | unset |
| `border_w` | `border_width` | number (strict) | `0` |
| `classes` | | array of string/ident | `[]` |
| `class` | | string/ident — *replaces* `classes` | |
| `on_click` | | string/ident | none |
| `min_height` | | number (strict) | `0` |
| `fill_w` | `fill` | bool | `false` |
| `fill_h` | | bool | `false` |
| `flex_weight` | `flex` | number (strict) | unset |
| `flex_shrink` | | number (strict) | unset |
| `bind_visible`, `bind_disabled` | | string/ident | none |
| `flex_wrap` | `wrap` | bool, or the word `wrap` | **`row` only** |

Plus the extension tail. There is no `id` and no `align_self` on either kind — both are
hardcoded absent. `flex_wrap` exists only on `row`; on a column it is
`unknown column field "flex_wrap"`.

A container with a non-zero `on_click` gets a hit region covering its whole rect, emitted
*before* its children, so a child's own hit wins.

`overflow_y` on a `column` (or `overflow_x` on a `row`) set to `auto` or `scroll` turns it
into a scroll container; `hidden` clips children to the padded bounds without one.

### 5.2 `scroll_column` / `scroll` and `scroll_row`

The same container, with a viewport. Children are flowed at their natural size from an origin
shifted by the host's scroll offset and clipped to the padded bounds, and a `ScrollRegionMeta`
is reported so the host can clamp the offset and draw a bar. `id` names the region; without
one the region gets a synthetic `_scroll_N`, which the host cannot then feed an offset back to.

Its table is the container table minus `on_click`, `main_align`, `flex_weight`, `flex_shrink`
and the `fill` alias, plus `id`, plus the extension tail. Defaults differ:
`fill_w` is `true` on both, `fill_h` is `true` on `scroll_column` and `false` on `scroll_row`.

The `min_*` asymmetry is real and is not a bug: **`scroll_column` takes `min_height` and
`scroll_row` takes `min_width`**, never the other way round.

```ui
scroll_column {
  id: "hierarchy.tree"
  fill_w: true
  fill_h: true
  padding_x: 4
  gap: 2
  min_height: 40
  text { text: "row 1" }
  text { text: "row 2" }
}
```

### 5.3 `spacer`

Two attributes, and no others — not `classes`, not `class`, not `tag`, not `id`, not the
extension tail.

| attribute | value | default |
|---|---|---|
| `w` | length | unset |
| `h` | length | unset |

An unset extent measures **8 px** at layout, not 0. Spacers are skipped entirely inside a
scroll container's flow.

**A spacer cannot be reached individually from a stylesheet, and that is deliberate.**
`tag spacer { }` matches every spacer in the document and there is no narrower selector, because
`classes` / `class` / `tag` are not attributes here. A spacer is only
`{ h, w }` — there is no field a class could resolve onto and no paint step that would read one —
so accepting `classes:` would store a list nothing could ever consult. When one spacer needs a
different size, give it that size directly (`spacer { h: 16 }`); a stylesheet cannot set `w`/`h`
anyway, since neither is a style property (§6).

### 5.4 `rect` and `surface`

`rect` is a filled rectangle. It draws `color` (rounded by `radius`) and nothing else — it has
no `bg`, no border, no `id`, no `on_click`, no `bind_*` and no `flex_*`.

| attribute | aliases | value |
|---|---|---|
| `w`, `h` | | length, **or the bare word `fill`** |
| `fill_w` | `fill` | bool |
| `fill_h` | | bool |
| `color` | | colour |
| `radius` | | number (strict) |
| `classes` / `class` | | array / one name |
| `align_self` | | `stretch` \| `start` \| `center` \| `end` |

Plus the extension tail.

`w: fill` on `rect`, `surface` and `image` does **not** set a length. The parser peeks: a
leading `-` or digit takes the length path, and the bare ident `fill` sets `fill_w` (or
`fill_h` for `h:`) instead. `w: "fill"` is quoted, so it is not the bare ident, and errors.

`surface` is `rect`'s set plus `id`, `on_click`, `flex_weight`/`flex` and `flex_shrink` —
still no `bg` and no border. It draws nothing at all; see §9.

**Neither takes `bind_visible` or `bind_disabled`**, and that is not an asymmetry between the two:
`rect { bind_visible: v }` is `unknown rect field "bind_visible"` and
`surface { bind_visible: v }` is `unknown surface field "bind_visible"`. Neither kind has a bind at all,
so accepting either would store something nothing reads. The
line runs between the *leaves that only draw* — `rect`, `surface`, `spacer`, `divider`, `image` —
and everything else: the containers, `text`, `button` and all seven widget/input kinds do take
both binds. Wrap a conditionally-shown rect in a bound container:

```ui
column { bind_visible: has_sel  rect { fill_w: true h: 1 color: #444 } }
```

### 5.5 `divider`

Sugar. It produces a `rect` pre-seeded with `fill_w: true`, `h: 1` and the class `"divider"`.

| attribute | value |
|---|---|
| `h` | length |
| `color` | colour |
| `radius` | number |
| `classes` | array — **appends** to the implicit `divider` class |

There is no `id`, no singular `class`, no `w`, no extension tail, no `bind_visible` /
`bind_disabled` (§5.4 — `divider { bind_visible: v }` is `unknown divider field "bind_visible"`,
so a conditionally-shown separator wraps in a bound `column`), and no required attribute.
The appending `classes` is the one place in the DSL where `classes:` does not replace, so
`divider { classes: [rule] }` carries both `divider` and `rule` into the cascade.

```ui
column {
  text { text: "above" }
  divider { classes: [section_rule] h: 1 }
  text { text: "below" }
}
```

### 5.6 `text`

A text run. Draws its shadow copies first, then the glyphs, then any decoration rules; the
box is left-aligned and vertically centred in its rect unless an anchor says otherwise.

| attribute | aliases | value | notes |
|---|---|---|---|
| `text` | `label` | string or ident | one of this or a `span` is required |
| `id` | | string/ident | |
| `size_px` | `font_size` | number (strict) | default `14` |
| `color` | `text_color` | colour | default white |
| `anchor` | | `[x, y]`, two numbers | |
| `text_align` | `horizontal_align`, `justify_content` | `start`/`left`, `center`/`middle`, `end`/`right` | writes anchor *x* |
| `vertical_align` | `align_items` | `start`/`top`, `center`/`middle`, `end`/`bottom` | writes anchor *y* |
| `bold`, `italic` | | bool (strict) | |
| `line_height_mult` | | number (strict) | default `1.2` |
| `max_width` | | number (strict) | |
| `min_height` | | number (strict) | |
| `letter_spacing`, `word_spacing` | | number (strict) | px; see §5.6 |
| `text_transform` | `transform` | `none`\|`uppercase`\|`lowercase`\|`capitalize` | |
| `text_overflow` | `overflow` | `clip` \| `ellipsis` | |
| `line_clamp` | | number, floored at `1` | |
| `nowrap` | | bool — `true` disables wrap, `false` is a **no-op** | |
| `wrap_text` | | bool (strict) | |
| `wrap` | | bool (lenient) — a Nori extension | |
| `wrap_hard_break` | | bool (strict) | |
| `align_self` | | cross-align word | |
| `classes` / `class` | | array / one name | |
| `height` | | length | Nori extension |
| `flex_weight` / `flex`, `flex_shrink` | | number | Nori extensions |
| `tab_index` | `focus_order` | number | Nori extension |
| `bind_visible`, `bind_disabled` | | string/ident | Nori extensions |

Note `justify_content` and `align_items` mean *anchor components* on `text`, not flex
alignment. Note also that `line_height_mult` is the attribute here, while the extension tail's
`line_height` sets the same field — both work on `text`, only `line_height` works on the other
core kinds.

The outline, gradient, shadow and decoration tail is real and painted:

| attribute | aliases |
|---|---|
| `text_outline_color` | `outline_color`, `stroke_color` |
| `text_outline_width_px` | `outline_width`, `stroke_width` |
| `text_gradient_from`, `text_gradient_to` | |
| `text_gradient_dir` | `text_gradient_direction` — `horizontal`/`x`/`right`/`to_right`, or `vertical`/`y`/`down`/`to_bottom` |
| `shadow_color` | `text_shadow_color` |
| `shadow_blur`, `shadow_spread`, `shadow_offset_x`, `shadow_offset_y` | each with a `text_shadow_*` alias |
| `text_decoration` | `decoration` — `none`\|`underline`\|`strikethrough`\|`line_through`\|`"line-through"`\|`overline` |
| `text_decoration_color` | `decoration_color` |
| `text_decoration_width_px` | `decoration_width`, `decoration_thickness` |

Each group is gated on one member. The outline draws only when `text_outline_color` is set —
a lone `text_outline_width_px` paints nothing. The gradient needs **both** endpoints. The
shadow needs `shadow_color`.

**`letter_spacing` and `word_spacing`** are px, and they are measured *and* painted. The rule:

```
line width = glyph advances + (glyphs - 1) * letter_spacing + (words - 1) * word_spacing
```

so letter spacing sits in the gaps *between* glyphs — a one-character line gets none, and there is
no trailing gap after the last glyph — and word spacing counts **word gaps**, i.e.
`split_whitespace().count() - 1`: a run of two spaces is one gap, and leading or trailing
whitespace is no gap at all. Both are clamped at `0`, so a negative value is a no-op rather than a
squeeze.

Because the width changes, so does everything derived from it: where the text wraps, where an
`ellipsis` cuts it, how wide an underline runs, and where the shadow copies fall. The same offsets
table drives both painters (`layout::text_spacing_offsets_cps`), so a string is drawn at exactly
the width it was measured at on the CPU and the GPU alike. A rich-text `text` node spaces each of
its segments with the node's values; a `span` cannot override them (see below).

`span { }` is `text`'s only legal child; anything else is
`unknown child node "x" in text (expected span { … })`.

A span has its own attribute table, at the same per-kind fidelity as every node kind:

| attribute | aliases | value | notes |
|---|---|---|---|
| `text` | | string or ident | the run's content |
| `classes` | | array of string/ident | resolved against `class` rules only (§6) |
| `class` | | string/ident — *replaces* `classes` | |
| `size_px` | `font_size` | number | overrides the parent `text`'s |
| `color` | `text_color` | colour | overrides the parent `text`'s |
| `bold`, `italic` | | bool | |
| `text_transform` | `transform` | `none`\|`uppercase`\|`lowercase`\|`capitalize` | a Nori extension |
| `text_outline_color` | `outline_color`, `stroke_color` | colour | |
| `text_outline_width_px` | `outline_width`, `stroke_width` | number | clamped to `0.5..16` at paint |

Anything else is `unknown span field "NAME"`, and a missing colon is
`` expected `:` in span node ``.

A span's outline **replaces** the parent `text`'s for that run; a span with no outline of its own
inherits the node's.

Two attributes you might expect on `span` are **not** accepted, for the reason `text`
rejects `font_family` (§11): nothing downstream could read them.

- `font_family` — `unknown span field "font_family"`. No font-family field on the paint
  primitive, and neither painter can select a face.
- `letter_spacing` — `unknown span field "letter_spacing"`. Spacing is a property of the whole
  glyph run here: the `text` node's `letter_spacing` / `word_spacing` (§5.6) space every segment of
  a rich-text flow, and a *per-span* override would need a tracking field on `TextSpanRun` that the
  segment layout does not carry. Set it on the `text` node instead.

```ui
text {
  size_px: 13
  span { text: "warn: " color: #e0a030 bold: true }
  span { text: "unsaved changes" classes: [dim] }
}
```

### 5.7 `button`

Draws a rounded `bg` rect and a centred label, and always emits a hit region over its whole
rect. Height defaults to 48 px.

| attribute | aliases | value |
|---|---|---|
| `label` | | **quoted string only** — required |
| `on_click` | | string/ident — required |
| `id` | `node_id` | string/ident |
| `bg` | | colour |
| `text_color` | `color` | colour |
| `radius` | | number (strict) |
| `height` | | number (strict), pixels |
| `label_size_px` | `size_px`, `text_size` | number (strict) |
| `fill_w` | | **bool only** |
| `flex_weight` | `flex` | number (strict) |
| `flex_shrink` | | number (strict) |
| `label_bold`, `label_italic` | | **bool only** |
| `label_line_height_mult` | | number (strict) |
| `align_self` | | cross-align word |
| `tab_index` | `focus_order` | number |
| `classes` / `class` | | array / one name |
| `bind_visible`, `bind_disabled` | | string/ident |

Plus the extension tail.

**`button` rejects `text:`.** The label attribute is `label`, and it must be a quoted string —
`label: Save` is `label must be a quoted string`. There is no `padding`, no `gap`, no
`border_*`, no `min_height` and no typography beyond the four `label_*` attributes.

```ui
button { label: "Save" on_click: "file/save" }
```

### 5.8 `checkbox`

A box, a tick when checked, and a label to its right. `fill_w` defaults `true`, height 28.
`id` is required.

`id`, `label`, `checked`, `bind`, `bind_visible`, `bind_disabled`, `classes`, `class`,
`height`/`h` (pixels), `fill_w`, `align_self`, `tab_index`/`focus_order`. No extension tail,
no `color`, no `bg` — colours come from the stylesheet.

### 5.9 `slider`

A track with a filled portion and a knob. `fill_w` defaults `true`, height 32, `id` required.
`value` is a plain number and is **not clamped at parse time** — clamping to `0..=1` happens
at paint.

`id`, `value`, `height`/`h`, `track_h`, `knob_r`, `track_color`, `fill_color`, `knob_color`,
`bind`, `bind_visible`, `bind_disabled`, `classes`, `class`, `fill_w` (strict bool),
`align_self`.

`slider` has **no `label`** and **no `tab_index`** — it is the one interactive kind that is
deliberately not focusable.

### 5.10 `dropdown`

A trigger showing `options[selected]` plus a `▾`/`▴` arrow. When `open` is true the option
list is painted into the popup layer, below the trigger, or flipped above it when it would
run off the bottom of the viewport. `fill_w` defaults `true`, height 38, `item_height` 34,
`id` required.

`id`, `options` (array of strings/idents), `selected`, `open`, `height`/`h`, `item_height`,
`bind`, `bind_visible`, `bind_disabled`, `classes`, `class`, `fill_w` (strict bool),
`align_self`, `tab_index`/`focus_order`. There is no `label`, no `bg` and no `color`.

`selected` is truncated to a whole number: a negative value silently becomes `0`, and an
out-of-range one renders an empty trigger label rather than erroring.

```ui
dropdown {
  id: "hier.mode"
  options: ["Scene", "Prefabs", "Layers"]
  selected: 0
}
```

### 5.11 `number` / `drag_value`

One kind under two names. **Every message says `number`**, whichever name you wrote:
`drag_value { bogus: 1 }` reports `unknown number field "bogus"`.

A label on the left and a value field on the right, showing `value` at `precision` decimals
with `suffix` appended. `fill_w` defaults `true`, height 28, `id` required.

| attribute | aliases | notes |
|---|---|---|
| `id`, `label` | | |
| `value` | | number |
| `min` | `lo` | default `0.0` |
| `max` | `hi` | default `1.0` |
| `step` | | number; when unset the drag step is `fabs(hi - lo) / 200` |
| `precision` | | clamped to `0..=6` and rounded — the only real clamp in the family |
| `suffix` | | string/ident |
| `classes`, `class`, `bind`, `bind_visible`, `bind_disabled`, `height`/`h`, `fill_w`, `align_self`, `tab_index`/`focus_order` | | shared with the other input kinds |

`min` and `max` are not validated against each other at parse time; the paint pass orders
them with `fmin`/`fmax` before clamping the value.

### 5.12 `text_input` and `picker`

Field for field identical — a label column (34% of the width, clamped to 80–150 px), an 8 px
gap, then a background rect with the value text in it. Only the error prefix and the event
differ. `fill_w` defaults `true`, height 28, `id` required.

`id`, `label`, `value`, plus the shared input attributes listed above.

A `picker` with an empty `value` displays the literal `Browse...`. A `picker` whose `value`
parses as a `#RGB`/`#RRGGBB`/`#RRGGBBAA` colour additionally draws a swatch at the right end
of the field, and — while it carries the runtime `focused` class — opens a full HSV colour
popup in the popup layer.

```ui
column {
  gap: 6
  text_input { id: "obj.name" label: "Name" value: "Cube" }
  picker { id: "mat.albedo" label: "Albedo" value: "#c04020" }
}
```

### 5.13 `text_area`

A multi-line text box you can type in. `fill_w` defaults `true`; its **height is intrinsic**
when unset (three rows minimum, one row when `max_rows: 1` says so), unlike the other input
kinds' 28. `id` is required.

`id`, `value`, `fill_h`, `read_only`, `tab_size` (floored at 1), `hard_tabs`, `word_wrap`
(default `true`), `size_px`, `max_rows`, and the three caller-owned state fields `caret`,
`sel_anchor` and `scroll_y`, plus the shared input attributes. **There is no `label`.**

The state fields are the same bargain `code_editor` makes: the widget draws a caret and a
selection and remembers neither between frames, so the host reads the `text_area_edit:` action,
runs its own editor over `MultilineEditState`, and feeds the offsets back. `area_edit.nori` is
that editor — `area_key`, `area_pointer`, `area_attrs`, `area_height` — and a panel should use
it rather than writing the key map again. `area_caret_at` is the exact inverse of the paint, so
a press lands on the character under the pointer.

`max_rows` caps the height it **grows** to: past it the box keeps its height and scrolls its own
content, which is what makes this usable as an input bar rather than something that pushes the
rest of a panel off the screen. Unset, it grows without limit, which is what an editor filling a
pane wants.

```ui
text_area { id: "prompt" max_rows: 8 size_px: 14 caret: 12 sel_anchor: 12 scroll_y: 0 }
```

### 5.14 `image`

Draws a PNG. `fill_w` and `fill_h` both default `false` and `w`/`h` default to zero, so an
image with no size given occupies nothing.

`path` (**quoted string only**, required — there is no `src` alias), `w`, `h` (length, or the
bare `fill`), `fill_w`/`fill`, `fill_h`, `classes`, `class`, `align_self`. Nothing else —
notably not `flex_weight`, `flex_shrink` or `bind_visible`.

`path` is resolved by `resolve_image_png_path` against two host-set roots
(`ui_media_set_resolve_roots(project_root, save_dir)`):

1. **Sanitize.** Trim, then reject the path outright if it is empty, contains a backslash,
   contains `..`, starts with `/`, or has a drive letter (`:` as the second byte).
2. **Thumb lookup.** Only when the path contains no `/`: probe `save_dir/thumbs/NAME` then
   `project_root/thumbs/NAME`. The first existing file wins, whatever its extension.
3. **Extension.** From here on, **PNG only**, case-insensitively. Anything else resolves to
   nothing.
4. Strip leading `./` repeatedly.
5. Probe `project_root/PATH`.

**A path that does not resolve draws nothing, silently** — no placeholder, no border, no
error. A typo in `path:` looks like an empty box, not like a bug.

---

## 6. Stylesheets

A document's look — colours, sizes, spacing, type — comes from stylesheet rules: `tag NAME { }`
for every node of a kind, `class NAME { }` for every node listing the class. They are written in a
`stylesheet { … }` block inside the document, or in a standalone `.style` file. The rules, the full
property set and how they resolve are [the `.style` language](style_dsl.md).

```ui
document {
  stylesheet {
    tag button { radius: 5 padding: 8 bg: [44, 44, 52, 255] hover_bg: [56, 56, 66, 255] }
    class title { size_px: 16 bold: true }
  }
  column {
    text { classes: [title] text: "Materials" }
    button { label: "Apply" on_click: "mat/apply" }
  }
}
```

---

## 7. Templates

`.ui` sources are usually built by substitution rather than string concatenation:
`render_ui_template_json(template, env)` runs a pre-pass over the source and returns the text
to parse. The environment is a `Json` value, and every key is a **dotted path** walked
through the tree.

Three forms:

| form | meaning |
|---|---|
| `{{path}}` | the value, escaped for a quoted-string context |
| `{{raw:path}}` | the value inserted verbatim |
| `{{#foreach ITEM in path}} … {{/foreach}}` | repeat the body once per array element, with `ITEM` bound |

The distinction between the two substitution forms is what goes in the output. `{{path}}`
escapes `\`, `"`, newline, CR and tab, so it is what you use *inside* a quoted attribute
value. `{{raw:path}}` is what you use for a bare number, a bool, or an array literal.

A string value renders escaped by `{{}}` and bare by `{{raw:}}`. A number renders as its
shortest round-tripping decimal (`13`, `0.1`). A bool renders `true`/`false`. `null` renders
as the empty string. **An array or object renders as its JSON text in both forms** — never
escaped — which is exactly why `options: {{raw:opts}}` works: JSON's `["a","b"]` is also a
valid `.ui` array.

```ui
column {
  gap: 6
  text { text: "{{title}}" }
  dropdown { id: "mode" options: {{raw:mode_options}} selected: {{raw:mode}} }
  {{#foreach row in items}}
  text_input { id: "{{row.id}}" label: "{{row.label}}" value: "{{row.value}}" }
  {{/foreach}}
}
```

against `{"title":"Inspector","mode_options":["A","B"],"mode":1,"items":[{"id":"a","label":"Name","value":"Cube"}]}`.

`foreach` blocks nest, and an inner block sees the outer block's binding. The item binding
shadows any environment member of the same name for the whole body.

An unresolved path is a hard error: `missing template variable: PATH`. So is a `foreach` over
a path that is missing or is not an array, or one whose tag is unclosed
(`foreach: expected 'ITEM in ARRAY'`, `foreach: missing {{/foreach}}`).

> **Substitution happens inside comments too.** The template pass is textual; it does not know
> what a `//` comment is. A comment that mentions `{{raw:something}}` to explain the syntax
> will be substituted, and if the name does not exist the whole render fails with
> `missing template variable`. Write such a comment with the braces broken up, or do not write
> it.

There is also a string-valued variant, `render_ui_template(template, ids, vals, lists)`,
taking parallel `Vec<Str>` environments and `TemplateList` records instead of JSON. It
predates the JSON form and supports only single-level `{{ITEM.field}}` access.

---

## 8. Events

A `HitRegion` carries an `id`, an `on_click` action string and a rect. The host walks
`overlay.hits` backwards, finds the region under the pointer, and passes its `on_click` to
`nori_plugin_event(panel_id, action)`. That string is the entire protocol.

For `column`, `row`, `surface` and `button` the string is exactly what you wrote in
`on_click:`, so the convention is yours. Every plugin in the tree uses a `verb/noun` or
`prefix:arg` shape: `hello/bump`, `file/save`, `section:transform`. The `/` is lexable
unquoted, which is why it is the popular separator.

The widget and input kinds synthesise theirs. These you do not choose; you parse them.

| kind | region | action string |
|---|---|---|
| `checkbox` | whole row | `check:{id}:{new_value}` — the value it would become, so `true`/`false` |
| `slider` | track, then knob | `{id}:{value}` with `value` already clamped to `0..=1` |
| `dropdown` | trigger | `{id}` — bare, no colon |
| `dropdown` | option row *N* (popup layer) | `{id}:{N}` |
| `number` / `drag_value` | whole row (drag) | `number:{id}:{value}:{lo}:{hi}:{step}`, each to 6 decimals |
| `number` / `drag_value` | value box (click to edit) | `number_edit:{id}:{value}:{lo}:{hi}:{step}:{text}` |
| `text_input` | field | `text_edit:{id}:@px={size_px}:{value}` |
| `picker` | field | `picker:{id}` |
| `picker` (colour, focused) | S/V square | `picker_sv:{id}` |
| `picker` (colour, focused) | hue strip | `picker_hue:{id}` |
| `text_area` | whole rect | `text_area_edit:{id}`, or `text_area_view:{id}` when `read_only` |

Two ordering rules matter. Regions are emitted **large first, small second**, because the hit
test walks backwards — so a slider's knob beats its track and a number's value box beats its
drag row. And the popup layer is appended after everything else, so an open dropdown's options
beat whatever they cover.

A node with `disabled` set emits no hit regions at all.

Nothing here carries a *new* value except the checkbox. A slider drag and a colour-picker
click report only which region was hit; the host converts the pointer position against the
region's own rect, at whatever precision it wants.

---

## 9. Surfaces

```ui
column {
  fill_w: true
  fill_h: true
  row { padding: 6 text { text: "Scene" } }
  surface { id: "viewport" w: fill h: fill }
}
```

A `surface` draws nothing. It reports a `SurfaceRegion { id, rect }` in the computed overlay
and stops — a named hole in the document that the host fills.

That is the boundary the design defends: **a plugin never touches the GPU.** It punches the
hole, reads the screen rect back with `surface_screen_rect(overlay.surfaces, ox, oy, id)` or
`surface_rect_of(overlay, id)`, allocates its own interaction rect over that region for orbit
/ zoom / picking, and draws gizmos scoped to it. The host blits the texture.

Surface rects come back already clipped by any enclosing scroll viewport, so a surface
scrolled half out of view reports the visible half — which is what both a blit and a hit test
want. `surface_count` and `surface_id_at` let a host enumerate the holes without knowing the
plugin.

A `surface` with `on_click:` also gets an ordinary hit region, which is the simple way to make
the hole clickable without an interaction rect.

---

## 10. Runtime state, binds and focus

Three mechanisms let a host change a parsed document without re-parsing it.

**Binds.** `bind_visible` and `bind_disabled` name a variable in a string-valued environment;
`apply_binds(tree, ids, vals)` evaluates them over the whole tree. A value is falsy when it is
`""`, `"false"` or `"0"`, and truthy otherwise.

Both binds name the **enabled** condition, and both invert. `bind_visible: "has_sel"` hides
the node when `has_sel` is falsy. `bind_disabled: "can_save"` **disables** the node when
`can_save` is falsy — not when it is truthy. An absent variable reads as `""`, so a
`bind_disabled` naming a variable the host forgot to set disables the node.

A hidden node is removed from layout entirely. A disabled node still paints, but emits no hits.

Not every kind takes them. The containers, `text`, `button` and the seven widget / input kinds do;
the pure-drawing leaves — `rect`, `surface`, `spacer`, `divider`, `image` — do not, and never will
(§5.4). Bind the container instead.

`bind:` (no suffix) is accepted on `checkbox`, `slider`, `dropdown` and the four input kinds
and records a value-binding name; nothing in `std/nori_ui` consumes it — it is there for a
host that wants to route values by name.

**Runtime state.** `OverlayRuntimeState` maps a node id to a `NodeRuntimeState` that can
replace `classes`, merge extra classes, and force `hidden` / `disabled`, applied by
`apply_runtime_state`.

**Focus.** `collect_overlay_focusables(tree)` returns the tab stops in order: nodes with an
explicit `tab_index` first, ascending, then everything else in tree order. A node is a tab
stop if it is a `checkbox`, `dropdown`, `number`, `text_input`, `text_area` or `picker` with a
non-empty `id`, or any node with a non-empty `on_click` — for which the stop's id falls back
to the `on_click` string when there is no `id`. `slider` is deliberately excluded. Hidden and
disabled nodes are skipped, so binds must be applied first; and a tab stop is a **leaf** for
the walk, so the children of a clickable container are never collected.
`cycle_overlay_focus` moves through the list and wraps; `rs_add_focus` gives the focused node
the class `"focused"`, which is both what a `class focused { }` rule styles and what makes a
`text_input` draw its ring and caret and a colour `picker` open its popup.

---

## 10b. `code_editor`

`code_editor { }` is a node, and the one the Code panel is made of: a multi-line control with a
gutter, syntax colouring, a current-line band, selection rectangles, a caret and virtualization
over the lines. It takes `value`, `language`, `read_only`, `word_wrap`, `hard_tabs`, `tab_size`,
`line_numbers`, `highlight_line`, `hot_line`, `size_px`, `gutter_marks`, `line_notes`, `diags`,
`focus_from`/`focus_to`, and the three state fields the caller owns: `caret`, `sel_anchor` and
`scroll_x`/`scroll_y`.

The state fields are the bargain: the widget draws a caret and a selection and does not remember
them between frames, so the host reads the `code_editor_edit:` / `code_editor_view:` action, runs
its own editor over `MultilineEditState`, and feeds the offsets back. `code_editor_caret_at` is the
exact inverse of the paint — widget-local coordinates in, a byte offset out — so a press lands on
the column under the pointer rather than near it.

A `read_only: true` one still hits, still focuses and still receives presses: that is what makes a
snippet in a chat transcript selectable and copyable without being editable.

## 11. What does not exist yet

Stated plainly, in one place, because each of these is a thing a plugin author will reach for.

**`font_family`** is not accepted, on `text` or on `span`. There is no font-family field on the
paint primitive and neither painter can select a face, so it stays
`unknown text field "font_family"` / `unknown span field "font_family"` rather than becoming an
attribute nothing reads.

**The sixteen animation attributes are not accepted:**
`anim_pulse_alpha`, `anim_pulse_speed_hz` (`anim_pulse_hz`), `anim_fade_in_s`,
`anim_float_y_amp`, `anim_float_y_hz`, `anim_text_blink_period_s`, `anim_text_blink_duty`,
`text_char_wave_amp_px`, `text_char_wave_hz`, `text_char_reveal_stagger_s`,
`text_char_reveal_duration_s`, `text_char_chromatic_amp_px`, `text_outline_pulse_alpha`,
`text_shadow_pulse_alpha`, `text_typing_cps` and `text_shake_amp_px` on `text`.
Nothing in the overlay path carries a clock — `ComputedOverlay` has no time and the layout
entry points take no `t` — so there is nothing for them to animate against. This is worth
distinguishing from the *fifteen static* outline / gradient / shadow / decoration attributes
listed in §5.6, which **do** parse and **are** painted.

**Keyboard input does not exist.** The focus model in §10 computes a tab order and a `focused`
class, and that is all. No node consumes a key event, `text_input` and `text_area` never see
typing, and the caret a focused field draws is decorative. Editing happens by the host reading
the `text_edit:` / `text_area_edit:` action, running its own editor, and re-rendering.

**Twelve node attributes work outside stylesheets too.** `tag`, `position`, `z_index`, `top`,
`right`, `bottom`, `left`, `overflow_x`, `overflow_y`, `line_height`, `w` and `h` are accepted
directly on `column`/`panel`, `row`, `scroll_column`/`scroll`, `scroll_row`, `text`, `button`,
`rect` and `surface`. A `stylesheet { }` rule can set them
as well.

Two more gaps worth knowing:

- **Per-`span` `letter_spacing` is still rejected.** The node-level `letter_spacing` /
  `word_spacing` are measured and painted (§5.6), but a `span` cannot override them — spacing is a
  property of the whole run, and `TextSpanRun` carries no tracking field.
- **Unquoted percent and unit lengths do not work on the container `w`/`h`.**
  `column { w: 50% }` reports `expected identifier`; write `w: "50%"`. `rect`, `surface`,
  `image`, `spacer` and `divider` take the unquoted form fine.
