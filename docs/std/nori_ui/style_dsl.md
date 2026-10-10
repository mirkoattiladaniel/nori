# The `.style` language

A stylesheet gives `.ui` documents their look: colours, sizes, spacing, type and position, set once
for every node of a kind or of a class instead of on each node. It is written inline, inside a
document's `stylesheet { … }` block, or as a standalone `.style` file of rules. The documents
themselves are described in [the `.ui` DSL](ui_dsl.md); values — colours, numbers, booleans and
names — are written exactly as there ([§3, lexical syntax](ui_dsl.md#3-lexical-syntax)).

## Rules

```ui
document {
  stylesheet {
    tag button {
      radius: 5
      size_px: 12
      padding: 8
      bg: [44, 44, 52, 255]
      text_color: [220, 218, 230, 255]
      hover_bg: [56, 56, 66, 255]
      active_bg: [70, 70, 82, 255]
    }
    class title { size_px: 16 bold: true }
  }
  column {
    text { classes: [title] text: "Materials" }
    button { label: "Apply" on_click: "mat/apply" }
  }
}
```

A `tag NAME { }` rule matches nodes whose stylesheet tag is `NAME`. That tag is the node's
explicit `tag:` attribute if it has one, and otherwise the canonical name of its kind:
`column`, `row`, `text`, `button`, `rect`, `spacer`, `surface`, `scroll_column`, `scroll_row`,
`image`, `checkbox`, `slider`, `dropdown`, `number`, `text_input`, `text_area`, `picker`. A
`divider` is a `rect`, so `tag rect` catches it and `class divider` is the way to reach it
specifically.

A `class NAME { }` rule matches any node listing `NAME` in `classes` (or `class`). Tag rules
resolve first and class rules layer over them, so a class wins over a tag.

Class rules also reach **spans**: a `span { classes: [x] }` layers `class x`'s properties over the
style its parent `text` node already resolved. Only class rules — a span never consults a tag rule,
because its base is the already-tag-resolved node. Of the property list below, a span takes the
four its `TextSpanRun` can carry: `size_px`, `text_color`/`color`, `bold` and `italic`.

Within one sheet, **a duplicate rule name replaces the earlier rule wholesale** — the second
`tag button { }` is not merged into the first, it discards it.

## Properties

**The stylesheet property set is not the node attribute set.** This is the single most common
surprise. `height:` is a perfectly good attribute on several kinds and is
`` unknown style property `height` `` in a rule; `fill_w`, `gap`, `on_click`, `id` and every
widget-specific attribute are likewise not style properties. Conversely `hover_bg`,
`active_bg`, `hover_text_color` and `hover_border_color` exist *only* as style properties —
there is no way to write a hover colour as a node attribute.

The full property list, with aliases:

| property | aliases | value |
|---|---|---|
| `bg` | | colour |
| `color` | | colour |
| `text_color` | | colour — applied to `text` nodes' colour specifically |
| `border_color` | | colour |
| `hover_bg`, `hover_border_color`, `hover_text_color`, `active_bg` | | colour |
| `size_px` | `font_size`, `text_size`, `label_size_px` | number |
| `radius` | | number |
| `padding` | `pad` | number |
| `border_w` | `border_width` | number |
| `line_height` | `line_height_mult` | number |
| `letter_spacing`, `word_spacing` | | number — px, see §5.6 of [the `.ui` DSL](ui_dsl.md) |
| `max_width` | `max_w` | number |
| `min_height` | `min_h` | number |
| `bold` | `font_bold` | bool |
| `italic` | `font_italic` | bool |
| `wrap` | `wrap_text` | bool |
| `wrap_hard_break` | | bool |
| `line_clamp` | | number, floored at 1 |
| `text_overflow` | `overflow` | `clip` \| `ellipsis` |
| `text_transform` | `transform` | `none`\|`uppercase`\|`lowercase`\|`capitalize` |
| `text_decoration` | `decoration` | `none`\|`underline`\|`strikethrough`\|`line_through`\|`overline` |
| `overflow_x`, `overflow_y` | | `visible`\|`hidden`\|`auto`\|`scroll` |
| `position` | | `relative` \| `absolute` \| `overlay` |
| `z_index` | | number |
| `top`, `right`, `bottom`, `left` | | number |

Anything else is `` unknown style property `NAME` ``.

This is how the widget kinds get their colours: `checkbox`, `slider`, `dropdown`, `number`,
`text_input`, `text_area` and `picker` have no `bg` or `color` attribute of their own, but
their paint code reads those fields, so a `tag dropdown { bg: … }` rule reaches them.

## Standalone `.style` files

A standalone `.style` file is the same rules without the wrapper, parsed with
`dsl_parse_stylesheet`:

```
tag text { size_px: 12 line_height: 1.2 }
class panel_root { bg: [26, 26, 32, 255] }
```

`dsl_parse_stylesheet(src)` is the entry point for one: it takes the file's text and gives back the
rules. An empty source is its own outcome, `empty overlay stylesheet`. A document names the sheet it
wants with `style: "name"` (see [§2 of the `.ui` DSL](ui_dsl.md#2-the-shape-of-a-document)); the
parser only records that name, and finding the file is the host's job.
