# std/nori_ui::layout

```nori
import "std/nori_ui" as ui          // then ui::layout::…
```

### `fn ap_ids_of(name: Str) -> Vec<Int>`

the property ids a `.style` name stands for in a transition (`translate` and `scale` are two,
`all` is every one); empty when the name is not animatable.

### `struct AVal`

one animatable value.

### `fn av_get(n: UiNode, p: Int) -> AVal`

the drawn value of property `p` on a node (a colour the node does not draw is transparent).

### `fn av_set(inout n: UiNode, p: Int, v: AVal)`

set property `p` on a node to `v`.

### `struct AValOpt`

the value a NodeStyle (a keyframe) sets for property `p`; `has` false when it sets none.

### `fn av_lerp(p: Int, x: AVal, y: AVal, t: Float) -> AVal`

`x` to `y` at `t` (0..1, may overshoot with a bezier that does).

### `fn ease_at(e: Ease, t: Float) -> Float`

the eased progress at `t` in 0..1.

### `struct RTrans`

one property in transition on one node.

### `fn trans_find(ts: Vec<TransSpec>, p: Int) -> Int`

the transition spec for property `p` in `ts`, or -1.

### `fn rtrans_value(t: RTrans, now: Float) -> AVal`

a running transition's value at `now`, and whether it has finished.

### `struct AnimAt`

where an animation is at `now`: `on` false before it starts (unless it fills backwards) and
after it ends (unless it fills forwards); `f` the progress through the keyframes (0..1), with
the direction applied; `done` true once it can never change again.

### `fn kf_value(kf: CKeyframes, p: Int, f: Float, base: AVal, e: Ease) -> AValOpt`

the value of property `p` at progress `f` through keyframes `kf`, easing each segment between
two stops that set it; `base` stands in at 0% and 100% where no stop sets the property.
`has` false when no stop sets `p` at all.


### `fn area_pad() -> Float`

the inset a `text_area` uses when nothing says otherwise: CONTENT_PAD, and the default value of
`node.padding`, so passing this is the same as passing nothing.

Padding is a style property. `padding`, `padding_x` and `padding_y` are node attributes and
`padding` cascades from a stylesheet class, as on a `row`, so a panel that wants a
tighter input bar says so in its `.ui` or its style block rather than subtracting pixels it does
not own. These two take the number so that the height a panel reserves and the height the widget
paints agree.

### `struct AreaEdit`

the caller-owned state of a `text_area`: the three attributes the node takes, plus the
text itself.

### `fn area_from(text: Str, caret: Int, anchor: Int, scroll_y: Float) -> AreaEdit`

...restored from what a panel kept since the last frame.

### `fn area_insert(inout a: AreaEdit, s: Str)`

insert text at the caret, replacing the selection.

Newlines are kept, unlike `field_insert`, which runs its argument through `field_flatten` and
turns a pasted paragraph into one long line. The shell delivers a paste as
`char:<id>:<the text>`, so this is the path a paste takes.

### `fn area_takes(sym: Int, mods: Int) -> Bool`

does this keystroke belong to the box? The same question `field_takes` answers, plus the two
keys that only mean something when there is more than one line.

### `fn area_key(inout a: AreaEdit, sym: Int, mods: Int) -> Str`

a keystroke. Returns "" for most, "copy" or "cut" when the caller should put `area_sel_text` on
the clipboard (this module cannot reach it: the clipboard belongs to whatever
owns the window).

`mods` is the shell's mask: 1 shift, 2 ctrl, 4 alt.

Enter opens a line here. A panel that wants Enter to do something else (send a message, accept a
dialog) checks for it before calling this.

Paste is not a key. The shell catches Ctrl+V itself and delivers the clipboard as
`char:<id>:<text>` before a plugin ever sees the keysym, so there is nothing here
for 118: an insert is all a paste is.

### `fn area_attrs(a: AreaEdit) -> Str`

`caret: N sel_anchor: N scroll_y: N` for this box, as a `.ui` fragment: the three attributes
`text_area` takes, emitted together so that no caller can send one and forget another.

### `fn area_rows(a: AreaEdit, box_w: Float, size_px: Float, wrap: Bool, pad_x: Float) -> Int`

how many rows the text occupies at this width, wrapped: what the box would like to be tall
enough for, before any cap is applied.

### `fn area_height(a: AreaEdit, box_w: Float, size_px: Float, wrap: Bool, min_rows: Int, max_rows: Int, pad_x: Float, pad_y: Float) -> Float`

...and how tall that is, in the same arithmetic the node's own intrinsic height uses, capped by
`max_rows` when one is given (0 for no cap).

A panel needs this before it emits the node, because the row the box sits in has to reserve the
space, which is why it is a function here rather than something only the layout knows.

### `fn area_caret_at(a: AreaEdit, w: Float, h: Float, size_px: Float, wrap: Bool, lx: Float, ly: Float) -> Int`

the byte offset under a point in the box, in widget-local coordinates. The inverse of the
paint, so a press lands on the character it is over rather than near it.

### `fn area_pointer(inout a: AreaEdit, action: Str, id: Str, size_px: Float, wrap: Bool) -> Bool`

apply a pointer action the shell addressed to this box: `press:<id>:<x>:<y>:<w>:<h>:<clicks>:<mods>`
or `dragto:` in the same shape. True when it was one of those, for this `id`, and it was applied.

A double click selects the word, a triple the line. Shift with a press extends from where the
caret already is, the way it does in every other text box.


### `fn caret_blink_period_ms() -> Int`

how long the caret spends on, and then off, in milliseconds.

530ms is the interval Windows has long used and most toolkits copied; a
caret is a thing people read past rather than look at, and faster reads as
a flicker.

### `fn caret_blink_on(since_ms: Int) -> Bool`

the blink phase for a caret whose owner last did something `since_ms` ago.

Phase 0 is ON. Measuring from the last keystroke rather than from a fixed
epoch means the caret is solid the instant you type and stays that way for
a full period, so a burst of typing never strobes.

### `fn set_caret_on(on: Bool)`

tell this image of the library whether carets are in their ON phase.

Call it once per frame, before laying anything out. Widgets are painted
from documents the host does not own field by field, so the phase cannot
travel as an attribute; it is one global read at four paint sites.

### `fn caret_on() -> Bool`

whether a focused widget should paint its caret this frame.


### `fn nori_keywords() -> Vec<Str>`

control flow, declarations and the parallelism forms. Mirrors the `noriKeyword`/`noriStorage`/
`noriConv`/`noriBool` groups of the Vim syntax file, which splits them only to hang different
highlight links off them; all four land on `keyword` here.

### `fn nori_types() -> Vec<Str>`

the built-in types (`noriType`).

### `fn nori_builtins() -> Vec<Str>`

the built-in functions (`noriBuiltin`).

### `fn nori_line_spans(line: Str) -> Vec<HighlightSpan>`

highlight spans for one line of Nori, left to right, non-overlapping.

Columns are byte offsets into `line`. A run the caller should draw in the default colour is
simply not covered by any span, which is why the painter walks the gaps rather than expecting a
span per character.

### `fn language_line_spans(language: Str, line: Str) -> Vec<HighlightSpan>`

highlight spans for one line of `language`. An unknown language yields none, which paints the
line in the default text colour, the right answer for plain text.

### `fn diagnostics_line_spans(line: Str) -> Vec<HighlightSpan>`

one line of a compiler report, coloured.

Purely syntactic, like every other language here: what a line's shape is, never what it means.
This paints the report `noric` and `roll` write, the one a person reads, so that the
same text can be selected and copied out of a panel instead of being redrawn as rows of labels
that no pointer can take a span of.

The shapes, and nothing beyond them:

  `== package <dir>`     a package boundary in a `--all` run
  `◆ …` / `▲ …`          the severity marker, and the message on the same line
  `  <file> · <L>:<C>`   where it is
  `▸ …`                  the suggested fix
  `  123 ▏ …`            the echoed source line, and the `━━━` underline beneath it

The glyphs are matched by bytes: the buffer is byte-indexed, and each of these is multi-byte
UTF-8: ◆ is E2 97 86, ▲ is E2 96 B2, ▸ is E2 96 B8, · is C2 B7, ━ is E2 94 81, ▏ is E2 96 8F.

### `fn manifest_line_spans(line: Str) -> Vec<HighlightSpan>`

one line of a `nori.manifest`, coloured.

Purely syntactic: what a line's shape is, never what its words mean. Whether `auto-loop` is a
key that exists is a question for `std/manifest`, which owns the schema; this module has no
business importing a build system to paint a word, and a manifest for a toolchain that grew a
key yesterday should still colour correctly today.

Four shapes and no more: a comment, a `[section]`, a `key = value`, and a blank.

### `struct CeRow`

one visual row: a slice [start, end) of the buffer, and which logical line it came from.

### `fn code_editor_rows(state: MultilineEditState, content_w: Float, size_px: Float, wrap: Bool) -> Vec<CeRow>`

the visual rows of `state.text` laid out for a content width of `content_w`.

With `wrap` false this is one row per logical line and `content_w` is ignored. With it true, a
line too wide to fit is broken at the last space that fits; a run with no space in it, such as a long
path or a base64 blob, is broken mid-word rather than allowed to overflow, because a wrap mode
that sometimes does not wrap is worse than one that occasionally splits a token.

Cost: this walks the whole buffer, so it is O(document) per call and the virtualization above it
only saves the painting, not the measuring. That is fine for source files and would not be for a
log; the fix, when it is needed, is caching these against a buffer revision.

### `fn ce_row_of(rows: Vec<CeRow>, off: Int) -> Int`

the index of the visual row holding `off`, or the last row.

A caret at a wrap point belongs to the row it starts, not the one it ends, which is what makes
pressing End then Right move to the next visual row rather than sitting still.

### `fn code_editor_caret_pos(state: MultilineEditState, rect: Rect, size_px: Float, line_numbers: Bool, wrap: Bool) -> Vec2`

the caret's pixel position (top-left of its cell) for a widget at `rect`.

### `fn code_editor_content_width(state: MultilineEditState, rows: Vec<CeRow>, size_px: Float) -> Float`

the width of the widest row, which is what a horizontal scrollbar has to span.

### `fn code_editor_content_rect(state: MultilineEditState, rect: Rect, size_px: Float, line_numbers: Bool) -> Rect`

the rectangle the text occupies inside `rect`: past the gutter, inside the padding.

Shared by the paint and the hit test. A click maps back to a caret only if it is
measured against the same content rect the glyphs were placed in, and two copies of
`gw + 8` would eventually disagree, putting the caret one column off near the gutter.

### `fn code_editor_caret_at(state: MultilineEditState, w: Float, h: Float, size_px: Float, line_numbers: Bool, wrap: Bool, lx: Float, ly: Float) -> Int`

the caret offset for a point in widget-local coordinates, where (0,0) is the widget's top-left.

Local rather than absolute because the caller that needs this is usually a plugin, which is told
the size of its widget but not where the shell put it. Scroll is already in `state`, so this is
the inverse of what `layout_code_editor` drew.

### `fn layout_code_editor(state: MultilineEditState, rect: Rect, size_px: Float, focused: Bool, opts: MultilinePaintOpts, pal: SyntaxTokenPalette, language: Str, wrap: Bool) -> Vec<PaintPrimitive>`

lay a syntax-highlighted editor out into `rect`.

Structurally the same pass as `layout_multiline_edit` (gutter numbers, current-line highlight,
per-line selection rects, visible lines with overscan, caret) with the single flat `pp_text` per
line replaced by one `pp_text` per coloured run. `opts.line_numbers` and `opts.highlight_current`
are honoured rather than forced off, because unlike `text_area` this widget is the gutter one.

`pal` supplies the colours; build it with `palette_defaults(opts.text_color)` for the built-in
scheme, or `palette_new(default, sheet)` to let a stylesheet override token classes.


### `struct UiComp`

a component definition: its parameters (with defaults where given), and where its body is.

### `struct DslCtx`

what a parse carries from file to file: the components it knows, the files it has read (`deps`,
in the order it read them), the files imported so far, and the chain being imported now.

### `fn nk_fragment() -> Int`

node kind: a fragment, the children a component's use put in its `slot`. Only the parser sees
one: a container that is handed a fragment takes its children instead (dsl_push_child).

### `fn dsl_comp_find(cx: DslCtx, name: Str) -> Int`

the component `name` in `cx`, or -1.


### `fn collapsed_strip_size() -> Float`

width/height of the thin strip shown for a collapsed pane (click to expand).

### `enum SplitDirection`

direction of a split: Horizontal = left | right, Vertical = top | bottom.

### `fn split_direction_tag(d: SplitDirection) -> Int`

integer tag of a SplitDirection (0 = Horizontal, 1 = Vertical).

### `struct SplitData`

the payload of a Split node: direction, ratio (fraction for the first child), per-side
collapse flags, and the two child subtrees.

### `struct LeafData`

the payload of a Leaf node: its tabs, active index, and (after layout) its rect.

### `enum Node`

a dock tree node: empty, a leaf (tabs), or a split (two children).

### `struct Tree`

a dock subtree (wraps a single Node; empty trees carry Node::NEmpty).

### `struct Surface`

one surface (main window or a floating window) holds a single tree.

### `fn tree_dup(t: Tree) -> Tree`

a Tree of its own: every leaf's tabs and rect and every split copied.

### `fn node_dup(n: Node) -> Node`

a Node of its own (see tree_dup).

### `fn surfaces_dup(v: Vec<Surface>) -> Vec<Surface>`

a fresh list of surface copies.

### `fn sd_direction(s: SplitData) -> SplitDirection`

the split direction.

### `fn sd_ratio(s: SplitData) -> Float`

the split ratio (fraction of space for the first child).

### `fn sd_set_ratio(inout s: SplitData, r: Float)`

set the split ratio.

### `fn sd_first_collapsed(s: SplitData) -> Bool`

whether the first child is collapsed.

### `fn sd_second_collapsed(s: SplitData) -> Bool`

whether the second child is collapsed.

### `fn sd_set_first_collapsed(inout s: SplitData, b: Bool)`

set the first-child collapse flag.

### `fn sd_set_second_collapsed(inout s: SplitData, b: Bool)`

set the second-child collapse flag.

### `fn sd_first(s: SplitData) -> view Tree`

the first child subtree.

### `fn sd_second(s: SplitData) -> view Tree`

the second child subtree.

### `fn sd_set_first(inout s: SplitData, sink t: Tree)`

replace the first child subtree.

### `fn sd_set_second(inout s: SplitData, sink t: Tree)`

replace the second child subtree.

### `fn ld_tabs(l: LeafData) -> view Vec<Str>`

the leaf's tabs (panel ids). A view of the leaf's own list: callers that rewrite tabs in place
(remap_tree_tabs renames them during a dock move) reach the list through `l.tabs` directly.

### `fn ld_active(l: LeafData) -> Int`

the active tab index.

### `fn ld_set_active(inout l: LeafData, a: Int)`

set the active tab index.

### `fn ld_has_rect(l: LeafData) -> Bool`

whether the leaf has a laid-out rect.

### `fn ld_rect(l: LeafData) -> view Rect`

the leaf's laid-out rect (valid only when ld_has_rect is true).

### `fn ld_set_rect(inout l: LeafData, sink r: Rect)`

set the leaf's laid-out rect.

### `fn ld_clear_rect(inout l: LeafData)`

clear the leaf's laid-out rect.

### `fn tree_empty() -> Tree`

an empty tree.

### `fn tree_leaf(sink tabs: Vec<Str>, active: Int) -> Tree`

a leaf tree with the given tabs and active index.

### `fn tree_split(direction: SplitDirection, ratio: Float, sink first: Tree, sink second: Tree) -> Tree`

a split tree with the given direction, ratio, and children (not collapsed).

### `fn tree_node(t: Tree) -> view Node`

the node of a tree.

### `fn tree_set_node(inout t: Tree, sink n: Node)`

replace a tree's node.

### `fn node_is_leaf(n: Node) -> Bool`

true if the tree's node is a leaf.

### `fn node_is_split(n: Node) -> Bool`

true if the tree's node is a split.

### `fn node_is_empty(n: Node) -> Bool`

true if the tree's node is empty.

### `fn node_leaf_tabs(n: Node) -> Vec<Str>`

the leaf's tabs (empty Vec if not a leaf) - for code that can't match the payload directly.

### `fn node_leaf_active(n: Node) -> Int`

the leaf's active index (0 if not a leaf).

### `fn node_split_dir(n: Node) -> SplitDirection`

the split's direction (Horizontal if not a split).

### `fn node_split_ratio(n: Node) -> Float`

the split's ratio (0 if not a split).

### `fn node_split_fc(n: Node) -> Bool`

the split's first-collapsed flag.

### `fn node_split_sc(n: Node) -> Bool`

the split's second-collapsed flag.

### `fn node_split_first(n: Node) -> view Tree`

the split's first child (empty tree if not a split).

### `fn node_split_second(n: Node) -> view Tree`

the split's second child (empty tree if not a split).

### `fn path_copy(p: Vec<Int>) -> Vec<Int>`

a fresh copy of a path.

### `fn path_with(p: Vec<Int>, idx: Int) -> Vec<Int>`

a fresh path = `p` with `idx` appended.

### `fn path_parent(p: Vec<Int>) -> Vec<Int>`

a fresh path = `p` without its last element.

### `fn path_last(p: Vec<Int>) -> Int`

the last element of a non-empty path.

### `fn path_eq(a: Vec<Int>, b: Vec<Int>) -> Bool`

true if two paths are equal.

### `struct TreeOpt`

a tree-at-path result: `tree` is a view into the tree that was searched (a miss views the
searched tree itself), so a caller can rewrite the subtree in place through it.

### `fn node_child(inout t: Tree, idx: Int) -> TreeOpt`

the child subtree of `t`'s node by index (0 = first, else second); ok=false for non-splits.

### `fn tree_at_path(inout tree: Tree, path: Vec<Int>) -> TreeOpt`

the subtree at `path` from `tree` (path empty = the tree itself); ok=false if the path is invalid.

### `struct LeafRect`

a (path, rect) pair for a laid-out leaf.

### `fn leaf_rects_inner(tree: Tree, path: Vec<Int>, inout out: Vec<LeafRect>)`

recursive worker for tree_leaf_rects.

### `fn tree_leaf_rects(tree: Tree) -> Vec<LeafRect>`

collect (path, rect) for every laid-out leaf (call after layout_surface).

### `struct TabLoc`

a (path, tab index) location of a tab within the tree.

### `fn tab_locations_inner(tree: Tree, tab: Str, path: Vec<Int>, inout out: Vec<TabLoc>)`

recursive worker for tree_tab_locations.

### `fn retain_in_tree(tree: Tree, keep: Vec<Str>)`

recursively keep only tabs present in `keep`, rebuilding each leaf and clamping active (empty
leaves are left in place, not collapsed).

### `fn remap_tree_tabs(inout tree: Tree, remap: Map<Str, Str>)`

recursively rename tab ids in place via `remap` (old id -> new id); ids absent from the map are left
as-is. Complements normalize_tree_tabs (which then drops invalid ids).

### `fn tree_tab_locations(tree: Tree, tab: Str) -> Vec<TabLoc>`

every (path, index) where `tab` appears in the tree.

### `struct SplitRects`

a split-rect result: first child rect, splitter rect, second child rect.

### `fn split_rect(rect: Rect, direction: SplitDirection, ratio: Float, splitter_width: Float) -> SplitRects`

compute child/splitter rects for `rect` split by `direction` at `ratio` with `splitter_width`.

### `struct SplitRectsC`

a collapsible split-rect result: rects plus whether the splitter is a collapsed strip.

### `fn split_rect_collapsible(rect: Rect, direction: SplitDirection, ratio: Float, splitter_width: Float, first_collapsed: Bool, second_collapsed: Bool) -> SplitRectsC`

like split_rect but honoring per-side collapse: a collapsed child becomes a thin strip.

### `struct SplitterInfo`

info about one splitter produced by layout: path, splitter rect, parent rect, direction,
whether it's a collapsed edge, and (if so) which side is collapsed.

### `fn layout_tree(tree: Tree, rect: Rect, splitter_width: Float, inout out: Vec<SplitterInfo>, path: Vec<Int>)`

recursively lay the tree into `rect`, filling each leaf's rect and collecting splitter infos.

### `fn str_vec_insert(v: Vec<Str>, idx: Int, x: Str) -> Vec<Str>`

a fresh Vec<Str> = `v` with `x` inserted at `idx` (clamped to [0, len]).

### `fn str_vec_remove(v: Vec<Str>, idx: Int) -> Vec<Str>`

a fresh Vec<Str> = `v` with element at `idx` removed (no-op copy if out of range).

### `fn tree_collapse_empty_leaf(inout tree: Tree, leaf_path: Vec<Int>)`

collapse the empty leaf at `leaf_path` by replacing its parent split with the sibling subtree.

### `fn tree_replace_leaf_with_split(inout tree: Tree, leaf_path: Vec<Int>, direction: SplitDirection, ratio: Float, new_pane_is_first: Bool, tab: Str) -> Bool`

replace the leaf at `leaf_path` with a split: the old leaf and a new leaf holding only `tab`.
`new_pane_is_first` puts the new leaf on the first (top/left) side. Returns true on success.

### `fn tree_wrap_root_with_split(inout tree: Tree, direction: SplitDirection, ratio: Float, new_pane_is_first: Bool, tab: Str) -> Bool`

Wrap the whole tree in a new split, putting `tab` in the new pane beside everything that is
already there. Unlike `tree_replace_leaf_with_split` the target is the root, which is usually a
split rather than a leaf. This is how a panel becomes a full-height
column beside the rest of the dock instead of taking half of one leaf.

### `fn tree_path_ok(tree: Tree, path: Vec<Int>) -> Bool`

true when `path` names a subtree of `tree` (every step descends a split).

### `fn tree_view_at(tree: Tree, path: Vec<Int>) -> view Tree`

the subtree at `path`, read-only; an invalid path yields `tree` itself (check tree_path_ok first).

### `fn tree_is_leaf_collapsed(tree: Tree, leaf_path: Vec<Int>) -> Bool`

true if the leaf at `leaf_path` is collapsed (its parent split marks that side collapsed).

### `struct DockState`

the full dock state: surfaces plus optional keyboard/click focus.

### `fn dock_state_new(sink tab_ids: Vec<Str>) -> DockState`

a new DockState with one surface whose root leaf holds `tab_ids`, focused on surface 0.

##### DockState

### `fn set_tab_title(self, id: Str, title: Str)`

what to draw for tab `id`. Setting the same id twice replaces it.

### `fn tab_title(self, id: Str) -> Str`

the title for `id`, or `id` itself when nobody set one, so an untitled dock shows the
ids and a host that does not care needs to do nothing.

### `fn surface_tree(self, i: Int) -> view Tree`

the tree of surface `i`, or an empty tree if `i` is out of range.

### `fn set_focus(inout self, surface_index: Int, has_leaf: Bool, leaf_path: Vec<Int>)`

set focus to a surface and (optional) leaf path; clamps to valid surfaces.

### `fn clear_focus(inout self)`

clear all focus.

### `fn push_to_first_leaf(self, tab: Str)`

push a tab onto the first leaf of surface 0 (descends first children to find it).

### `fn retain_tabs(self, keep: Vec<Str>)`

keep only tabs whose id is in `keep`, everywhere (empty leaves stay; active clamped).

### `fn normalize_dock_state(inout self, valid: Vec<Str>, remap: Map<Str, Str>, ensure_tabs: Vec<Str>, default_dock: DockState)`

normalize the dock: remap tab ids (`remap` old->new), drop tabs not in `valid`, and clamp active;
if that leaves no tabs anywhere, replace the whole dock with `default_dock`; otherwise push any
`ensure_tabs` that are valid but absent onto the first leaf. The id-normalizing and
default-if-empty steps are a remap Map and a prebuilt default dock.

### `fn tab_is_present(self, surface_index: Int, tab: Str) -> Bool`

true if `tab` appears anywhere on surface `surface_index`.

### `fn set_tab_present(self, surface_index: Int, tab: Str, present: Bool)`

add (`present`=true) or remove every instance (`present`=false) of `tab` on the surface.

### `fn find_leaf_path_containing_point(self, surface_index: Int, point: Vec2) -> TreeFindPath`

the leaf path whose laid-out rect contains `point` on surface `surface_index` (front-to-back).

### `fn layout_surface(self, surface_index: Int, dock_rect: Rect, splitter_width: Float) -> Vec<SplitterInfo>`

lay out surface `surface_index` into `dock_rect`, filling leaf rects; returns splitter infos.

### `fn set_split_collapsed(self, surface_index: Int, path: Vec<Int>, first_side: Bool, collapsed: Bool)`

set whether the first/second child of the split at `path` is collapsed.

### `fn collapse_leaf(self, surface_index: Int, leaf_path: Vec<Int>)`

collapse the leaf at `leaf_path` (its sibling takes the space).

### `fn expand_leaf(self, surface_index: Int, leaf_path: Vec<Int>)`

expand the leaf at `leaf_path` if collapsed.

### `fn is_leaf_collapsed(self, surface_index: Int, leaf_path: Vec<Int>) -> Bool`

true if the leaf at `leaf_path` is collapsed.

### `fn remove_leaf(self, surface_index: Int, leaf_path: Vec<Int>)`

remove the leaf at `leaf_path` (replace its parent split with the sibling).

### `fn collapse_empty_leaf(self, surface_index: Int, leaf_path: Vec<Int>)`

collapse the leaf at `leaf_path` if it has no tabs.

### `fn set_split_ratio(self, surface_index: Int, path: Vec<Int>, ratio: Float)`

set the ratio (clamped to 0.15..0.85) of the split at `path`.

### `fn split_ratio(self, surface_index: Int, path: Vec<Int>) -> Float`

the ratio of the split at `path`, or -1 if not a split.

### `fn resize_split_from_cursor(self, surface_index: Int, path: Vec<Int>, parent_rect: Rect, direction: SplitDirection, cursor: Vec2, splitter_width: Float)`

drag a divider to follow `cursor`, preserving the outer size of any nested same-direction split
child (so resizing `((A|B)|C)` keeps A's width).

### `fn set_leaf_active(self, surface_index: Int, leaf_path: Vec<Int>, active: Int)`

set the active tab index of the leaf at `leaf_path` (clamped to the tab count).

### `fn remove_tab_from_leaf(self, surface_index: Int, leaf_path: Vec<Int>, tab_index: Int) -> Bool`

remove tab `tab_index` from the leaf at `leaf_path`; returns true if removed.

### `fn insert_tab_into_leaf(self, surface_index: Int, leaf_path: Vec<Int>, tab_index: Int, tab: Str) -> Bool`

insert `tab` into the leaf at `leaf_path` at `tab_index` (push if past the end); returns true on success.

### `fn split_leaf_and_put_tab(self, surface_index: Int, leaf_path: Vec<Int>, direction: SplitDirection, ratio: Float, new_pane_is_first: Bool, tab: Str) -> Bool`

split the leaf at `leaf_path`, moving `tab` into a new sibling leaf; returns true on success.

### `fn split_surface_and_put_tab(self, surface_index: Int, direction: SplitDirection, ratio: Float, new_pane_is_first: Bool, tab: Str) -> Bool`

put `tab` in a new pane beside the whole surface: a full-height column or a full-width strip
spanning everything already docked, rather than half of one leaf.

### `struct TreeFindPath`

a path-find result (present flag + path).

### `fn push_leaf_first(tree: Tree, tab: Str) -> Bool`

push `tab` onto the first leaf reachable by descending first children of `tree`.

### `fn imin_i(a: Int, b: Int) -> Int`

min of two Ints (local helper; std math is aliased).

### `fn imax_i(a: Int, b: Int) -> Int`

max of two Ints (local helper).

### `fn dock_show_tab(ds: DockState, surface_index: Int, tab: Str) -> Bool`

make `tab` visible in surface `surface_index`: if it is already docked, activate it in its
leaf; if it is not, append it to the first laid-out leaf and activate it there. Returns false
only when the surface has no leaf to put it in.

This is what a "Panels" menu needs and nothing in DockState offered: `set_leaf_active` wants a
path the caller does not have, and `push_to_first_leaf` neither focuses nor checks for a tab
that is already there (docking it twice).

### `fn dock_tab_is_showing(ds: DockState, surface_index: Int, tab: Str) -> Bool`

true when `tab` is the active tab of whatever leaf holds it, i.e. showing right now.
A "Panels" menu greys out the entry for the panel that is already in front, which is the
honest way to have a disabled row without inventing a command that does nothing.

### `fn dock_adopt(inout ds: DockState, src: DockState)`

take on `src`'s surfaces and focus, in place.

A host holds its DockState by reference (`inout`), so `ds = seed_dock(h)` is rejected: it can
be changed, but not replaced. A "reset layout" menu action needs exactly that, so it is offered
here rather than open-coded in every host.


### `fn str_vec_contains(v: Vec<Str>, x: Str) -> Bool`

true if `x` is in the Vec<Str> `v`.

### `fn prefer_tabs(valid: Vec<Str>, candidates: Vec<Str>) -> Vec<Str>`

the elements of `candidates` that are present in `valid`, in candidate order.

### `fn build_horizontal_split(valid_tab_ids: Vec<Str>, left_candidates: Vec<Str>, right_candidates: Vec<Str>, left_ratio: Float) -> DockState`

a DockState with a horizontal split: left/right tab groups (filtered to `valid_tab_ids`),
first child weighted by `left_ratio`. Falls back to a single leaf when a side is empty.

### `fn path_copy_str(v: Vec<Str>) -> Vec<Str>`

a fresh copy of a Vec<Str> (so callers don't share the backing handle).

### `fn collect_dock_tab_ids(tree: Tree, inout out: Vec<Str>)`

collect every tab id in `tree` into `out` (deduplicated).

### `fn collect_dock_tab_ids_from_state(state: DockState) -> Vec<Str>`

every tab id present across all surfaces of `state` (deduplicated).

### `fn tree_has_any_tabs(tree: Tree) -> Bool`

true if any leaf in `tree` has at least one tab.

### `fn normalize_tree_tabs(tree: Tree, valid_tab_ids: Vec<Str>)`

strip tabs not in `valid_tab_ids` from every leaf of `tree`, clamping each leaf's active index.

### `fn normalize_dock_strip_unknown(dock: DockState, valid_tab_ids: Vec<Str>)`

strip unknown tabs from every surface of `dock` (in place), keeping only `valid_tab_ids`.

### `fn dock_state_from_tree(sink tree: Tree) -> DockState`

wrap a single tree in a one-surface DockState.

### `fn build_vertical_split(valid: Vec<Str>, top_candidates: Vec<Str>, bottom_candidates: Vec<Str>, top_ratio: Float) -> DockState`

a vertical split: top/bottom tab groups (filtered to `valid`), first weighted by `top_ratio`.

### `fn build_three_pane(valid: Vec<Str>, left_c: Vec<Str>, center_c: Vec<Str>, right_c: Vec<Str>, left_ratio: Float, center_ratio: Float) -> DockState`

three side-by-side columns: left | center | right. `left_ratio` = left's fraction of the whole;
`center_ratio` = center's fraction of the remainder. Empty groups are dropped (2- or 1-pane fallback).

### `fn build_ide(valid: Vec<Str>, sidebar_c: Vec<Str>, main_c: Vec<Str>, bottom_c: Vec<Str>, sidebar_ratio: Float, main_ratio: Float) -> DockState`

classic IDE layout: a left sidebar | a right column split into a main area (top) and a bottom
panel. `sidebar_ratio` = sidebar's fraction of the width; `main_ratio` = main's fraction of the height.

### `fn build_grid_2x2(valid: Vec<Str>, tl_c: Vec<Str>, tr_c: Vec<Str>, bl_c: Vec<Str>, br_c: Vec<Str>, col_ratio: Float, row_ratio: Float) -> DockState`

a 2x2 grid: top-left / top-right / bottom-left / bottom-right. `col_ratio` = left column's width
fraction; `row_ratio` = top row's height fraction. Empty cells collapse gracefully.


### `fn menu_bar_height() -> Float`

height of the top menu bar.

### `fn toolbar_height() -> Float`

height reserved for a toolbar below the menu bar (0 by default).

### `fn dock_tab_bar_height() -> Float`

height of a leaf's tab bar.

### `fn splitter_width_const() -> Float`

width of a splitter between panes.

### `fn splitter_hit_extra() -> Float`

extra hit-test padding around a splitter.

### `struct DrawRect`

one rectangle to draw in the given color.

### `struct TextLabel`

one text label to draw. Optional features are gated by `has_*` flags / OptColor;
`styled_runs` empty means a single solid color.

### `fn text_label(left: Float, top: Float, text: Str) -> TextLabel`

a plain text label at (left, top) with the renderer's default color/font.

### `fn text_label_clipped(left: Float, top: Float, text: Str, clip: Rect) -> TextLabel`

a text label clipped to `clip` (e.g. a tab's text area).

### `struct MenuRect`

a menu-bar item's index and rect.

### `struct TabRect`

a tab's (path, index, rect).

### `struct LeafBtnRect`

a leaf button's (path, rect).

### `struct DockLayoutResult`

the full output of layout_dock_ui: draw lists + hit-test rects + splitter infos.

### `struct DockHit`

a dock hit-test result. `kind`: -1 none, 0 Menu, 1 Splitter, 2 SplitterExpand,
3 Tab, 4 TabClose, 5 LeafCollapse, 6 LeafClose. `path`/`index` are meaningful per kind.

### `fn dock_hit_none() -> DockHit`

a "no hit" DockHit.

### `struct TabDropTarget`

a tab drop target. `kind`: 0 Append, 1 Split. For Split, `direction`/`new_pane_is_first` apply.
kind: 0 = append to the target leaf, 1 = split that leaf, 2 = split the whole surface (an

### `struct DropResolve`

a drop-target resolution: present flag + leaf path + target.

### `fn content_rect_for_leaf(tree: Tree, path: Vec<Int>, tab_bar_height: Float) -> RectOpt`

the content rect (below the tab bar) of the leaf at `path`; ok=false if not a leaf / no height.

### `struct RectOpt`

a present flag + rect.

### `fn splitter_hit_rect(split_rect: Rect) -> Rect`

expand a splitter rect horizontally for easier hit-testing.

### `fn panel_base_id(id: Str) -> Str`

`term.panel#2` -> `term.panel`; anything without an instance marker is returned unchanged.

The same rule `plugin/host.nori` resolves ids by, stated in both places: the dock makes
these ids and the host resolves them, and the two modules do not import each other. Two lines
duplicated is cheaper than a dependency between the layout and the plugin loader, and the `#`
marker is fixed by both comments.

### `fn dock_leaf_tab_ids(state: DockState, surface_index: Int, leaf_path: Vec<Int>) -> Vec<Str>`

the tab ids in one leaf, in the order they are drawn.

### `fn DOCK_TAB_CLOSE() -> Str`

the action string a tab menu row carries.

### `fn dock_tab_menu(ntabs: Int, duplicable: Bool) -> Menu`

the rows a right-click on a tab shows.

`duplicable` comes from the panel's own declaration: a second view of a file tree is a second
picture of the same thing, while a second terminal is a second shell. Only the panel knows which
it is, so only the panel may offer it.

### `fn dock_next_instance(state: DockState, id: Str) -> Str`

the id a duplicate of `id` should take: the next instance nobody is using.

`term.panel` -> `term.panel#2` -> `term.panel#3`. The number is not a count of anything, it is a
name: closing `#2` and duplicating again gives `#2` back, which is what a person expects from
something they think of as "another one".

### `fn dock_tab_apply(inout state: DockState, surface_index: Int, leaf_path: Vec<Int>, tab_index: Int, action: Str) -> Str`

perform a tab menu action. Returns the id of any tab that was created, or "".

The caller needs that id: a new tab is a panel the host has never rendered, and the shell has to
make it the active one so that duplicating something puts you in the copy rather than leaving you
looking at the original wondering whether anything happened.

### `fn tab_font_size() -> Float`

the font a dock tab's title is drawn in.

### `fn tab_close_w() -> Float`

how wide the close box is, and the smallest tab that can hold one and still show a title.

Below this a tab is just a title. A close box drawn unconditionally at `tw - 20` would, once
tabs were squeezed, sit on top of the name, and the title's clip floored at
twelve pixels would cut the name to a letter or two with the `x` printed over it.
Nothing is gained by a close box you cannot hit without hitting the tab.

### `fn tab_width_for(title: Str) -> Float`

the width a tab wants: its title, measured, plus padding and room for the close box.

Measured, not `len * 7`. Seven pixels a character is right for no font and wrong for every one:
too wide for `Chat`, too narrow for `Terminal 2`, and the error compounds because this number
decides both the width and where the title is clipped.

### `fn tab_title_fit(title: Str, w: Float) -> Str`

`title` shortened with an ellipsis until it fits `w` pixels.

A hard clip cuts a glyph in half and says nothing about what was lost; an ellipsis says the name
continues. Walks down a character at a time: a tab title is a few words, and the alternative is
a binary search over a measurement that is already cheap.

### `fn layout_dock_ui(state: DockState, surface_index: Int, view_width: Int, view_height: Int, theme: Theme, hovered_splitter_index: Int, has_hovered_menu: Bool, hovered_menu_index: Int, menu_items: Vec<Str>, exclude_content_fill_tabs: Vec<Str>) -> DockLayoutResult`

lay out the dock chrome for surface `surface_index` at view size (w,h); a tab draws the title the
dock was given for its id, or the id itself when it was given none.
`exclude_content_fill_tabs`: ids whose panels render custom content (skip the content fill).

### `fn dock_hit_test(cursor: Vec2, menu_rects: Vec<MenuRect>, splitter_infos: Vec<SplitterInfo>, tab_rects: Vec<TabRect>, tab_close_rects: Vec<TabRect>, leaf_collapse_rects: Vec<LeafBtnRect>, leaf_close_rects: Vec<LeafBtnRect>) -> DockHit`

hit-test the dock at `cursor`. Checks menu bar, then leaf close/collapse, tab close/tab, splitter.

### `fn splitter_index_at(cursor: Vec2, splitter_infos: Vec<SplitterInfo>) -> Int`

which splitter the cursor is over, as an index into `splitter_infos`, or -1 for none. The same
widened hit rect `dock_hit_test` uses, so the splitter that lights up is the one a press takes.

### `fn splitter_index_of_path(path: Vec<Int>, splitter_infos: Vec<SplitterInfo>) -> Int`

index of the splitter at `path`, or -1, for keeping the one being dragged lit once the pointer
has left its rectangle, which it does immediately on any real drag.

### `fn dock_outer_margin() -> Float`

how far in from the dock's outer edge a drop docks against the whole surface rather than the leaf
under the cursor. Wide enough to aim at, narrow enough that ordinary drops into a panel are
unaffected: only a drop pushed against the edge lands here.

### `fn dock_bounds(tree: Tree) -> RectOpt`

the rectangle the whole surface occupies: the union of its leaves. `layout_tree` fills the dock
rect with them, so this is that rect, without having to be told it again.

### `fn resolve_drop_target(cursor: Vec2, tree: Tree) -> DropResolve`

resolve a tab drop target: the outer edge of the dock docks against the whole surface, and
anywhere else targets the leaf under the cursor (center=append, quadrants=split).

The outer edge is why this is not just the leaf scan. Without it every drop splits one leaf, so a
panel could take half of a neighbour but could never become a full-height column beside
everything: drag a panel to the right-hand edge of a dock whose bottom strip spans the width and
the strip stays spanning it, because the dragged panel never entered its row at all. `kind: 2`
wraps the whole tree instead, which is the only target that makes every other panel give way.

### `fn drop_target_highlight(tree: Tree, path: Vec<Int>, target: TabDropTarget) -> RectOpt`

the highlight rect for a drop target on the leaf at `path` (for overlay drawing); ok=false if unknown.

### `struct DockDragResult`

outcome: 0 = nothing (not an active drag), 1 = re-docked in the tree, 2 = dropped outside any leaf
(the tab was removed from the tree; the host should place it, e.g. tear it into a window). `tab` is
the moved tab id for outcomes 1 and 2.

### `fn dock_drag_begin(tab: Str, path: Vec<Int>, index: Int, copy press: Vec2) -> DockDrag`

begin a tab drag (call when a tab is pressed; `press` is the cursor position at press).

### `fn dock_drag_update(inout d: DockDrag, cursor: Vec2, threshold: Float) -> DockDrag`

mark the drag as "really moving" once the cursor leaves a small threshold from the press point.

### `fn dock_drag_commit(inout ds: DockState, surface_index: Int, d: DockDrag, cursor: Vec2, dock_rect: Rect, splitter_width: Float) -> DockDragResult`

commit the drag on release: remove the tab from its source leaf (collapsing an emptied leaf),
re-lay-out, and re-dock it where the cursor is, or report outcome 2 if the cursor is over no leaf
(the tab is removed; the host tears it off). `dock_rect`/`splitter_width` re-lay-out the surface.

### `fn menu_row_height() -> Float`

row height of an ordinary menu item.

### `fn menu_separator_height() -> Float`

vertical space a separator row occupies (the rule itself is 1px, centred).

### `fn menu_panel_pad() -> Float`

padding above the first row and below the last.

### `fn menu_panel_min_width() -> Float`

the narrowest an open list may be.

### `fn menu_label_inset() -> Float`

left inset of a row's label.

### `fn menu_font_size() -> Float`

font size of a menu row's label.

### `fn menu_hit_id() -> Str`

the `HitRegion.id` every menu row carries, which is how a host tells a menu hit from a panel's.

### `fn menu_sub_hit_id() -> Str`

the hit id a submenu row carries. Distinct from an ordinary row so a click on it can be told
apart without re-deriving any geometry, which needs a view size the click handler does not
have.

### `fn menu_submenu_arrow() -> Str`

the glyph on a row that opens a submenu.

### `fn menu_text_color() -> Color`

label colour of an enabled row.

### `fn menu_disabled_text_color() -> Color`

label colour of a disabled row.

### `fn menu_separator_color() -> Color`

colour of a separator's rule.

### `struct MenuItem`

one row of an open menu. A `separator` row draws a rule and takes no hit; an `enabled: false`
row draws dimmed and takes no hit. `action` is the string handed back to the host on a click.
One row. `sub` is a nested menu opened by hovering the row, mutually recursive with `Menu`,
the same shape the dock's own Tree/Node pair uses, so nesting costs nothing structurally.

A row has either an action or a submenu. A row that did both would have to decide what a click
on it means, and every answer to that is a surprise to somebody.

### `fn menu_item(label: Str, action: Str) -> MenuItem`

a clickable row.

### `fn menu_submenu(label: Str, sub: Menu) -> MenuItem`

a row that opens `sub` when hovered, drawn with an arrow. Disabled when `sub` is empty: a
submenu with nothing in it is a dead end, and drawing an arrow onto one is a small lie.

### `fn menu_item_has_sub(m: Menu, i: Int) -> Bool`

true if `i` opens a submenu.

### `fn menu_item_sub(m: Menu, i: Int) -> view Menu`

the submenu on row `i`, or an empty menu.

### `fn menu_item_disabled(label: Str) -> MenuItem`

a row drawn dimmed and never clickable, for a command that exists but does not apply right now.

### `fn menu_check_item(label: Str, action: Str, checked: Bool) -> MenuItem`

a horizontal rule between two groups of rows.
a row with a tick box: `checked` says whether it is on now.

Sticky, because a set of checkboxes is a set. A menu that shut on the first tick would make
turning three things on three round trips through the same gesture, which is not a filter, it is
a punishment. The menu closes on the next click outside it, on Escape, or on an ordinary item.

### `fn menu_sticky_item(label: Str, action: Str) -> MenuItem`

an ordinary row that does not close the menu: "All", "None", anything that changes the list you
are looking at rather than acting and leaving.

### `fn menu_item_sticky(m: Menu, i: Int) -> Bool`

does acting on row `i` leave the menu open?

### `fn menu_item_checked(m: Menu, i: Int) -> Bool`

is row `i` ticked?

### `fn menu_index_of_action(m: Menu, action: Str) -> Int`

the row whose action is `action`, or -1. What a host has after a click: an action string and the
need to know whether it came from a sticky row.

### `fn menu_has_checks(m: Menu) -> Bool`

does this menu hold a tick box? If so every row is indented to leave the column clear, so labels
line up whether or not their own row can be ticked.

### `fn menu_check_inset(m: Menu) -> Float`

the width of the tick column, or 0 for a menu with no tick boxes.

### `fn menu_check_mark() -> Str`

the mark a ticked row carries.

### `struct Menu`

one top-level menu: the title drawn in the bar, plus the rows its list shows.

### `fn menu_dup(m: Menu) -> Menu`

a Menu of its own: every row and submenu copied.

### `fn menu_item_dup(it: MenuItem) -> MenuItem`

a MenuItem of its own (its submenu copied with it).

### `fn menu_new(title: Str) -> Menu`

an empty menu titled `title`.

### `fn menu_empty() -> Menu`

an empty menu, the "no submenu" value, so MenuItem needs no optional.

### `fn menu_add(inout m: Menu, it: MenuItem)`

append a row to `m`.

### `fn menu_item_count(m: Menu) -> Int`

how many rows a menu has, separators included; the index space `menu_row_rect` uses.

A Menu could be built but not read back, which is fine while only `menu_emit` consumes one, and not fine
the moment a host wants to say "click the row that says Open" without knowing the row height.

### `fn menu_item_label(m: Menu, i: Int) -> Str`

the label of row `i`, or "" for a separator or an index off the end.

### `fn menu_item_action(m: Menu, i: Int) -> Str`

the action of row `i`, or "" for a separator, a disabled row, or an index off the end.

### `fn menu_titles(menus: Vec<Menu>) -> Vec<Str>`

the bar titles of `menus`, what `layout_dock_ui`'s `menu_items` wants.

### `struct MenuBarState`

which top-level menu, if any, is open. The whole interaction state of a menu bar.

### `fn menu_bar_state_new() -> MenuBarState`

a closed menu bar.

### `fn menu_bar_close(inout st: MenuBarState)`

close the bar without acting (Escape, a click elsewhere, a window losing focus).

### `fn menu_bar_open_at(inout st: MenuBarState, index: Int)`

open menu `index`.

### `fn menu_bar_is_open(st: MenuBarState) -> Bool`

true while a list is showing.

### `fn menu_bar_open_index(st: MenuBarState) -> Int`

the open menu's index (meaningless while closed).

### `fn menu_panel_height(m: Menu) -> Float`

the height of `m`'s open list, padding included.

### `fn menu_panel_width(m: Menu) -> Float`

the width of `m`'s open list: the widest label, floored at `menu_panel_min_width()`.
Uses a 7px-per-character approximation, so layout here can
answer without a font.

### `fn menu_panel_rect(m: Menu, anchor: Rect, view_w: Float, view_h: Float) -> Rect`

the rect of `m`'s open list, anchored to its bar item.

It opens downward from the bar item's bottom edge. If the list would run off the bottom of the
viewport and there is room above the bar item, it flips up. If there is room in neither
direction it stays downward and overflows the bottom, because a list drawn off the top edge is
one nothing could ever reach; the dropdown's popup follows the same rule.
It is also nudged left so it cannot run off the right edge of the viewport.

### `fn menu_row_rect(m: Menu, panel: Rect, index: Int) -> Rect`

the rect of row `index` inside `panel`. Separator rows are counted, so an index here is an
index into `m.items`.

### `fn menu_overlay(menus: Vec<Menu>, st: MenuBarState, menu_rects: Vec<MenuRect>, theme: Theme, view_w: Float, view_h: Float, has_cursor: Bool, copy cursor: Vec2) -> ComputedOverlay`

the open menu's list, as a `ComputedOverlay` a host renders last (after the dock chrome and
after every panel overlay).

Every primitive and hit goes into the popup layer and is then flushed, so the returned overlay
is an ordinary flat list to any painter, with no painter change needed, while keeping the
escapes-a-clip / wins-the-hit-test properties the layer exists for.

Returns an empty overlay when the bar is closed, so a host can build and render it every frame

### `fn menu_emit(inout out: ComputedOverlay, menus: Vec<Menu>, st: MenuBarState, menu_rects: Vec<MenuRect>, theme: Theme, view_w: Float, view_h: Float, has_cursor: Bool, copy cursor: Vec2)`

the same list, emitted into an existing overlay's popup layer and left there unflushed, for a
host that composes the menu into a document overlay it is already building. This is the form
that survives an enclosing clip: `merge_clipped_overlay` copies `pop_prims`/`pop_hits` through
unchanged, and the eventual `overlay_flush_popups` puts them last.

### `fn menu_panel_emit(inout out: ComputedOverlay, m: Menu, panel: Rect, theme: Theme, has_cursor: Bool, copy cursor: Vec2)`

paint one open menu into `panel`: the rows, the separators, the hover, the hit regions.

Shared by the menu bar and by a context menu, which differ only in where the panel is put: under
a title in the bar, or at the pointer. Everything after that decision is the same list.

Into the popup layer, which is what lets a menu escape the clip of whatever it was opened over
and be hit-tested before it; see the note on ComputedOverlay's pop_prims.

### `struct ContextMenu`

an open (or closed) context menu, with the items it was opened on.

`path` is the chain of expanded submenu rows: empty for just the root, `[3]` while row 3's
submenu is showing, `[3, 1]` for a submenu of that. A chain rather than a single index because
nesting has no natural depth limit, and the geometry below is the same at every level.

### `fn context_menu_open(inout cm: ContextMenu, copy at: Vec2, m: Menu)`

open `m` at `at` (view coordinates). An empty menu opens nothing: a right-click on something
with no actions does nothing rather than flash an empty box.

### `fn context_menu_replace(inout cm: ContextMenu, m: Menu)`

swap the items of an open menu, keeping where it is and how far into its submenus you are.

What a sticky row needs: ticking a box changes the host's state, so the list has to be rebuilt to
show the new tick, and reopening it would move it back to the pointer and collapse any submenu
the user had walked into. This is the same menu with new contents.

### `fn context_menu_rect(cm: ContextMenu, view_w: Float, view_h: Float) -> Rect`

where the panel lands: at the pointer, pulled back inside the view when it would not fit.

The anchor is a zero-height rect at the cursor, so `menu_panel_rect` puts the panel's top edge
exactly there and flips it above the pointer near the bottom of the window, the same placement
the menu bar gets for free.

### `fn menu_child_rect(parent: Rect, row: Rect, sub: Menu, view_w: Float, view_h: Float) -> Rect`

where a submenu goes: beside its parent row, flipped to the other side when it will not fit.

Top-aligned with the row rather than the panel, which is what makes a chain of submenus read as
coming out of the rows you hovered instead of as a stack of panels.

### `fn context_menu_panels(cm: ContextMenu, view_w: Float, view_h: Float) -> Vec<Rect>`

the panel rect of every level currently showing: [0] is the root, then one per expanded row.

### `fn context_menu_at_depth(cm: ContextMenu, d: Int) -> view Menu`

the menu shown at depth `d`: the root at 0, then down the open path.

### `fn context_menu_hover(inout cm: ContextMenu, copy cursor: Vec2, view_w: Float, view_h: Float)`

follow the pointer: expand the submenu it is resting on, and collapse the levels it has left.

The rule that matters is what happens when the cursor is over nothing, between two panels or
off the menu entirely. The path is left alone, because the gap between a row and its submenu is
the space you cross to reach it, and a menu that collapsed there could not be used at
all with a mouse that does not move in perfect straight lines.

### `fn context_menu_overlay(cm: ContextMenu, theme: Theme, view_w: Float, view_h: Float, has_cursor: Bool, copy cursor: Vec2) -> ComputedOverlay`

the overlay for an open context menu; an empty one when it is closed.

### `fn context_menu_click(inout cm: ContextMenu, ov: ComputedOverlay, copy cursor: Vec2) -> MenuClick`

route a click while a context menu is open.

`consumed` is true for any click while it is open, including one that lands outside it: that
click's job was to dismiss the menu, and letting it also reach whatever is underneath is how a
menu ends up selecting something on the way out.

### `struct MenuClick`

what a mouse press did to the menu bar. `consumed` means the bar took the click and the host
must not also offer it to the dock or a panel; `action` is the acted item's string ("" if none).

### `fn menu_bar_click(inout st: MenuBarState, menus: Vec<Menu>, menu_rects: Vec<MenuRect>, ov: ComputedOverlay, copy cursor: Vec2) -> MenuClick`

route a mouse press at `cursor` through the menu bar, given the overlay `menu_overlay` produced
for this frame.

Order is the behaviour: an open list is tested first (its rows sit over the dock), then the bar
itself (the open menu's own title toggles it shut, another title switches to it), and finally
any other click closes an open menu without acting. A press with nothing open and nothing on
the bar is not consumed, so the host's normal dock/panel routing runs unchanged.

### `fn menu_bar_index_at(menu_rects: Vec<MenuRect>, copy cursor: Vec2) -> Int`

the index of the bar item under `cursor`, or -1. Feeds `layout_dock_ui`'s hover highlight.

### `fn menu_bar_hover(inout st: MenuBarState, menu_rects: Vec<MenuRect>, copy cursor: Vec2)`

move an open menu to whatever bar item the pointer is over, which makes a row of menus feel
like one menu bar rather than N independent dropdowns. A no-op while the bar is closed, so a
host can call it every frame with the current cursor.




### `fn set_focused_widget(id: Str)`

tell this image of the library which widget id the host has focused.

Scoped to one document: set it before laying that document out and clear it after. The same
arrangement as `set_caret_on` (caret_blink.nori) and `set_char_advances` (text_measure.nori):
a plugin `.so` has its own image of this module and its own copy of this global.


### `fn ui_media_set_extra_roots(roots: Vec<Str>)`

set the two host path roots used to resolve `image { path: ... }` strings.
`project_root` "" = the process working directory; `save_dir` "" = no save directory.
further directories an image path may be found under, tried after the project root, in order (a desktop
shell gives the XDG data directories, so a document can name an application's icon as
`icons/hicolor/48x48/apps/foot.png`); none by default. The same rules apply (relative paths only, no `..`).

### `fn ui_media_project_root() -> Str`

the currently configured project root ("" = the process working directory).

### `fn ui_media_save_dir() -> Str`

the currently configured save directory ("" = none).

### `fn resolve_image_png_path(requested: Str) -> StrOpt`

resolve a layout `path` string to an on-disk PNG. `ok = false` means "draw nothing".

The path goes through these steps, in order:

1. sanitize      - `sanitize_overlay_image_path`: trim, then reject empty, any `\`,
                   any `..`, a leading `/`, or a drive letter (byte 1 == ':').
2. thumb lookup  - only when the path contains no `/`; re-checked with
                   `sanitize_save_thumb_filename`, then probed as
                   save_dir/thumbs/<name> and then project_root/thumbs/<name>. First file wins.
3. extension     - PNG only, ASCII-case-insensitive. Anything else resolves to nothing.
4. `./` strip    - repeatedly.
5. direct probe  - project_root/rel on the filesystem.

There is no virtual-filesystem step: an asset that exists only behind a host callback resolves
to nothing (`ok = false`).

### `fn pp_image(rect: Rect, path: Str) -> PaintPrimitive`

a filled-image paint primitive: kind 3, the resolved absolute path in `text`, and the placed
rect. Reusing `text` is what keeps PaintPrimitive unchanged.

### `struct UiMediaImage`

a decoded overlay image. `px` is `w*h*4` RGBA bytes owned by the cache: never free it, and
never hold it across a call that could evict (blit and drop it).

### `fn ui_media_cache_len() -> Int`

how many images the decode cache currently holds (for tests and diagnostics).

### `fn ui_media_cache_clear()`

drop every cached image, freeing the pixel buffers.

### `fn ui_media_image_load(path: Str) -> UiMediaImage`

decode `path` (a resolved absolute/cwd-relative PNG) through the cache.

`read_file` aborts the process on a missing or unreadable path: it is a trapping
builtin, not a Result-returning one, so the stat below is a guard, not an optimisation, and
the same stat supplies the cache key's length and mtime.

### `fn ui_media_is_svg(path: Str) -> Bool`

is `path` an SVG (by its extension)?

### `fn ui_media_svg_load(path: Str, w: Int, h: Int, r: Int, g: Int, b: Int) -> UiMediaImage`

an SVG rasterized at `w` x `h` device pixels with `currentColor` (r, g, b), through the same cache
as the PNGs (keyed by path, size and colour; a changed file is drawn again). An icon is drawn at the
size it is shown at (crisp at any scale) rather than a bitmap stretched.

### `fn ui_media_letterbox(panel: Rect, src_w: Int, src_h: Int) -> Rect`

the letterbox-contain fit: `scale = min(panel_w/src_w, panel_h/src_h)`, blit size `src*scale`
(floored at 1.0 per axis), centered in `panel`, including
the `.max(1.0)` on the panel extents that keeps a zero-width rect from producing a NaN scale.


### `struct DslP`

parser state: a std/parse Scanner cursor + a sticky error (Nori has no exceptions). Line/column for
error messages are computed on demand from the scanner offset (parse::line_col).

`file` names the source in errors and is what its imports are relative to ("" for a string with
no file: then it cannot import). `cx` is what a parse carries from file to file: the components
it knows, the files it has read; it is moved into the parser of an imported file or a
component's body and back, never copied. `params`/`args`: inside a component body, its
parameters and the values this use of it gave them; `slot`: the children the use gave it.

### `struct DslParseResult`

the strict parse result: on failure, `err` is "L:C: message",
with the parts in eline/ecol/emsg; src_empty marks a blank source.
`efile`: the file the error is in ("" for the source itself when it has no name; an import's
or a component's error names the file it is in); `deps`: every file the parse read (its
imports, transitively), which is what hot reload watches.

### `fn dsl_parse_document(src: Str) -> DslParseResult`

parse a `.ui` document: an optional `document { ... }` wrapper, else top-level
`style:` / `stylesheet { }` / root-node fields in any order (exactly one root).
Strict: unknown elements/attributes/properties and malformed values are errors with line:col;
a blank source is the distinct `src_empty` error.

### `fn dsl_parse_document_file(src: Str, file: Str) -> DslParseResult`

dsl_parse_document for a source that is the file `file`: errors name it, and its `import`s are
read relative to its directory.

### `struct DslSheetResult`

the strict stylesheet-only entry for a standalone `.style` source: the legacy tag/class sheet
(`sheet`, what the immediate path draws with) and everything the source says (`set`, which a
retained document compiles; see layout/sheet.nori).




### `fn field_pad() -> Float`

how far a field's text is inset from its box, each side.

### `fn field_scroll_x(text: Str, caret: Int, field_w: Float, size_px: Float, focused: Bool) -> Float`

how far the value is scrolled left, in pixels, so that `caret` is inside a field `field_w` wide.

The paint and the hit test both call this. A click has to land on the character it is over, so
turning an x back into an index must undo the shift the drawing applied. Two copies of
this would put the caret a few characters off, which reads as an inaccurate pointer rather than
as a bug.

An unfocused field is never scrolled: it shows its beginning, which is what identifies it, rather
than wherever its caret was left.

(Left-to-right text. field_origin is the general form: a value in a right-to-left paragraph
starts at the field's right edge and scrolls the other way.)

### `fn field_origin(text: Str, caret: Int, field_w: Float, size_px: Float, focused: Bool, dir: Int) -> Float`

where the value's line starts, in pixels right of the field's inner left edge (negative when it
is scrolled), for a value whose paragraph runs `dir` (dir_ltr / dir_rtl / dir_auto) with the
caret at byte `caret`.

A left-to-right value starts at the left and scrolls left to keep the caret in view. A
right-to-left one starts at the right (its first character is the rightmost) and when it is
wider than the field shows its end at the right and scrolls right as the caret goes left.

### `fn field_origin_st(text: Str, caret: Int, field_w: Float, sty: TextMeasureStyle, focused: Bool, dir: Int) -> Float`

field_origin for a value measured with `sty` (its size and its font stack: a field in its own face)

### `fn field_hit(text: Str, caret: Int, x: Float, field_w: Float, size_px: Float) -> Int`

the byte offset nearest to `x`, measured from the field's left edge, which is what a press
event carries, so a caller never has to know where its panel is on the screen.

### `fn field_hit_dir(text: Str, caret: Int, x: Float, field_w: Float, size_px: Float, dir: Int) -> Int`

field_hit for a value whose paragraph runs `dir`: the nearest caret stop on screen, always
between grapheme clusters, and through mixed directions to the cluster drawn under the pointer.

### `fn field_hit_st(text: Str, caret: Int, x: Float, field_w: Float, sty: TextMeasureStyle, dir: Int) -> Int`

field_hit_dir for a value measured with `sty`: the field's own face, as its paint measures it

### `struct FieldEdit`

a single-line editor's state: the value, the caret, and where a selection started.

`anchor == caret` means no selection, which is what a field that has never been dragged in
holds, so the common case costs nothing to represent and nothing to draw.

### `fn field_set(inout f: FieldEdit, text: Str)`

replace the whole value, putting the caret at the end and dropping any selection.

### `fn field_insert(inout f: FieldEdit, s: Str)`

insert `s`, replacing the selection if there is one.

### `fn field_place(inout f: FieldEdit, i: Int, sel: Bool)`

the caret at `i`, extending the selection or dropping it.

### `fn field_select_word(inout f: FieldEdit, i: Int)`

select the word under `i`, which is what a double-click means everywhere.

### `fn field_takes(sym: Int, mods: Int) -> Bool`

would `field_key` act on this keystroke?

A field is not always the only thing listening. A file tree's search box can share its keyboard
with the tree: typing a few letters and pressing Down to walk the matches is the point of
it, so Up, Down and Enter have to fall through while Left, Right, Home, End, Backspace and
Delete do not. A caller that cannot ask which is which ends up hard-coding the list a second
time, and the second copy is the one that goes stale.

### `fn field_key(inout f: FieldEdit, sym: Int, mods: Int) -> Str`

a keystroke. Returns "" for most, "copy" or "cut" when the caller should put `field_sel_text` on
the clipboard (this module cannot reach it: the clipboard belongs to whatever
owns the window).

`mods` is the shell's mask: 1 shift, 2 ctrl, 4 alt.

### `fn field_attrs(f: FieldEdit) -> Str`

`caret: N sel_anchor: N` for this field, as a `.ui` fragment. A panel needs both of these
attributes and they are easy to emit inconsistently, so they are produced together here.

### `fn field_pointer(inout f: FieldEdit, action: Str, id: Str, size_px: Float) -> Bool`

apply a pointer action the shell addressed to a text widget: `press:<id>:<x>:<y>:<w>:<h>:<clicks>:<mods>`
or `dragto:` in the same shape. True when it was one of those, for this `id`, and it was applied.

The shell sends these for any text widget, and a panel that ignores them leaves the pointer
unable to select text. The code editor reads the same two events.

A double click selects the word and a triple selects the line, which here is everything. Shift
with a press extends from where the caret already is, the way it does in every other field.


### `struct Xf`

p' = (sx * x + tx, sy * y + ty).

### `fn xf_then(first: Xf, second: Xf) -> Xf`

`first`, then `second`: second(first(p)).

### `fn xf_unpoint(x: Xf, p: Vec2) -> Vec2`

the inverse map of a point (a zero scale maps everything to the origin it collapsed onto).

### `fn xf_rect(x: Xf, r: Rect) -> Rect`

the image of `r`, normalised so min <= max whatever the sign of the scale.

### `fn xf_len(x: Xf, v: Float) -> Float`

a length scaled by the transform (the mean of the two axes: a radius or a font size has one).

### `fn node_has_fx(node: UiNode) -> Bool`

true when `node` asks for any of the three effects.

### `fn node_fx_xf(node: UiNode, rect: Rect) -> Xf`

the node's own transform for a node laid out at `rect`: scale about the rect's centre, then translate.

### `fn fx_opacity(v: Float) -> Float`

clamp an opacity to 0..1.

### `fn pp_apply_xf(inout p: PaintPrimitive, x: Xf)`

move and scale one primitive.

### `fn pp_apply_opacity(inout p: PaintPrimitive, op: Float)`

fade one primitive: every colour it draws with (an image's is the alpha it is blitted at).

### `fn overlay_apply_fx(inout ov: ComputedOverlay, x: Xf, op: Float)`

apply a transform and an opacity to everything an overlay holds: primitives (and the popup
layer's), hit regions, surfaces and scroll viewports. Hit regions move with what they hit, as
they do under a CSS transform.

### `fn overlay_append(inout out: ComputedOverlay, sink src: ComputedOverlay)`

move everything `src` holds onto the end of `out`, layer by layer.


### `struct FindMatch`

one find match: [start, end) in character offsets.

### `struct MultilineEditState`

the per-field editor state.

### `fn me_hist_cap_bytes() -> Int`

how much history one buffer keeps, in bytes of stored text.

A node is a whole copy of the text, so the size of the history is the size of the file times
the number of edits. A count would give a 1 KB file a thousand states and a 1 MB file the same
thousand, which is four hundred megabytes for one buffer. A byte budget gives each file as many
states as it can afford.

### `fn me_hist_count(state: MultilineEditState) -> Int`

how many nodes the tree has, pruned ones included (they keep their numbers).

### `fn me_hist_cur(state: MultilineEditState) -> Int`

which node the buffer is at.

### `fn me_hist_parent(state: MultilineEditState, i: Int) -> Int`

node `i`'s parent, or -1 when it is a root or its parent was pruned.

### `fn me_hist_label(state: MultilineEditState, i: Int) -> Str`

what made node `i`.

### `fn me_hist_name(inout state: MultilineEditState, label: Str)`

name the node the buffer is at ("chat: turn 7", say) so a history can be read.

### `fn me_hist_text(state: MultilineEditState, i: Int) -> Str`

the text at node `i`, or "" when it has been pruned.

### `fn me_hist_live(state: MultilineEditState, i: Int) -> Bool`

does node `i` still have its text?

### `fn me_hist_children(state: MultilineEditState, i: Int) -> Vec<Int>`

every child of node `i`, oldest first. A node with two is a fork: one branch you went back on and
one you took instead, and both are still there.

### `fn me_goto(inout state: MultilineEditState, i: Int) -> Bool`

put the buffer at node `i`. False when there is no such node, or its text has been pruned.

This is what a rewind is: one buffer, one node. Everything else (undo, redo, reverting what a
tool wrote) is this with the node worked out differently.

### `fn me_line_col_at(text: Str, idx: Int) -> (Int, Int)`

(line, col) of offset `idx`.

### `fn me_line_start(text: Str, line: Int) -> Int`

the offset where `line` starts (text length when past the last line).

### `fn me_line_end_index(text: Str, idx: Int) -> Int`

the offset just past the last char of the line containing `idx` (before its newline).

### `fn me_line_start_index(text: Str, idx: Int) -> Int`

the offset where the line containing `idx` starts.

### `fn me_line_count(text: Str) -> Int`

total line count (a trailing newline still counts its empty last line).

### `fn me_caret_adjacent_line(text: Str, idx: Int, delta: Int) -> Int`

the caret moved to the adjacent line (delta -1 up / +1 down), clamped, keeping the column.

### `fn me_word_bounds(text: Str, idx: Int) -> (Int, Int)`

the [start, end) of the word around `idx` (empty range on a non-word char).

### `fn me_selection(state: MultilineEditState) -> (Int, Int)`

the ordered selection range [start, end); start == end when nothing is selected.

### `fn me_selected_text(state: MultilineEditState) -> Str`

the selected text ("" when the selection is empty).

### `fn me_select_all(inout state: MultilineEditState)`

select everything (caret at the end).

### `fn me_select_word(inout state: MultilineEditState)`

select the word at the caret.

### `fn me_insert(inout state: MultilineEditState, s: Str)`

insert `s` at the caret, replacing the selection; caret lands after the insertion.

### `fn me_backspace(inout state: MultilineEditState)`

delete the selection, or the char before the caret.

### `fn me_delete(inout state: MultilineEditState)`

delete the selection, or the char after the caret.

### `fn me_insert_newline(inout state: MultilineEditState)`

insert a newline, auto-indenting with the current line's leading whitespace.

### `fn me_insert_tab(inout state: MultilineEditState)`

insert spaces to the next tab stop (tab_size).

### `fn me_set_text(inout state: MultilineEditState, text: Str)`

replace the whole buffer (host seeding); resets caret/selection/undo.

### `fn me_undo(inout state: MultilineEditState)`

undo the last edit (text snapshot; caret clamps).

### `fn me_redo(inout state: MultilineEditState)`

redo the last undone edit, down the branch you last came up from.

The other branches are still there. Typing after an undo forks the history rather than
deleting the redo stack: `hlast` points at the newer branch, and the older one stays reachable
through `me_hist_children` and `me_goto`.

### `fn me_para_dir(state: MultilineEditState) -> Int`

the direction every line of the box runs (dir_ltr or dir_rtl): `state.dir`, or for dir_auto the
first strong character of the whole text (HTML's dir=auto on a text area), so one Arabic message
is right-aligned and reordered as a whole, not line by line.

### `fn me_find_next(text: Str, query: Str, frm: Int, case_sensitive: Bool) -> FindMatch`

the next literal match at/after `frm`.

### `fn me_find_prev(text: Str, query: Str, frm: Int, case_sensitive: Bool) -> FindMatch`

the previous literal match starting at or before `frm`.

### `fn me_find_all(text: Str, query: Str, case_sensitive: Bool) -> Vec<FindMatch>`

every literal match, left to right (non-overlapping).

### `fn me_hit_to_caret(state: MultilineEditState, x: Float, y: Float, content: Rect, size_px: Float) -> Int`

the caret offset for a click at (x, y) inside `content` (scroll-adjusted).

### `fn me_caret_pos(state: MultilineEditState, content: Rect, size_px: Float) -> Vec2`

the caret's pixel position (top of its line) inside `content`, scroll-adjusted.

### `struct MultilinePaintOpts`

paint options; multiline_paint_opts() gives the code-editor defaults.

### `struct MeDiag`

one diagnostic as the editor was told it: 1-based line and column, the length of the span it is
about, and 1 for an error / 2 for a warning.

### `fn me_parse_diags(spec: Str) -> Vec<MeDiag>`

`LINE:COL:LEN:SEV|...`, all 1-based except SEV. A malformed entry is dropped rather than guessed
at: an underline in the wrong place points at innocent code and is worse than none.

### `fn me_parse_perline(spec: Str) -> Vec<Str>`

`L:text|L:text` into one entry per line, 0-based, "" where a line has nothing.

A sparse thing written sparsely. A file has thousands of lines and a handful of annotations, so
the caller sends the handful and this spreads them; the alternative is a plugin building a
string with one empty field per line of the file, across an ABI that only carries strings.

### `fn me_rail_width(size_px: Float) -> Float`

The rail has a column of its own, reserved inside the gutter.

A rail drawn in the padding the gutter already had would be wrong in the plainest possible
way: with a one-digit gutter the padding is where the number is, so a lifeline would be painted
straight through `3`, `4`, `5` and neither could be read.

The gutter's width feeds `code_editor_content_rect`, which is what the caret is positioned
against, so widening the paint without widening the measurement puts the caret a column off.
The width is therefore added in one place: `me_gutter_width` is what every caller asks, the
paint and the content rect included, so a column added there is a column everybody already
agrees about.

Reserved whenever the numbers are on, whether or not anything is in it. A gutter that changes
width when a diagnostic appears would slide the whole document sideways under the pointer.

### `fn me_gutter_width(total_lines: Int, size_px: Float, line_numbers: Bool) -> Float`

the gutter width for `total` lines at `size_px` (digits + padding), 0 when line numbers are off.

### `fn layout_multiline_edit(state: MultilineEditState, rect: Rect, size_px: Float, focused: Bool, opts: MultilinePaintOpts) -> Vec<PaintPrimitive>`

lay the editor out into `rect`: gutter numbers, current-line highlight, per-line selection rects,
visible line text (virtualized with 5 lines of overscan), and the caret when `focused`.


### `fn ui_kind_is_ext(kind: Int) -> Bool`

true for the kinds whose measure/layout lives in a family file rather than in
runtime_document.nori's own if/else chains.


### `fn parse_error_summary(res: DslParseResult) -> Str`

one-line summary suitable for logs or window titles (Display for the error).

### `fn parse_error_lines(res: DslParseResult) -> Vec<Str>`

the multi-line body for the in-panel error overlay (title + message + source hint).

### `fn paint_parse_error(inout ctx: Context, bounds: Rect, res: DslParseResult)`

paint a diagnostic card inside `bounds` (panel-local coordinates); geometry and
palette are fixed.


### `struct PickerItem`

one candidate. `label` is what is matched and shown first (a basename, a
command); `detail` is the dimmer context after it; `note` a short right-hand
marker (a git letter, a tab index). `action` is the host's business.

### `fn picker_item_dup(it: PickerItem) -> PickerItem`

a PickerItem of its own.

### `fn picker_items_dup(v: Vec<PickerItem>) -> Vec<PickerItem>`

a fresh list of item copies.

### `struct PickerHit`

a candidate that survived the query: which item, how well, and where it
matched, so the matched characters can be lit up.

The runs are indices into `label + " " + detail`, the string the match was
actually run against: a path typed as `fltree/lib` matches across the join,
and highlighting only the basename would leave half the match invisible.

### `struct Picker`

an open (or closed) picker.

`items` is everything the host offered; `hits` is what survived the query,
in rank order. `sel` indexes `hits`, not `items`; the selection follows the
list you can see.

`scope` is a narrowing that is not part of the query.

A picker that can answer for more than one kind of thing needs a way to say which, and typing
the name of the kind into the same box the query lives in means the query no longer matches
anything. The scope is held apart: it is drawn as `name::` before the caret, it is not matched
against, and backspacing out of an empty query pops it. The host decides what the scopes are and
what rows each one holds; this only carries and shows it.

### `fn picker_scope(p: Picker) -> view Str`

the scope this picker is narrowed to, or "".

### `fn picker_open(inout p: Picker, kind: Str, items: Vec<PickerItem>)`

open `kind` over `items`. An empty list opens nothing: a picker with nothing
to pick is a box you have to dismiss.

### `fn picker_rows() -> Int`

how many rows the list shows at once.

### `fn picker_row_h() -> Float`

the height of one row.

### `fn picker_prompt_h() -> Float`

the height of the prompt.

### `fn picker_font() -> Float`

the font the list is drawn in.

### `fn picker_preview_font() -> Float`

the font the preview is drawn in: smaller, because it is context and not the thing you are
choosing.

### `fn picker_rect(view_w: Float, view_h: Float) -> Rect`

where the box goes: centred, and a fixed share of the view.

A share rather than a size, because the same picker has to be usable on a
laptop and on a monitor, and a box that is 800px on both is either most of
the first or a stamp on the second.

### `fn picker_list_rect(p: Picker, box: Rect) -> Rect`

the list occupies the left of the box when there is a preview, all of it otherwise.

### `fn picker_preview_rect(p: Picker, box: Rect) -> Rect`

and the preview the right.

### `fn picker_row_rect(p: Picker, box: Rect, i: Int) -> Rect`

the rect of visible row `i`; 0 is the topmost row drawn, not hit 0.

### `fn picker_set_query(inout p: Picker, q: Str)`

replace the query outright, which is what a host does after it has taken a `scope::` prefix out of it.

### `fn picker_set_scope(inout p: Picker, scope: Str, items: Vec<PickerItem>)`

narrow to `scope` over `items`, keeping the picker open and starting a fresh query.

The rows come from the host because only the host knows what a scope means. An empty list is
still accepted here, unlike `picker_open`: a scope that currently matches nothing is a state you
can back out of with one backspace, not a reason to close the picker under the person's hands.

### `fn picker_scope_pop(inout p: Picker) -> Bool`

drop the last segment of the scope, and say whether there was one to drop.

A scope is a path (`preference::editor`), so backing out of it goes one level at a time, the way
backing out of anything typed with a `::` in it should. The host has to refill the rows
afterwards, which is why this answers rather than doing it: this module has no idea what any of
those names mean.

### `fn picker_move(inout p: Picker, delta: Int)`

move the selection, clamped, keeping it inside the window of drawn rows.

### `fn picker_select_visible(inout p: Picker, i: Int)`

select the visible row `i`, which is what a click does.

### `fn picker_selected(p: Picker) -> PickerItem`

the item under the selection, or an empty one.

### `fn picker_hit_item(p: Picker, i: Int) -> PickerItem`

the item at rank `i` of the surviving list, which is what a test reads.

### `fn picker_haystack(it: PickerItem) -> Str`

what a candidate is matched against and drawn as: the detail, then the label, which for a file
is the path in its natural order.

In that order because that is the order people type. `ftlib` means "something like lib in
something like file_tree", and a haystack of `lib.nori plugins/file_tree/src` cannot match it:
the `l` of `lib` comes before the `f` of `file_tree`, and a subsequence has to be in order.
Basename-first reads better in a list and is unusable as a target.

The row is drawn from this same string, so the matched positions line up with the characters
under them without anything having to be mapped between two spellings. There is one string
for both: two ways to find the same row would be two things to keep true, and the one on the
screen is the one the editor means.

### `fn picker_label_at(it: PickerItem) -> Int`

where the label starts inside the haystack; everything before it is context and is drawn dim.

### `fn picker_set_preview_image(inout p: Picker, of: Str, path: Str)`

a PNG to show instead of lines (see `preview_img`). "" goes back to text.

### `fn picker_set_preview(inout p: Picker, of: Str, lang: Str, lines: Vec<Str>)`

set the preview to `lines` of language `lang`. The host fills this when the selection changes;
the widget only draws it.

### `fn picker_preview_lines(p: Picker) -> view Vec<Str>`

the lines the preview is showing.

The host hands these in and could not read them back, so what the preview contained was
unobservable from outside: a test could say which file was previewed and never that the window
was around the right line. A picker that shows the wrong twenty lines looks exactly like one
that shows the right twenty.

### `fn picker_preview_rows() -> Int`

how many lines of preview fit.

### `fn picker_rerank(inout p: Picker)`

re-rank every item against the query, best first.

An empty query keeps the host's order, which is the order it thought was
useful (most-recently-opened, or the order of the menu bar). Sorting by a
score everything ties on would replace that with nothing.

### `fn picker_lower(s: Str) -> Str`

ASCII lower case. Enough for matching a filename, and it does not pretend otherwise.

### `fn picker_match(hay: Str, needle: Str, label_at: Int) -> PickerMatch`

does `needle` appear in `hay` as a subsequence, and how good is the fit?

Subsequence, not substring, which is the reason a picker is faster than
the file tree's search box: `fltree` has to find `plugins/file_tree/...`, and no
substring of that path is `fltree`. The tree's filter narrows a thing you are
reading and should be exactly predictable; a picker ranks things you are
jumping to, and ranking is what makes three keystrokes enough.

The score is four rules, and they are meant to be statable in one breath:

  * a character adjacent to the previous match is worth much more than a
    scattered one; that is what makes `fltr` prefer `file_tree` to a path
    with an f, an l, a t and an r sprinkled through it;
  * a character starting a word (after `/`, `_`, `-`, `.`) is worth more
    again, so typing initials works;
  * matching inside the label beats matching in the detail, because you
    usually mean the filename and not the folder it is in;
  * shorter candidates break ties, because a short path that matches is
    almost always the one meant.

Greedy left to right. A backtracking matcher would find a better alignment in
some cases; it would also make the score something you cannot predict from
looking, which for a list you steer by eye is the worse trade.

### `fn picker_run_has(p: Picker, i: Int, at: Int) -> Bool`

is index `at` of the haystack one of the matched characters of hit `i`?

### `fn picker_hit_id() -> Str`

the id every picker row's hit region carries.

### `fn picker_row_at(p: Picker, box: Rect, copy cursor: Vec2) -> Int`

which visible row is at `cursor`, or -1.

### `fn picker_visible_count(p: Picker) -> Int`

how many rows are actually drawn: the window, or what is left of the list.

### `fn picker_overlay(p: Picker, theme: Theme, view_w: Float, view_h: Float, has_cursor: Bool, copy cursor: Vec2) -> ComputedOverlay`

the picker as an overlay of its own, drawn after everything else. Its rows go into the
overlay's popup layer, which is what puts it over every panel and gets it hit-tested first.

The counterpart of `context_menu_overlay`, and here for the same reason: a host cannot build one
itself without naming `Theme`, which is private to this module.

### `fn picker_placeholder(kind: Str) -> Str`

what the empty prompt says. Called with the scope first and the kind second, so a narrowed
picker says what it is narrowed to; "" means "nothing to say about this one, ask about the kind".


### `fn ui_node_shallow_eq(a: UiNode, b: UiNode) -> Bool`

every field of `a` and `b` but their children is equal (generated from UiNode's field list:
a field added to UiNode must be added here, or a change to it will not be seen).

### `fn overlay_empty() -> ComputedOverlay`

an overlay with nothing in it and no input state.

### `fn overlay_dup(ov: ComputedOverlay) -> ComputedOverlay`

a deep copy of an overlay's output layers (the input state is not carried).

### `fn overlay_append_copy(inout out: ComputedOverlay, src: ComputedOverlay)`

copy every output layer of `src` onto the end of `out`.

### `fn pp_same(a: PaintPrimitive, b: PaintPrimitive) -> Bool`

two primitives draw the same thing.

### `fn pp_bounds(p: PaintPrimitive) -> Rect`

The screen area a primitive can change, as either painter draws it: generous rather than tight,
because a damage rect that is too small leaves a stale strip on the screen and one that is a
few pixels too big costs a few pixels. Empty (zero area) when nothing of it is drawn.
  fill:   its rect, plus the drop shadow the painters put under a rounded, opaque fill when
          shadows are on (paint_cpu: offset 3, blur 7; paint_gpu: shadow_rect). The CPU shadow
          ignores the primitive's clip, so its box is not clipped either.
  border: its rect.
  text:   from its top-left over the widest line and every line's height, padded by the outline,
          a quarter em for glyph overhang (italics, accents above the box) and a pixel.
  image:  its rect (the letterboxed blit lands inside it).
  box shadow: its box and the blurred shape, moved by its offset and grown by its blur.

### `fn damage_add(inout dmg: Vec<Rect>, r: Rect)`

add `r` to a damage list (nothing when it is empty).

### `fn damage_merge(dmg: Vec<Rect>, bounds: Rect, max_n: Int) -> Vec<Rect>`

merge a damage list into a few disjoint-ish rects: overlapping (or touching) rects are unioned
until nothing overlaps; past `max_n` rects everything collapses into their bounding box. Each
rect is clipped to `bounds` and snapped outward to whole logical pixels.

### `struct RNode`

one node of a retained document. See the header.

### `struct RIdState`

the per-id state a host sets on nodes, which survives every re-render of the document.

### `struct RDoc`

a retained document. Create with rdoc_new, feed it with rdoc_set_tree / rdoc_set_source and
the input setters, and call rdoc_frame once per frame.

### `fn rdoc_set_viewport(inout d: RDoc, w: Float, h: Float)`

the viewport (logical px). A change relays out the root and damages everything.

### `fn rdoc_pointer(inout d: RDoc, x: Float, y: Float, down: Bool)`

the pointer, in the same logical space as the viewport.

### `fn rdoc_pointer_leave(inout d: RDoc)`

the pointer left the surface.

### `fn rdoc_set_scroll(inout d: RDoc, id: Str, off: Float)`

a scroll region's offset (by its id, as rdoc_overlay(d).scrolls reports it).

### `fn rdoc_set_bind(inout d: RDoc, name: Str, value: Str)`

a bind variable (`bind_visible:` / `bind_disabled:`).

### `fn rdoc_add_class(inout d: RDoc, id: Str, cls: Str)`

give the node `id` the class `cls` (a state the host sets: `.urgent`, `.playing`), on top of the
classes its `.ui` gives it. Survives re-renders of the document.

### `fn rdoc_remove_class(inout d: RDoc, id: Str, cls: Str)`

take the class `cls` off the node `id`, including one its `.ui` gave it.

### `fn rdoc_set_text(inout d: RDoc, id: Str, text: Str)`

replace the text of the node `id` (until the next rdoc_set_text for it, whatever the `.ui` says).

### `fn rdoc_set_focus(inout d: RDoc, id: Str)`

which node has keyboard focus ("" for none): it gets the `focused` class and `:focus`.

### `fn rdoc_set_caret_on(inout d: RDoc, on: Bool)`

the blink phase of the carets in this document (caret_blink.nori): its focused fields are laid out again
when it changes, so only they are repainted. A document that is never told keeps its carets on.

### `struct RLayer`

one style source of a document: a theme, a plugin's `.style`, the document's own. `rank` orders
them (lower cascades first, so a higher one overrides); `path` is the file it was loaded from
("" when it was handed over as a value), and `deps` every file that went into it.

### `fn rdoc_set_styles(inout d: RDoc, name: Str, styles: StyleSet) -> UiError`

set the style layer `name` at rank 0 (see rdoc_set_styles_ranked).

### `fn rdoc_set_styles_ranked(inout d: RDoc, name: Str, rank: Int, styles: StyleSet) -> UiError`

set the style layer `name` (a theme, a plugin's `.style`, a user's overrides). Layers cascade by
`rank`, lower first: a theme at -1, the document's own styles at 0 (rdoc_load puts them
there), a user's overrides at 1; within a rank, in the order they were first set.
Replacing a layer keeps its place. They are compiled together, so a token one defines is used
by every other and the last definition wins. Every node is restyled; one whose drawn form
does not change costs nothing more.

A sheet that does not compile (an undefined token, a token loop) is refused: the error comes
back (file, line, column) and the document keeps the sheet it had, so a broken theme never
blanks the desktop.

### `fn rdoc_style_error(d: RDoc) -> UiError`

the last style error rdoc_set_styles refused a sheet for (`msg` empty when the sheet in use is
the one last set).

### `fn rdoc_load_styles(inout d: RDoc, name: Str, rank: Int, file: Str) -> UiError`

load a `.style` file (and the `.style` files it imports) as the layer `name` at `rank`. It is
watched: rdoc_poll reloads it when it or anything it imports changes. On an error the layer
keeps what it had.

### `fn rdoc_set_source(inout d: RDoc, src: Str, file: Str) -> UiError`

the document from `.ui` source text, which is what a plugin sends. `file` names it in errors and is what
its imports are relative to ("" for a source that imports nothing). The document is diffed
against the one there (rdoc_set_tree), and its own styles (its `stylesheet { }` block and the
styles it imports) become the layer "document" at rank 0.

Nothing changes on an error: a source that does not parse, an import that is missing or broken,
styles that do not compile, each comes back as file:line:col and the document on screen stays
the last good one. rdoc_error says what is wrong until a good source replaces it.

### `fn rdoc_load(inout d: RDoc, file: Str) -> UiError`

the document from a `.ui` file, watched for changes (rdoc_poll) with everything it imports.

### `fn rdoc_poll(inout d: RDoc) -> Bool`

Hot reload re-reads whatever changed on disk (the document's file, any file it imports, any
style layer's file or its imports) and applies it. A file that now has an error leaves the last
good version in place (rdoc_error says why). Two stats per watched file; returns true when
something was reloaded.

### `fn rdoc_watch_list(d: RDoc) -> Vec<Str>`

every file the document and its style layers were read from, for a host that watches them
itself (inotify), so that a quiet desktop polls nothing, and calls rdoc_reload when one changes.

### `fn rdoc_watch_dirs(d: RDoc) -> Vec<Str>`

the directories those files are in, once each, as os::watch_add takes (inotify watches
directories): watch them, and call rdoc_reload when os::watch_take says something changed.

### `fn rdoc_reload(inout d: RDoc)`

re-read every file the document and its style layers came from, changed or not. rdoc_poll's
stat stamp is size and mtime in whole seconds, so two saves of the same length within one
second look the same to it; a host told by the file system that a file changed calls this.

### `fn rdoc_error(d: RDoc) -> UiError`

what is wrong with the last source, style or file the document was given (`msg` empty when
nothing is: the document on screen is the last one it was given).

### `fn rdoc_set_tree(inout d: RDoc, sink tree: UiNode)`

replace the document's tree with `tree`, keeping every node that is still there.

Nodes are matched to the existing ones by `id` where they have one and by position among their
un-id'd siblings where they do not; a match of the same kind is diffed (its attributes compared,
its children matched in turn) and keeps its caches, its pointer state and its running
transitions, so a plugin that re-renders its whole document when one label changes costs the
parse and the compare, and lays out and repaints that label.

### `struct RFrame`

what a frame did. `changed` false: nothing to draw, and the caller can sleep (or keep the last
frame on screen): a document left alone answers this frame after frame at no cost.
`damage`: the logical-px rects that differ from the previous frame (merged, inside the
viewport); `full`: the whole viewport (the first frame, a resize).
`animating`: something is moving (a transition, an animation): ask for another frame at the
next vblank; false once everything has settled, and the document is idle again.
`hits_changed`: the hit regions differ from the last frame's, which can happen with nothing to draw (an
`on_click` given to a node that paints nothing): a host routing input re-reads rdoc_overlay then.

### `fn rdoc_frame(inout d: RDoc, now: Float) -> RFrame`

bring the document up to date with its inputs and report what changed on the screen.

### `fn rdoc_overlay(d: RDoc) -> view ComputedOverlay`

the document's current paint list, hit regions, surfaces and scroll regions, the same shape
layout_overlay returns, so every painter and hit test works on it unchanged.

### `fn rdoc_hit(d: RDoc, p: Vec2) -> HitRegion`

the front-most hit region under `p`.

### `fn rdoc_pending(d: RDoc) -> Bool`

true when a frame would do something (an input changed since the last rdoc_frame).

### `fn rdoc_node_count(d: RDoc) -> Int`

the number of nodes in the document.


### `struct UiLength`

length unit: 0 px, 1 percent (of parent inner), 2 vw, 3 vh, 4 ui (scale multiplier).

### `fn ui_px(v: Float) -> UiLength`

a pixel length.

### `fn ui_percent(v: Float) -> UiLength`

a percent-of-parent length.

### `fn ui_vw(v: Float) -> UiLength`

a viewport-width-percent length.

### `fn ui_vh(v: Float) -> UiLength`

a viewport-height-percent length.

### `fn ui_scale_len(v: Float) -> UiLength`

a ui-scale-multiplier length.

### `struct LayoutLengthContext`

length axis: 0 width, 1 height.

### `fn default_ui_scale(viewport_w: Float, viewport_h: Float) -> Float`

default UI scale: fit a 1280x720 reference, clamped to [0.5, 2.0].

### `fn length_context_root(viewport_w: Float, viewport_h: Float) -> LayoutLengthContext`

a root length context for a viewport (ui_scale auto-derived).

### `fn length_context_with_parent(ctx: LayoutLengthContext, inner_w: Float, inner_h: Float) -> LayoutLengthContext`

a copy of `ctx` with a new parent inner size (for resolving percent lengths of children).

### `fn ui_length_resolve(l: UiLength, ctx: LayoutLengthContext, axis: Int) -> Float`

resolve a UiLength to pixels on `axis` (0 width, 1 height) within `ctx`.

### `struct UiLengthOpt`

a present flag + UiLength.

### `fn ui_length_parse(s: Str) -> UiLengthOpt`

parse a length string ("50%", "10vh", "25vw", "300ui", "ui", "42"); ok=false if invalid.

### `fn parse_unit_num(s: Str, unit: Int) -> UiLengthOpt`

parse a trimmed numeric string with a unit tag into a UiLengthOpt (internal helper).

### `fn nui_lower(s: Str) -> Str`

lowercase ASCII (inline helper).

### `fn nui_ends_with(s: Str, suf: Str) -> Bool`

suffix test (inline helper).

### `fn cross_start() -> Int`

cross-axis alignment: 0 start, 1 center, 2 end, 3 stretch.

### `fn cross_center() -> Int`

cross-axis alignment: center.

### `fn cross_end() -> Int`

cross-axis alignment: end.

### `fn cross_stretch() -> Int`

cross-axis alignment: stretch to fill.

### `fn main_start() -> Int`

main-axis alignment: 0 start, 1 center, 2 end, 3 space-between.

### `fn main_center() -> Int`

main-axis alignment: center.

### `fn main_end() -> Int`

main-axis alignment: end.

### `fn main_space_between() -> Int`

main-axis alignment: space-between.

### `struct FlexGrowSlot`

a flex grow/shrink slot: which child index, its grow weight, and shrink factor.

### `fn distribute_flex_grow(inout sizes: Vec<Float>, slots: Vec<FlexGrowSlot>, remaining: Float)`

distribute `remaining` extra main-axis space across `slots` into `sizes` (by weight; equal if weightless).

### `fn apply_flex_shrink(inout sizes: Vec<Float>, slots: Vec<FlexGrowSlot>, deficit: Float)`

shrink `sizes` by up to `deficit` across `slots` weighted by size*shrink.

### `fn apply_flex_main_axis(inout sizes: Vec<Float>, slots: Vec<FlexGrowSlot>, inner_main: Float, fixed_sum: Float, gap_count: Int, gap: Float)`

grow or shrink `sizes` to fit `inner_main` given fixed sizes + gaps.

### `fn nk_column() -> Int`

node kind: vertical flex container.

### `fn nk_row() -> Int`

node kind: horizontal flex container.

### `fn nk_text() -> Int`

node kind: text leaf.

### `fn nk_button() -> Int`

node kind: button leaf.

### `fn nk_rect() -> Int`

node kind: colored rectangle leaf.

### `fn nk_spacer() -> Int`

node kind: empty spacer leaf.

### `fn nk_surface() -> Int`

node kind: reserved surface rectangle (host-drawn).

### `fn nk_scroll_column() -> Int`

a vertically-scrolling column (children flow at natural height; clipped to the viewport).

### `fn nk_scroll_row() -> Int`

a horizontally-scrolling row.

### `fn nk_image() -> Int`

node kind: image leaf (a resolved path blitted into the node rect).

### `fn nk_checkbox() -> Int`

node kind: checkbox leaf.

### `fn nk_slider() -> Int`

node kind: slider leaf (horizontal track + thumb).

### `fn nk_dropdown() -> Int`

node kind: dropdown leaf (closed trigger; the popup is host-drawn).

### `fn nk_number() -> Int`

node kind: numeric drag field, the `number` / `drag_value` element.

### `fn nk_text_input() -> Int`

node kind: single-line text field.

### `fn nk_text_area() -> Int`

node kind: multi-line text field.

### `fn nk_picker() -> Int`

node kind: value picker (a label + a "Browse..." field the host fills in).

### `fn nk_code_editor() -> Int`

node kind: syntax-highlighted code editor, `text_area`'s gutter-and-colours sibling.

Distinct from `text_area` on purpose. `text_area` is a plain multiline
field: no gutter, no colours, and its state rebuilt from `value:` on every layout.
`code_editor` turns the gutter and current-line highlight on, takes a `language:` to colour by,
and accepts `caret:`/`sel_anchor:`/`scroll_y:` so the caller can own the editing state across
frames, which is what makes it usable from a plugin that re-emits its document every frame.

### `fn nk_scrollbar() -> Int`

node kind: a scrollbar for content the plugin scrolls itself.

The other kind of scrolling is `overflow_y: scroll`, where this library owns the flow: it
measures the children, shifts them by an offset the host keeps, clips them, and reports the
region so the host can draw a bar over it. That is right for a settings page and wrong for a
transcript with ten thousand lines in it, where emitting every row into a document rebuilt each
frame is the expensive part and drawing a window of them is the whole design.

So this is the bar for the other case: the plugin says how much content there is, how much of it
is showing and where it is, and gets told when someone moves it. The three numbers are in the
plugin's own units (lines, rows, items, pixels) because the only arithmetic done with them is
a ratio, and the offset comes back in whatever they were.

### `struct UiNode`

an overlay node: a `kind` tag plus the union of layout-relevant fields and children.

### `fn ui_node_blank(kind: Int) -> UiNode`

a blank node of `kind` with default layout fields (internal builder base).

### `fn ui_tagged(sink node: UiNode, tag: Str, classes: Vec<Str>) -> UiNode`

tag `node` for stylesheet resolution: an explicit `tag` (empty = use the element kind) + `classes`.

### `fn ui_kind_tag(kind: Int) -> Str`

the canonical stylesheet tag for a node kind (column/row/text/button/rect/spacer/surface).

### `fn apply_style_to_node(inout node: UiNode, style: NodeStyle)`

apply a resolved NodeStyle's set fields onto `node`'s visual + text-layout fields (in place).

### `fn apply_style_to_span(inout sp: TextSpanRun, style: NodeStyle)`

apply a resolved NodeStyle onto one styled span (in place). A span's
`classes` layer over the parent text node's resolved style, and
only the four properties a TextSpanRun can carry are taken: size, colour, bold, italic.
`text_color` wins over `color`, matching apply_style_to_node's rule for a `text` node.

### `fn apply_stylesheet(inout node: UiNode, sheet: OverlayStyleSheet)`

cascade `sheet` over the whole tree: for each node resolve its style from (tag or kind) + classes,
then apply it, recursing into children.

### `struct NodeRuntimeState`

a runtime overlay for one node id, applied without re-parsing: optionally replace `classes`,
merge `add_classes`, and force disabled/hidden.

### `fn node_runtime_state_dup(s: NodeRuntimeState) -> NodeRuntimeState`

a NodeRuntimeState of its own.

### `struct OverlayRuntimeState`

id -> NodeRuntimeState (parallel vecs).

### `fn rs_set(inout state: OverlayRuntimeState, id: Str, ns: NodeRuntimeState)`

set (replace) the runtime state for `id`; a later set for the same id wins.

### `fn rs_clear(inout state: OverlayRuntimeState, id: Str)`

remove the runtime state for `id`.

### `fn apply_runtime_state(inout node: UiNode, state: OverlayRuntimeState)`

apply the runtime overlay to the tree: for each node whose id has an entry, replace classes when
has_classes, merge add_classes, and set hidden/disabled. Recursive; call before apply_stylesheet.

### `fn bind_truthy(v: Str) -> Bool`

truthiness of a bind var value: "", "false" and "0" are false; anything else is true.

### `fn bind_get(var_ids: Vec<Str>, var_vals: Vec<Str>, name: Str) -> Str`

the value of var `name` in the parallel (ids, vals) bind environment ("" when absent).

### `fn apply_binds(inout node: UiNode, var_ids: Vec<Str>, var_vals: Vec<Str>)`

evaluate `bind_visible` / `bind_disabled` on the whole tree against a var environment
bind_visible hides the node when its var is falsy; bind_disabled disables when its var is falsy or
absent: the var expresses the enabled condition, so a node is disabled when `bind_disabled` is set
and the var is false.

### `struct OverlayFocusable`

one tab stop: the node id (falls back to on_click when the id is empty), its explicit tab_index
(has_tab_index=false sorts after all explicit indices), document tree order, and the action.

### `fn focusable_dup(f: OverlayFocusable) -> OverlayFocusable`

an OverlayFocusable of its own.

### `fn collect_overlay_focusables(root: UiNode) -> Vec<OverlayFocusable>`

collect focusable controls in tab order: explicit tab_index first (ascending), then tree order;
hidden/disabled nodes (runtime state or binds; apply those first) are skipped.

### `fn cycle_overlay_focus(focusables: Vec<OverlayFocusable>, current: Str, reverse: Bool) -> Str`

the next (or previous, when `reverse`) focus id after `current`; wraps; the first stop when
`current` is empty/unknown; "" when there are no focusables.

### `fn rs_add_focus(inout state: OverlayRuntimeState, focused_id: Str)`

merge the `focused` class for `focused_id` into a runtime state:
apply_runtime_state afterwards gives the node the class so a
stylesheet `.focused` rule can style the focus ring.

### `fn ui_text_layout(node: UiNode, avail_w: Float) -> TextLayout`

the rendered content + box for a text `node`, honoring its wrap/clamp/transform/overflow style
within `avail_w`. Uses text_render_content_and_metrics.

### `fn node_measure_style(node: UiNode) -> TextMeasureStyle`

how a node's text is measured: its size and spacings, and the font stack, direction and
language the text engine shapes it with.

### `fn node_label_width(node: UiNode) -> Float`

the width of a node's one-line label (a button's), shaped in the label's own direction.

### `fn node_font_stack(node: UiNode) -> Int`

the font stack a node's text is shaped with: its `font_family` / `font_weight` / `font_style`
resolved by the text engine, or 0 (the default stack) when it names none.

### `fn node_letter_spacing(node: UiNode) -> Float`

the node's letter spacing in px (0 when unset). A negative value is
clamped to 0, so it is a no-op rather than a squeeze.

### `fn node_word_spacing(node: UiNode) -> Float`

the node's word spacing in px (0 when unset), clamped the same way.

### `fn node_pad_x(node: UiNode) -> Float`

a vertical container with `gap`, `padding`, and children.
the horizontal padding: padding_x when set, else padding.

### `fn node_pad_y(node: UiNode) -> Float`

the vertical padding: padding_y when set, else padding.

### `fn ui_row(gap: Float, padding: Float, children: Vec<UiNode>) -> UiNode`

a horizontal container with `gap`, `padding`, and children.

### `fn ui_text(text: Str, size_px: Float) -> UiNode`

a text node.

### `fn ui_button(label: Str, on_click: Str) -> UiNode`

a button node with a label and click action id.

### `fn ui_rect(w: UiLength, h: UiLength, color: Color) -> UiNode`

a colored rectangle of size (w, h).

### `fn ui_spacer(w: UiLength, h: UiLength) -> UiNode`

an empty spacer of size (w, h).

### `fn ui_surface(id: Str, w: UiLength, h: UiLength) -> UiNode`

a reserved surface rectangle (host draws content) with an id and size.

### `fn ui_text_spans(spans: Vec<TextSpanRun>, size_px: Float) -> UiNode`

a rich text node: styled spans flowed inline (wrap via node.wrap / max_width, like plain text).

### `fn ui_scroll_column(id: Str, gap: Float, padding: Float, min_height: Float, children: Vec<UiNode>) -> UiNode`

a vertically-scrolling column: fixed viewport (min_height, default 120), children flow at natural
height, clipped; the host feeds offsets via layout_overlay_scrolled using `id`.

### `fn ui_scroll_row(id: Str, gap: Float, padding: Float, min_width: Float, children: Vec<UiNode>) -> UiNode`

a horizontally-scrolling row. The fixed viewport width is `min_width`; a scroll row
has no `min_height`.

### `struct PaintPrimitive`

paint primitive kind: 0 fill rect, 1 border rect, 2 text, 3 image.

Text effects (kind 2 only), optional fields:
  `outline_w > 0`  -> draw an outline of `outline_color` around every glyph (clamped to 0.5..16
                      by the layout pass).
  `has_grad`       -> fill the glyphs with a linear ramp from `color` (the gradient's from colour)
                      to `grad_to`, over the axis span [grad_a, grad_a + grad_b), x when
                      `grad_horizontal`, else y.
Both are painted by paint_cpu and paint_gpu; no other kind reads them.

These live on the struct and not in a side table indexed by a `has_fx` flag: a
`Vec<PaintPrimitive>` is produced in places that have no ComputedOverlay to hang a side
table off (`layout_multiline_edit` returns a bare vector, and
`SurfacePainter` accumulates one), so an index would make a primitive vector
no longer self-describing and would force those modules to thread a second value everywhere.
The cost is bounded by reusing what is already there: the gradient's from colour is `color`, its
bounds collapse to two floats, and "no outline" is `outline_w == 0` rather than a flag, which
makes seven fields instead of twelve.

### `fn pp_rect(rect: Rect, copy color: Color, radius: Float) -> PaintPrimitive`

a filled-rect paint primitive.

### `fn pp_border(rect: Rect, copy color: Color, border_w: Float) -> PaintPrimitive`

a border-rect paint primitive.

### `fn pp_border_round(rect: Rect, copy color: Color, border_w: Float, radius: Float) -> PaintPrimitive`

a border-rect paint primitive that follows a corner radius.

### `fn pp_box_shadow(rect: Rect, copy color: Color, radius: Float, blur: Float, spread: Float, dx: Float, dy: Float) -> PaintPrimitive`

a box shadow (kind 4): a soft shadow outside a box, as CSS `box-shadow` draws one: the box `rect`
with corner `radius`, grown by `spread`, moved by (dx, dy) and blurred over `blur` px, in `color`.
Stored: `rect` the grown box (unmoved), `radius` its corners, `border_w` the spread (the box itself is
`rect` shrunk by it: no shadow is drawn inside that), `outline_w` the blur, (`letter_spacing`,
`word_spacing`) the offset, the fields a transform scales, so a scaled box's shadow scales with it.

### `fn pp_text(copy pos: Vec2, text: Str, copy color: Color, size_px: Float) -> PaintPrimitive`

a text paint primitive.

Contract for `pos` on a kind-2 (text) primitive:
`pos` is the text box top-left in the same logical space as `rect`, not a baseline.
`pos.y` is the top edge of the first line box; line `n` starts at `pos.y + n * line_height`.

Every producer computes a top (this file's nk_text/button/rich-span branches centre a
box top inside the node rect; multiline_edit.nori walks line tops; dsl_widgets/dsl_input centre
a line box; dock_ui's `TextLabel.top` is the same quantity), so the top is what layout can state
without knowing anything about a font. Turning it into a baseline needs the font's ascent, which
only a painter has, so the conversion lives in the painters, once each:
`paint_cpu::cpu_text_baseline_offset` and `paint_gpu::gpu_text_baseline_offset`, both
`font_v_metrics_px(font, size_px).ascent`. Anything that draws a kind-2 `pos` straight into a
rasterizer's baseline argument paints one ascent too high.

### `fn rect_dup(r: Rect) -> Rect`

a click target: id, action string, and screen rect.
a Rect of its own, sharing nothing with `r`: the spelling for storing a rect that was read
from a value which goes on living (or dying) independently of the store.

### `fn pp_dup(p: PaintPrimitive) -> PaintPrimitive`

a PaintPrimitive of its own (every rect, colour and string fresh).

### `fn floats_dup(v: Vec<Float>) -> Vec<Float>`

a Vec<Float> of its own.

### `fn color_dup(c: Color) -> Color`

a Color of its own: a fresh record, whatever `c` was read from.

### `fn hit_region_dup(h: HitRegion) -> HitRegion`

a HitRegion of its own (strings and rect all fresh).

### `struct SurfaceRegion`

a reserved surface region: id and screen rect.

### `struct ScrollRegionMeta`

the result of laying out an overlay: paint primitives, hit regions, and surface regions.
one scroll region the host must manage: clamp its offset to [0, content_extent - viewport size]
and feed it back via layout_overlay_scrolled. `axis`: 1 = vertical (column), 0 = horizontal (row).

### `fn overlay_hovered(out: ComputedOverlay, rect: Rect) -> Bool`

true when the pointer is over `rect` (hover styling).

### `fn overlay_new(inout in_ids: Vec<Str>, inout in_offs: Vec<Float>) -> ComputedOverlay`

a fresh empty overlay carrying the given scroll-offset input state.

### `fn overlay_child(inout out: ComputedOverlay) -> ComputedOverlay`

a fresh child overlay inheriting all of `out`'s input state (scroll offsets + pointer).

### `fn overlay_scroll_off(out: ComputedOverlay, rid: Str) -> Float`

the input scroll offset for region `rid` (0 when the host has not set one).

### `fn overlay_text_width(text: Str, size_px: Float) -> Float`

approximate intrinsic width of `text` at `size_px` (half the size per char).

### `fn overlay_line_height(size_px: Float) -> Float`

the line height of text at `size_px`.

### `fn wrap_text(text: Str, max_w: Float, size_px: Float, hard_break: Bool) -> Str`

word-wrap `text` to `max_w` at font `size_px`, inserting '\n' between wrapped lines (existing '\n's
are preserved as hard breaks). When `hard_break`, a single word wider than `max_w` is split mid-word.

### `fn wrap_text_st(text: Str, max_w: Float, style: TextMeasureStyle, hard_break: Bool) -> Str`

wrap_text against a full measure style, the form that carries letter_spacing / word_spacing, so
a spaced-out string wraps at the width it will actually be drawn at.

### `fn truncate_with_ellipsis(text: Str, max_w: Float, size_px: Float) -> Str`

truncate `text` to fit `max_w` at `size_px`, appending "..." when it overflows (no-wrap ellipsis).

### `fn truncate_with_ellipsis_st(text: Str, max_w: Float, style: TextMeasureStyle) -> Str`

truncate_with_ellipsis against a full measure style (carries letter_spacing / word_spacing).

### `fn apply_line_clamp(text: Str, max_lines: Int, max_w: Float, size_px: Float, ellipsis: Bool) -> Str`

clamp `text` to at most `max_lines` lines; if `ellipsis`, the last kept line is truncated with "...".

### `fn apply_line_clamp_st(text: Str, max_lines: Int, max_w: Float, style: TextMeasureStyle, ellipsis: Bool) -> Str`

apply_line_clamp against a full measure style (carries letter_spacing / word_spacing).

### `fn apply_text_transform(text: Str, transform: Int) -> Str`

apply a CSS-like text-transform (0 none / 1 uppercase / 2 lowercase / 3 capitalize) over ASCII.

### `fn text_line_height_px(size_px: Float, line_height_mult: Float) -> Float`

resolved per-line height: size_px * max(line_height_mult, 0.25), floored at 1.

### `struct TextLayout`

the rendered text + its intrinsic box.

### `fn text_render_content_and_metrics(text: Str, available_w: Float, max_width: Float, size_px: Float, wrap: Bool, hard_break: Bool, line_clamp: Int, ellipsis: Bool, transform: Int, line_height_mult: Float, min_height: Float) -> TextLayout`

resolve a text node's rendered content and box: apply transform, optionally wrap (when `wrap` or
`line_clamp>0`), then clamp or single-line ellipsis, and measure. `max_width<=0` means "no cap"
(use `available_w`).

### `fn text_render_content_and_metrics_sp(text: Str, available_w: Float, max_width: Float, size_px: Float, wrap: Bool, hard_break: Bool, line_clamp: Int, ellipsis: Bool, transform: Int, line_height_mult: Float, min_height: Float, letter_spacing: Float, word_spacing: Float) -> TextLayout`

text_render_content_and_metrics plus `letter_spacing` / `word_spacing` (px), which widen every
line and therefore change where it wraps and where it is cut. They are the same values the painters space
the glyphs by, so the box a document reserves is the box the glyphs land in.

### `fn push_text_decorations(rect: Rect, text: Str, size_px: Float, line_height_mult: Float, decoration: Int, color: Color, width_px: Float) -> Vec<PaintPrimitive>`

build the decoration rects for `text` laid out at `rect` (one per non-empty line). `decoration`:
0 none / 1 underline / 2 strikethrough / 3 overline; `width_px` is the rule thickness (>= 0.5).

### `fn push_text_decorations_sp(rect: Rect, text: Str, size_px: Float, line_height_mult: Float, decoration: Int, color: Color, width_px: Float, letter_spacing: Float, word_spacing: Float) -> Vec<PaintPrimitive>`

push_text_decorations measuring with letter/word spacing, so the rule spans the whole spaced-out
line instead of stopping where the unspaced one would have ended.

### `fn push_text_decorations_lines(rect: Rect, text: Str, widths: Vec<Float>, offs: Vec<Float>, size_px: Float, line_height_mult: Float, decoration: Int, color: Color, width_px: Float) -> Vec<PaintPrimitive>`

push_text_decorations for lines already measured: line `i` of `text` is `widths[i]` wide and starts
`offs[i]` right of the box (offs empty = every line at the box's left), so the rule sits under the
line where it was placed, at the right for a right-aligned line of Arabic.

### `fn pp_text_set_lines(inout pr: PaintPrimitive, stack: Int, lang: Str, line_dir: Str, line_x: Vec<Float>)`

stamp a text primitive with how its lines are drawn: the font stack, the language, each line's
direction and x offset (see PaintPrimitive).

### `fn color_scale_alpha(copy c: Color, scale: Float) -> Color`

scale a colour's alpha by `scale` (clamped to 0..1).

### `fn text_shadow_steps(rect: Rect, blur: Float) -> Int`

how many stacked, fading copies a text shadow of `blur` over `rect` uses, at most 12.

### `fn push_text_shadow(rect: Rect, text: Str, size_px: Float, copy scolor: Color, blur: Float, offset_x: Float, offset_y: Float, font_bold: Bool, font_italic: Bool) -> Vec<PaintPrimitive>`

build a text drop shadow as stacked, offset, fading copies of the glyph run, which needs no blur
capability in either painter because every copy is an ordinary text primitive. `rect` is the
laid-out text box. A text shadow has no `spread`; only a rect shadow does.

### `fn push_text_shadow_sp(rect: Rect, text: Str, size_px: Float, copy scolor: Color, blur: Float, offset_x: Float, offset_y: Float, font_bold: Bool, font_italic: Bool, letter_spacing: Float, word_spacing: Float) -> Vec<PaintPrimitive>`

push_text_shadow with the node's letter/word spacing stamped on every copy. The shadow is the
glyph run drawn again, so it has to be spaced identically or it slides out from under the text.

### `fn text_outline_width_clamped(w: Float) -> Float`

the outline width actually drawn with: clamped to 0.5..16. Shared by
the plain-text node path and the per-span path, which apply the same clamp to the same field.

### `fn pp_text_apply_fx(inout pr: PaintPrimitive, node: UiNode, bounds: Rect)`

stamp a text node's outline / gradient effect data onto a text primitive.
The outline width is clamped to 0.5..16 and is
0 when there is no outline colour, and a gradient's from colour replaces the prim colour.

### `fn number_value_text(value: Float, precision: Int, suffix: Str) -> Str`

format `value` with up to `precision` decimals (default 3, capped at 6), trimming trailing zeros
and a bare ".", then append `suffix` (what `number` and `drag_value` show).

### `fn wrap_text_lines(text: Str, max_w: Float, size_px: Float, hard_break: Bool) -> Vec<Str>`

the wrapped lines of `text`, as a vector.

`wrap_text` answers with one string and newlines in it, which is what a text node wants and the
wrong shape for a panel that has to page through the result: a transcript that scrolls needs to
take a window out of the middle of the lines, and drawing a scrollbar needs to know how many
there are. Splitting the joined string back apart would be the first thing every such caller
wrote, and a caller wrapping by character count gets a proportional font wrong.

### `fn wrap_text_lines_st(text: Str, max_w: Float, style: TextMeasureStyle, hard_break: Bool) -> Vec<Str>`

wrap_text_lines against a full measure style; see `wrap_text_st`.

### `fn wrap_line_count(text: Str, max_w: Float, size_px: Float, hard_break: Bool) -> Int`

the number of lines `wrap_text` would produce (= wrapped newline count + 1).

### `fn ui_measure(node: UiNode, ctx: LayoutLengthContext) -> Vec2`

intrinsic (natural) size of `node` within `ctx` (recursive).

### `fn container_fixed_size(node: UiNode, ctx: LayoutLengthContext, measured: Vec2) -> Vec2`

A container's `w` / `h` (from its style: `.pool { h: 30 }`), where it has them, in place of what its
children measure: a box of a given size, its children placed in it as its alignment says. Unset (a
zero length in px) leaves the measured size.

### `fn ui_emit_chrome(node: UiNode, rect: Rect, inout out: ComputedOverlay)`

emit the background fill and border of a container/leaf `node` covering `rect` (internal).

### `fn measure_flow_column_h(children: Vec<UiNode>, ctx: LayoutLengthContext, gap: Float) -> Float`

total flowed height of `children` stacked vertically at natural size (Spacers skipped).

### `fn measure_flow_row_w(children: Vec<UiNode>, ctx: LayoutLengthContext, gap: Float) -> Float`

total flowed width of `children` laid horizontally at natural size (Spacers skipped).

### `fn overlay_flush_popups(inout out: ComputedOverlay)`

append the popup layer to `prims`/`hits` and empty it. Called once, by the top-level entry
points, after the whole tree has been laid out, so popup content is the last thing painted and
the last thing in `hits`.

### `fn resolve_absolute_rect(inner: Rect, size: Vec2, node: UiNode) -> Rect`

lay `node` into `rect`, emitting paint primitives, hit regions, and surface regions (recursive).
place an absolutely-positioned node inside `inner` at its natural size using top/right/bottom/left
(missing side -> the min edge).

### `fn has_absolute_child(node: UiNode) -> Bool`

true if any direct child is positioned out of flow (absolute/overlay).

### `fn z_sort_absolute(kids: Vec<UiNode>) -> Vec<Int>`

order out-of-flow siblings by ascending `z_index`, stably: equal `z_index` keeps document order,
so a document with no `z_index` at all lays out in document order.
Insertion sort with a `>` (never `>=`) comparison is the stability guarantee: swapping equals
would make paint order depend on the sort's internals, which is the non-determinism a
layout pass must not have. n is the number of absolute children of one container, so O(n^2) is
the cheap choice as well as the simple one.

### `fn ui_layout_node_body(inout node: UiNode, rect: Rect, ctx: LayoutLengthContext, inout out: ComputedOverlay)`

ui_layout_node past the hidden check and the paint effects: what the node draws itself, and its
children. A retained document calls this for a leaf, because it applies the effects itself
(layout/retained.nori composes them with its ancestors' and its scroll offsets).

### `fn ui_layout_flex_children(inout node: UiNode, rect: Rect, ctx: LayoutLengthContext, inout out: ComputedOverlay)`

a row / column's children: measured against the padded box, placed by the flex pass, laid out.

### `fn flex_stride() -> Int`

one child as the flex pass sees it: its measured size and the flex properties it carries,
pushed as FLEX_STRIDE floats onto `items`: w, h, fill_w, fill_h (0/1), weight, shrink,
has_align_self (0/1), align_self.

The pass itself (`container_flex_rects`) takes these rather than the children, so the
immediate layout above and a retained document, whose children are not a `Vec<UiNode>` and
whose sizes come from a cache, run the same arithmetic and cannot drift apart. Flat floats and
not a record per child because this runs for every child of every container on every frame of
the immediate path, where a record each is a measurable share of the layout.

### `fn container_inner(node: UiNode, rect: Rect) -> Rect`

the padded box a row / column flows its children into.

### `fn container_flex_rects(node: UiNode, inner: Rect, items: Vec<Float>) -> Vec<Float>`

the flex pass of a row / column: the rect of each child inside `inner`, in child order.
Reads the container's kind, gap and alignment; everything about the children is in `items`.

### `fn flex_rect_at(rs: Vec<Float>, i: Int) -> Rect`

child `i`'s rect from container_flex_rects' answer.

### `fn layout_overlay(inout root: UiNode, viewport: Rect) -> ComputedOverlay`

lay out an overlay `root` into a viewport rect, returning paint primitives, hits, and surfaces.

### `fn layout_overlay_scrolled(inout root: UiNode, viewport: Rect, inout scroll_ids: Vec<Str>, inout scroll_offs: Vec<Float>) -> ComputedOverlay`

like layout_overlay, with host-provided scroll offsets (parallel region-id -> offset vectors; get the
region ids + extents from the returned `scrolls`). The frame loop: layout -> read scrolls -> clamp/
update offsets from input -> layout again next frame.

### `fn layout_overlay_interactive(inout root: UiNode, viewport: Rect, inout scroll_ids: Vec<Str>, inout scroll_offs: Vec<Float>, mouse: Vec2, mouse_down: Bool) -> ComputedOverlay`

full interactive layout: scroll offsets + the pointer (position, button state) for hover/active
styling (`hover_bg`/`hover_border_color`/`hover_text_color`/`active_bg` from the cascade).

### `fn overlay_hit_test(overlay: ComputedOverlay, point: Vec2) -> HitRegion`

hit-test an overlay at `point`; returns the front-most hit region (id empty if none).

### `fn up_ws(s: Str, inout pos: Vec<Int>)`

skip whitespace and `// line comments` at the cursor (internal helper).

### `fn up_is_ident(c: Int) -> Bool`

true if byte `c` can start/continue an identifier (internal helper).

### `fn up_ident(s: Str, inout pos: Vec<Int>) -> Str`

read an identifier at the cursor (internal helper).

### `fn up_value_str(s: Str, inout pos: Vec<Int>) -> Str`

read a bare value token (string / number / ident) into a string (internal helper).

### `fn up_number_str(s: Str, inout pos: Vec<Int>) -> Str`

read a numeric token as is into a string (internal helper).

### `fn up_color(s: Str, inout pos: Vec<Int>) -> Color`

parse a `[r, g, b(, a)]` color array at the cursor (internal helper).

### `fn up_kind(name: Str) -> Int`

map a node-name string to a node kind tag (-1 if unknown) (internal helper).

### `fn up_align(word: Str) -> Int`

map an alignment word to its int (cross/main share start/center/end; stretch/space_between extra).

### `fn up_node(s: Str, inout pos: Vec<Int>) -> UiNode`

parse one node (and its children) at the cursor; returns the node (kind -1 placeholder on failure).

### `fn up_apply_field(s: Str, inout pos: Vec<Int>, inout node: UiNode, key: Str)`

apply field `key` (value at the cursor) to `node` (internal helper).

### `fn up_anchor_x(v: Str) -> Float`

a horizontal text-alignment word -> anchor x (start/left 0, center/middle 0.5, end/right 1).

### `fn up_anchor_y(v: Str) -> Float`

a vertical text-alignment word -> anchor y (start/top 0, center/middle 0.5, end/bottom 1).

### `fn up_overflow(v: Str) -> Int`

overflow attribute word -> om_* value (visible/hidden/auto/scroll).

### `struct TextSpanRun`

one styled run inside a text node (core subset).

### `fn ui_length_dup(l: UiLength) -> UiLength`

a UiLength of its own.

### `fn span_dup(r: TextSpanRun) -> TextSpanRun`

a TextSpanRun of its own (text, classes and colours all fresh).

### `fn spans_dup(v: Vec<TextSpanRun>) -> Vec<TextSpanRun>`

a fresh list of span copies.

### `fn nodes_dup(v: Vec<UiNode>) -> Vec<UiNode>`

a fresh list of node copies (deep: every subtree is copied too).

### `fn ui_node_dup(n: UiNode) -> UiNode`

a UiNode of its own: a deep copy, sharing nothing with `n` (children and spans included).

### `fn ui_node_dup_shallow(n: UiNode) -> UiNode`

every field of `n` but its children, which come back empty: the node on its own. A retained
document keeps a tree of these (each node's children live beside it, not inside it).

### `fn span(text: Str) -> TextSpanRun`

a plain span at the parent's size/color.

### `struct RichSegment`

a positioned styled segment on one laid-out line (x is relative to the line start).

### `struct RichLine`

one laid-out line of rich segments.

### `struct RichLayout`

the rich layout result box.

### `struct RichFace`

The face a rich text node's run is shaped and painted with: the node's own (its font_family, font_weight and
font_style; 0, the default stack, when it names none), made bold (weight 700) or italic by the span. A span's
`bold`/`italic` select this face; they are not only flags on the primitive, which the painter does not read.

### `fn layout_rich_node(node: UiNode, max_w: Float) -> RichLayout`

a rich text node laid out within `max_w`, the same for measuring it and painting it: its spans, size, line
height, wrapping, spacing and face

### `fn layout_rich_spans(spans: Vec<TextSpanRun>, base_size: Float, lh_mult: Float, wrap: Bool, max_w0: Float) -> RichLayout`

flow styled runs into wrapped lines (core subset): char-by-char with a word
buffer flushed at whitespace (when wrapping), at newlines, and at the wrap boundary; each segment
carries its run's style; a line's height is its tallest segment's line height.

### `fn layout_rich_spans_sp(spans: Vec<TextSpanRun>, base_size: Float, lh_mult: Float, wrap: Bool, max_w0: Float, letter_spacing: Float, word_spacing: Float) -> RichLayout`

layout_rich_spans with the text node's letter/word spacing applied to every segment. A `span`
cannot carry its own (the parser rejects `letter_spacing` there), so the node's
value is the only one there is, and it spaces each segment's own glyph run, which is
what the per-segment text primitives are then painted with.

### `fn layout_rich_spans_face(spans: Vec<TextSpanRun>, base_size: Float, lh_mult: Float, wrap: Bool, max_w0: Float, letter_spacing: Float, word_spacing: Float, face: RichFace) -> RichLayout`

layout_rich_spans_sp with each run measured in the face it is painted with (RichFace: the node's, bold or italic
by the span)

### `struct UiDocument`

a parsed `.ui` tree + its (optional) stylesheet. `doc_layout` runs the full pipeline.
`rich`: everything the document's `stylesheet { }` block says (selectors, tokens, transitions,
keyframes; layout/sheet.nori); `sheet` is the tag/class part of it the immediate path uses.

### `fn ui_document_parse(source: Str) -> UiDocument`

parse a `.ui` source into a document (no stylesheet attached yet).

### `fn doc_merge_stylesheet(inout doc: UiDocument, extra: OverlayStyleSheet)`

merge `extra`'s rules into the document's stylesheet (field-level; creates one when absent).

### `fn doc_layout(inout doc: UiDocument, viewport: Rect) -> ComputedOverlay`

lay the document out: cascade the stylesheet onto the tree (idempotent for an unchanged sheet),
then layout.

### `fn doc_layout_scrolled(inout doc: UiDocument, viewport: Rect, inout scroll_ids: Vec<Str>, inout scroll_offs: Vec<Float>) -> ComputedOverlay`

like doc_layout with host scroll offsets (see layout_overlay_scrolled).

### `fn doc_layout_interactive(inout doc: UiDocument, viewport: Rect, inout scroll_ids: Vec<Str>, inout scroll_offs: Vec<Float>, mouse: Vec2, mouse_down: Bool) -> ComputedOverlay`

like `doc_layout_scrolled`, with the pointer as well, so `hover_bg` / `hover_text_color` /
`active_bg` from the cascade actually fire.

The three exist in the stylesheet, in the cascade and in `ui_emit_chrome`, and a host that lays a
document out through `doc_layout_scrolled` gets an overlay whose `in_has_mouse` is false: every
one of them is then dead, silently, and a panel full of `class row { hover_bg: ... }` rows never
lights up under the pointer. This is the same call with the pointer in it.

### `fn doc_patch_text(inout doc: UiDocument, id: Str, new_text: Str) -> Bool`

replace the text of the text node with `id`; false when no such node.

### `struct OverlayParseCache`

a capped parse cache keyed by the exact source string; when full, the oldest entry is evicted.

### `fn parse_cache_get(inout cache: OverlayParseCache, source: Str) -> view UiNode`

the parsed tree for `source`, reusing a cached document when the source is identical. Note the
returned tree is a view of the cache's own document: treat it as immutable, or take
`ui_node_dup` of it when the caller mutates trees (apply_stylesheet/apply_binds mutate!). For
mutated flows, parse fresh and keep the cache for static chrome panels.

### `fn parse_ui(src: Str) -> UiNode`

parse a `.ui` document string into a UiNode tree (root node).


### `struct UiError`

where something went wrong: file, line, column (1-based) and what.

### `fn ui_error_text(e: UiError) -> Str`

"file:line:col: message" (the file left out when there is none).

### `struct SelPart`

one compound selector: a type (or "" for any), classes, pseudo-classes.

### `struct Selector`

a complex selector, left to right; `combs[i]` joins parts[i] and parts[i+1]: 0 descendant, 1 child.

### `struct Ease`

an easing curve: 0 linear, 1 cubic bezier (x1, y1, x2, y2).

### `struct TransSpec`

one property's transition.

### `struct AnimSpec`

an animation: keyframes by name, duration and delay (ms), iteration count (-1 infinite),
direction (0 normal 1 reverse 2 alternate 3 alternate_reverse), fill (0 none 1 forwards
2 backwards 3 both).

### `struct StyleDecl`

one declaration: kind 0 a property (`val`), 1 a transition (`trans`), 2 an animation (`anim`).
`src`/`off`: the source and byte offset of its value (where an error about it is reported).

### `struct StyleRule`

a rule: its selectors and declarations. `legacy`: 1 `tag NAME`, 2 `class NAME`, 0 a selector.

### `struct StyleImport`

an `import "path"` met in a source (resolved by whoever knows where the source lives).

### `struct StyleSet`

everything one or more sources said, in source order, unresolved.
`key`: the text of every source as parsed, so two sets from the same text compare equal
without comparing what they hold (a document re-sent unchanged recompiles nothing).

### `fn style_set_add_source(inout ss: StyleSet, file: Str, src: Str) -> Int`

register a source with a set (its text is kept for error positions); returns its index.

### `fn style_set_append(inout ss: StyleSet, other: StyleSet)`

append everything `other` says after what `set` says (later wins, so `other` overrides).

### `struct StyleParse`

a parsed `.style` source.

### `fn style_parse(src: Str, file: Str) -> StyleParse`

parse a standalone `.style` source. `file` names it in errors.

### `struct CRule`

a compiled rule: one selector, its resolved style, and the transition / animation it sets.

### `struct CSheet`

a compiled sheet: rules from least to most specific (source order between equals), keyframes,
and what kinds of selector it holds (so a host knows what a state change can affect).
  state:     some rule tests :hover / :active / :focus / :disabled
  state_anc: ... on a part that is not the last (a state on an ancestor restyles descendants)
  comb:      some rule has a combinator (a node's classes can restyle its descendants)
  ui_font:   the `--ui-font` token's family list, "" when no layer defines it: the face every
             node without a `font_family` of its own is drawn in (a theme sets the UI font with
             one token)

### `fn style_compile(ss: StyleSet) -> SheetCompile`

compile a set: every token resolved, every rule's declarations folded into a NodeStyle, the
rules ordered by specificity then source order, every animation's keyframes found.

### `fn csheet_kf(sheet: CSheet, name: Str) -> Int`

the keyframes named `name`, or -1.

### `fn style_set_legacy(ss: StyleSet) -> OverlayStyleSheet`

the legacy tag/class sheet a set implies, for the immediate path: the `tag X` / `class X` rules
(a later one replacing an earlier one of the same name, as that path always has), and the
new-syntax rules that are exactly one type or one class with no state (merged field by field).
Everything else (states, combinators, lists, transitions) is a retained document's.

### `struct MInfo`

what a selector can see of a node: its type, classes and states.

### `fn sel_matches(sel: Selector, me: MInfo, anc: Vec<MInfo>) -> Bool`

does `sel` match the node `me`, whose ancestors (root first) are `anc`?

### `struct Cascade`

the cascade's answer for one node.

### `fn csheet_cascade(sheet: CSheet, me: MInfo, anc: Vec<MInfo>) -> Cascade`

every rule that matches, least specific first, folded field by field; the last `transition`
and the last `animation` win whole (each is one property).


### `struct NodeStyle`

optional per-node visual props; an unset field (has_* == false) does not override during merge.
text_overflow: 0 clip / 1 ellipsis. text_transform/text_decoration use the tt_*/td_* constants.

### `fn overflow_mode_of(v: Int) -> OverflowMode`

convert a stored overflow Int to the core OverflowMode enum.

### `fn node_style_empty() -> NodeStyle`

an empty NodeStyle (every field unset).

### `fn node_style_dup(s: NodeStyle) -> NodeStyle`

a NodeStyle of its own: every colour a fresh record.

### `fn node_style_apply_over(base: NodeStyle, ovr: NodeStyle) -> NodeStyle`

merge `ovr` on top of `base` with field-level precision: each set field in `ovr` overrides,
unset fields leave `base` intact.

### `fn node_style_apply_in(inout acc: NodeStyle, ovr: NodeStyle)`

node_style_apply_over in place: `ovr`'s set fields written over `acc`'s.

### `struct OverlayStyleSheet`

tag rules (`"button"`/`"text"`/`"rect"`/`"surface"`) + class rules (any name).

### `fn stylesheet_new() -> OverlayStyleSheet`

an empty stylesheet.

### `fn sheet_merge_tag(inout sheet: OverlayStyleSheet, tag: Str, style: NodeStyle)`

merge `style` into the tag rule `tag` (field-level; inserts wholesale when new).

### `fn sheet_merge_class(inout sheet: OverlayStyleSheet, cls: Str, style: NodeStyle)`

merge `style` into the class rule `cls` (field-level; inserts wholesale when new).

### `fn sheet_has_tag(sheet: OverlayStyleSheet, tag: Str) -> Bool`

true if a rule exists for tag `tag`.

### `fn sheet_tag(sheet: OverlayStyleSheet, tag: Str) -> NodeStyle`

the tag rule (or an empty NodeStyle when absent).

### `fn sheet_has_class(sheet: OverlayStyleSheet, cls: Str) -> Bool`

true if a rule exists for class `cls`.

### `fn sheet_class(sheet: OverlayStyleSheet, cls: Str) -> NodeStyle`

the class rule (or an empty NodeStyle when absent).

### `fn stylesheet_merge_over(inout base: OverlayStyleSheet, ovr: OverlayStyleSheet)`

merge every rule of `ovr` on top of `base` (field-level).

### `fn resolve_node_style(sheet: OverlayStyleSheet, tag: Str, classes: Vec<Str>) -> NodeStyle`

cascade a node's style: start empty, apply the tag rule, then each class rule in order (later
classes win). Overflow/text/box props all resolve through this.

### `fn text_layout_from_style(style: NodeStyle, text: Str, available_w: Float, default_size: Float, default_lh_mult: Float) -> TextLayout`

resolve a text node's rendered content + box from a cascaded NodeStyle, using the given
defaults for any unset size/line-height. Pulls size/wrap/hard_break/line_clamp/text_overflow/
transform/max_width/min_height straight into text_render_content_and_metrics.


### `struct SurfaceLookup`

the answer to a surface lookup: `found` plus the screen rect (zero-sized on a miss).
A struct rather than Option<Rect> so a caller can ask `.found` without a match, and so a
miss still carries the id it failed to find for an error message.

### `fn surface_miss(id: Str) -> SurfaceLookup`

a clear miss: found = false, an empty rect, the id that was not there.

### `fn surface_screen_rect(surfaces: Vec<SurfaceRegion>, ox: Float, oy: Float, id: Str) -> SurfaceLookup`

the screen rect of the surface named `id`, offsetting the document-local rect by the panel
origin (`ox`, `oy`). The rects in `surfaces` are
already clipped by any enclosing scroll viewport, so a surface scrolled half out of view
reports the visible half, which is what a blit and a hit test both want.

### `fn surface_rect_of(overlay: ComputedOverlay, id: Str) -> SurfaceLookup`

`surface_screen_rect` against an overlay's surfaces, in the overlay's own coordinates.

### `fn surface_count(overlay: ComputedOverlay) -> Int`

how many surfaces the overlay declared.

### `fn surface_id_at(overlay: ComputedOverlay, i: Int) -> Str`

the id of the i-th surface ("" when out of range), for a host enumerating holes to fill.

### `fn sfb_unbound() -> Int`

binding kind: 0 unbound (placeholder fill), 1 a host texture handle, 2 a flat colour.

### `fn sfb_texture() -> Int`

binding kind 1: the host has a texture (or texture view) for this hole.

### `fn sfb_color() -> Int`

binding kind 2: the host wants a flat colour in this hole (a viewport that has not rendered yet).

### `struct SurfaceBinding`

what the host has for one named hole.

### `fn surface_binding_dup(b: SurfaceBinding) -> SurfaceBinding`

a SurfaceBinding of its own.

### `struct SurfaceBindings`

the host's whole table of hole contents, id-keyed.

### `fn surface_bindings() -> SurfaceBindings`

an empty binding table.

### `fn surface_placeholder_color() -> Color`

the colour drawn into a hole nothing is bound to: visible, so an unfilled viewport is a bug
you can see rather than a black rectangle indistinguishable from a rendered night scene.

### `fn surface_bind_texture(inout b: SurfaceBindings, id: Str, texture: Int)`

bind a host texture handle to a hole. The handle is opaque here: this module never
dereferences it, and nothing about it reaches the plugin that declared the hole.

### `fn surface_bind_color(inout b: SurfaceBindings, id: Str, copy color: Color)`

bind a flat colour to a hole.

### `fn surface_unbind(inout b: SurfaceBindings, id: Str)`

drop a binding (the hole falls back to the placeholder fill).

### `fn surface_binding_of(b: SurfaceBindings, id: Str) -> SurfaceBinding`

the binding for `id`, or an `sfb_unbound` one carrying the placeholder colour.

### `struct SurfaceBlit`

one texture-view blit the host owes: where (already clipped, already in screen space) and what.
This is the whole contract a GPU host has to satisfy.

### `struct SurfaceDrawList`

the result of consuming an overlay's surfaces: blits for the GPU layer, fill prims for
everything else. A CPU-only host uses `prims` and ignores `blits`; both are already clipped.

### `fn surface_draw_list(overlay: ComputedOverlay, b: SurfaceBindings, ox: Float, oy: Float) -> SurfaceDrawList`

consume `overlay.surfaces`: every hole becomes either a blit (texture-bound) or a fill prim
(colour-bound or unbound), in screen space at panel origin (`ox`, `oy`).

### `fn surface_paint_into(inout overlay: ComputedOverlay, b: SurfaceBindings) -> Vec<SurfaceBlit>`

consume the surfaces straight into the overlay's own prim list, and hand back the blits.
This needs no renderer change: paint_cpu and paint_gpu already walk `overlay.prims`,
so a placeholder or colour-bound hole draws with nothing added to either backend.

Ordering: the fills are appended, so they draw last. That is right for a viewport hole (the
usual case: a full-panel surface with nothing over it but gizmos, which this file draws after).
A host with document chrome on top of a hole should call `surface_draw_list` instead and draw
its prims before the overlay's.

### `fn surface_sense_none() -> Int`

sense nothing: the rect is geometry only.

### `fn surface_sense_hover() -> Int`

report hover.

### `fn surface_sense_click() -> Int`

report hover + click.

### `fn surface_sense_drag() -> Int`

report hover + click + drag.

### `fn surface_sense_click_and_drag() -> Int`

report everything: what a 3D viewport wants (SDK_SENSE_CLICK_AND_DRAG).

### `struct SurfaceInput`

the pointer state a host carries across frames. `dragging_id` is the drag-ownership latch:
once a drag starts on a rect it keeps receiving the delta
even when the pointer leaves, which is what an orbit drag needs.

### `fn surface_input_new() -> SurfaceInput`

a fresh pointer state: no pointer yet, nothing held, nothing dragging.

### `fn surface_input_frame(inout inp: SurfaceInput, copy mouse: Vec2, down: Bool)`

feed one frame of raw pointer input; derives delta, the press/release edges, and the press
origin. The origin is not cleared on release: the click check below reads it on
the release frame, and clearing it there would let a press-elsewhere/release-here through.

### `fn surface_input_leave(inout inp: SurfaceInput)`

the pointer left the window: hover and drag both stop.

### `struct SurfaceResponse`

what an allocated rect reports this frame, including
`local`, the pointer in rect-local coordinates. A picker needs pixel-in-viewport, and
recomputing it at every call site is how off-by-a-panel-origin bugs get written.

### `fn surface_response_none(id: Str) -> SurfaceResponse`

an all-false response (an unknown surface, or a rect that senses nothing).

### `fn surface_allocate_rect(inout inp: SurfaceInput, id: Str, rect: Rect, sense: Int) -> SurfaceResponse`

allocate an interaction rect and sense it: the press-origin gate, the drag-ownership latch, and
`changed = clicked || dragged`.

### `fn surface_sense(overlay: ComputedOverlay, ox: Float, oy: Float, id: Str, inout inp: SurfaceInput, sense: Int) -> SurfaceResponse`

find the named hole in `overlay` and sense it, in one call. The rect comes from `surface_screen_rect`, so it is the clipped
rect: a surface scrolled half out of its viewport senses only the half you can see, and a
press in the scrolled-away part lands on nothing.

### `fn surface_event(id: Str, kind: Str) -> Str`

the event string for a surface interaction: extra information rides in the
string, `HitRegion` does not grow fields. `surface_event("viewport", "click")` -> "surface:viewport:click".

### `fn surface_push_hit(inout overlay: ComputedOverlay, ox: Float, oy: Float, id: Str, event: Str) -> Bool`

push a whole-surface HitRegion carrying `event`, so a host that routes everything through
`overlay_hit_test` can reach a surface too. Returns false when the id is unknown.

`overlay_hit_test` walks backwards, so the last push wins: call this first and push any
smaller gizmo hits after it, never the other way round.

### `struct SurfacePainter`

an immediate-mode painter bound to one surface. Draw into it, then flush it into the overlay.

### `fn surface_painter(overlay: ComputedOverlay, ox: Float, oy: Float, id: Str) -> SurfacePainter`

a painter clipped to the named surface. `found` is false for an unknown id, and every draw
call on such a painter is a no-op: a missing hole draws nothing rather than
scribbling on the panel at (0,0).

### `fn surface_fill(inout p: SurfacePainter, rect: Rect, copy color: Color, radius: Float)`

a filled rect gizmo.

### `fn surface_stroke(inout p: SurfacePainter, rect: Rect, copy color: Color, border_w: Float)`

a stroked rect gizmo (a selection box, a bounds outline).

### `fn surface_label(inout p: SurfacePainter, copy pos: Vec2, text: Str, copy color: Color, size_px: Float)`

a text gizmo (an axis label, a coordinate readout). `pos` is the text box top-left, not a
baseline, the same meaning a kind-2 PaintPrimitive's `pos` carries everywhere (see the
contract on layout::pp_text); the painters add the font's ascent.

### `fn surface_line(inout p: SurfacePainter, copy a: Vec2, copy b: Vec2, w: Float, copy color: Color)`

an axis-aligned line gizmo of thickness `w` (a grid line, a gizmo axis), as a thin filled rect.
PaintPrimitive has no line kind and adding one would mean touching both backends.

### `fn surface_painter_len(p: SurfacePainter) -> Int`

how many gizmos are queued (none of them clipped yet).

### `fn surface_painter_flush(inout p: SurfacePainter, inout overlay: ComputedOverlay) -> Int`

clip the queued gizmos to the surface rect and append them to `overlay.prims`; returns how
many survived the clip. The painter is emptied, so a painter can be reused across passes.

Delegates to `merge_clipped_overlay`: rect/border prims are intersected with the surface rect
(and dropped when fully outside), text prims keep their anchor but carry `has_clip`/`clip` for
the backend to scissor (and are dropped when the anchor itself is outside).


### `struct HighlightSpan`

highlight span within a source line: [start_col, end_col) with a token `class`.

### `fn highlight_span(start_col: Int, end_col: Int, cls: Str) -> HighlightSpan`

build a highlight span.

### `struct HighlightLine`

highlight spans for one source line.

### `fn default_token_color(cls: Str) -> OptColor`

the built-in color for a token `class`, or no_color() for an unknown class.

### `fn token_classes() -> Vec<Str>`

the classes seeded into a palette.

### `struct SyntaxTokenPalette`

resolved token colors: stylesheet class `text_color`/`color` overrides, else the built-in default,
else the palette default. Stored as parallel Vecs (Map<Color> is unsupported).

### `fn palette_new(copy def: Color, sheet: OverlayStyleSheet) -> SyntaxTokenPalette`

build a palette: for each token class, prefer `sheet`'s class rule text_color/color, then the
built-in default, then `def`. Pass an empty stylesheet for defaults-only.

### `fn palette_defaults(def: Color) -> SyntaxTokenPalette`

a defaults-only palette (no stylesheet overrides).

### `fn palette_color(pal: SyntaxTokenPalette, cls: Str) -> Color`

the color for token `class`: the palette entry, else the built-in default, else the palette default.

### `fn source_lines(source: Str) -> Vec<Str>`

split `source` into lines on '\n'; an empty source yields a single empty line.

### `struct VisibleRange`

the visible line window for a virtualized editor.

### `struct BracketMatch`

the matching-bracket position for the bracket at/nearest `caret`.

### `fn scan_bracket(text: Str, frm: Int, open: Int, close: Int, forward: Bool) -> BracketMatch`

scan for the partner of the bracket at `frm` (open/close byte values), forward or backward.

### `fn bracket_partner_index(text: Str, caret: Int) -> BracketMatch`

the matching bracket for the bracket at `caret` (clamped into range); no match if `caret` isn't on
one of ()[]{}.


### `struct TemplateItem`

one foreach item: parallel field keys/values (`{{ITEM.key}}` -> value).

### `struct TemplateList`

a named list for `{{#foreach ITEM in name}}`.

### `struct TemplateResult`

the render outcome: `out` when ok, else `err` names the missing variable/list.

### `fn render_ui_template(template: Str, var_ids: Vec<Str>, var_vals: Vec<Str>, lists: Vec<TemplateList>) -> TemplateResult`

render a `.ui` template: expand foreach blocks, then substitute `{{name}}` (string-escaped) and
`{{raw:name}}` (as-is) from the parallel (ids, vals) environment.

### `fn tpl_format_number(x: Float) -> Str`

render a JSON number: integral values in integer form, others as the shortest decimal that
reads back as the same double.

This forwards to `std/fmt::float_json`. `null` is printed for NaN/inf, and the scientific
form is used when the decimal exponent `e < -5 || e >= 17`. Negative zero renders `-0`.

### `fn tpl_json_token(v: Json, raw: Bool) -> Str`

the text a `{{name}}` (raw=false, strings escaped for a DSL string) or `{{raw:name}}` placeholder
expands to: null is empty, numbers/bools print bare, and
arrays/objects print as their JSON text in both forms (they are never string-escaped).

### `fn tpl_json_lookup(env: Json, path: Str, inout found: Vec<Int>) -> Json`

resolve a dotted path (`a`, `a.b`, `a.b.c`, ...) against a Json environment; found[0] = 1 on a hit.

### `fn render_ui_template_json(template: Str, env: Json) -> TemplateResult`

render a `.ui` template against a JSON environment: expand `{{#foreach ITEM in PATH}}` blocks,
then substitute `{{path}}` (strings escaped for a DSL string) and `{{raw:path}}` (as-is). Keys
are dotted paths into the tree.


### `fn text_prev_cluster(text: Str, i: Int) -> Int`

the grapheme cluster boundary before byte `i` (0 at the start).

### `fn text_next_cluster(text: Str, i: Int) -> Int`

the grapheme cluster boundary after byte `i` (the length at the end).

### `fn text_snap_cluster(text: Str, i: Int) -> Int`

`i` moved back to the start of the cluster it is inside (unchanged when it is on a boundary).

### `fn text_backspace_from(text: Str, i: Int) -> Int`

where Backspace at byte `i` deletes from (it deletes [result, i)): the last code point of the
cluster before `i`, or the whole cluster when it is one symbol (see the header).

### `struct TextCarets`

one line's caret geometry: its grapheme clusters (starts `gs`, then the line's length), each
cluster's visual extent [x0, x1) and whether it runs right to left, and the paragraph direction.

### `fn text_carets(line: Str, style: TextMeasureStyle, dir: Int, unit: Bool) -> TextCarets`

the caret geometry of one `line` (no newline in it) measured with `style` in paragraph
direction `dir` (dir_ltr/dir_rtl/dir_auto). With `unit` every cluster is one unit wide, which gives the
visual order of the stops (all moving needs) without a font.

### `fn text_carets_x(tc: TextCarets, i: Int) -> Float`

the x of a caret at byte `i` of the line: the leading edge of the cluster that starts there
(its left for a left-to-right cluster, its right for a right-to-left one); at the line's end the
trailing edge of the logically last cluster.

Where two directions meet a position has two places on screen (the end of one run and the start
of the other), and drawing every such position at the same one of them leaves the other place
unreachable. The caret goes with the run nested deeper (the higher bidi level): after an Arabic
word in an English sentence it is drawn at the word's left end, where the word ended, and
before the word at its right end, where it starts, so every place on screen is some position.

### `fn text_carets_hit(tc: TextCarets, x: Float) -> Int`

the caret stop (a byte offset) nearest to `x`; a tie goes to the later stop in the string.

### `fn text_carets_move(tc: TextCarets, i: Int, delta: Int) -> Int`

the caret stop one step to the right (`delta` 1) or left (-1) of byte `i` on screen, or -1 when
there is none in this line.

### `fn text_carets_sel(tc: TextCarets, a: Int, b: Int) -> Vec<Float>`

the screen spans [x0, x1, x0, x1, ...] of the selection [a, b) in this line: one per visual run of
selected clusters (a selection through mixed directions is several pieces on screen).

### `fn text_move_visual(text: Str, i: Int, delta: Int, dir: Int) -> Int`

the caret after one Left (`delta` -1) or Right (1) press from byte `i` of `text`, in a paragraph
running `dir`: one cluster on screen within the line; past the line's visual edge, to the
neighbouring line (the next one when the press points forward in the paragraph's direction).

### `fn set_ime_preedit(text: Str, cursor_begin: Int, cursor_end: Int)`

tell this image of the library what the input method is composing for the focused widget
("" for nothing), and the pre-edit's own cursor/selection as byte offsets into it.

### `fn set_ime_preedit_from(input: InputState)`

...from an InputState (the immediate-mode runner's frame input).

### `fn ime_preedit() -> Str`

the pre-edit text ("" when nothing is being composed).


### `fn dir_ltr() -> Int`

paragraph directions, as std/font numbers them.

### `fn text_engine_on() -> Bool`

does layout have a real face to measure with?

### `fn text_engine_face_count() -> Int`

how many faces the pool holds.

### `fn text_engine_face(i: Int) -> view font::Font`

pool face `i`.

### `fn text_engine_pool() -> view Vec<font::Font>`

the pool, for shaping through a stack (std/font's shape_line_pool takes it whole).

### `fn text_engine_generation() -> Int`

a number that changes whenever any stack's faces change (painters key their own caches on it).

### `fn text_engine_set_font(inout f: font::Font)`

make `f` the face layout measures the default stack with (a copy is kept). Installing another
face later replaces it: a host that switches the UI font calls this again, and every width
measured with the old face is forgotten. A face equal to the installed one changes nothing.

### `fn text_engine_add_fallback(inout f: font::Font)`

add `f` behind the default stack's faces: a cluster none of them has is drawn and measured from
it. Adding a face that is already in the stack changes nothing.

### `fn text_engine_reset()`

forget every face and stack (layout falls back to the heuristic measure).

### `fn text_engine_pool_face(sink f: font::Font, key: Str) -> Int`

put `f` in the pool (taking it) under `key` and return its index; a key already pooled is not
added again. Stacks name faces by these indices.

### `fn text_engine_pooled(key: Str) -> Int`

the pool index of the face loaded from `key`, or -1.

### `fn text_engine_stack(key: Str, faces: Vec<Int>) -> Int`

register a stack of pool faces under `key` (tried in order, then the default stack behind them)
and return its id (> 0). The same key returns the same id.

### `fn text_engine_stack_id(key: Str) -> Int`

the stack id registered under `key`, or 0 (the default stack) when there is none.

### `fn text_engine_stack_faces(stack: Int) -> Vec<Int>`

the pool faces stack `stack` tries, in order: its own, then the default stack's (a family that
lacks a character falls back to the UI font, then to its fallbacks). Stack 0 is the default.

### `fn text_engine_shape(text: Str, px: Float, stack: Int, dir: Int, lang: Str) -> font::ShapedLine`

shape one line of `text` at `px` through stack `stack`, laid out in visual order with paragraph
direction `dir` (dir_ltr/dir_rtl/dir_auto). A glyph's `font` is a pool index (not a position in
the stack). Empty when no face is installed.

### `fn text_engine_width(text: Str, px: Float, stack: Int, dir: Int, lang: Str) -> Float`

the advance width of one line of `text` at `px`, shaped through stack `stack`.

### `fn text_engine_width_stats() -> Vec<Int>`

the width cache's hits and misses so far (for measuring).

### `fn text_engine_logical_x(text: Str, px: Float, stack: Int, dir: Int, lang: Str) -> Vec<Float>`

the caret x of every byte offset of `text` (`text.len() + 1` entries) in logical order: entry `i`
is the advance of everything before byte `i` (the line's own reading order, not its visual one).
A cluster's advance is shared out evenly over the grapheme clusters inside it, so a caret between
the letters of a ligature ("ffi") lands proportionally inside the glyph; a byte inside a grapheme
cluster gets the x of the cluster's start.

### `fn text_para_dir(text: Str, dir: Int) -> Int`

the direction a paragraph of `text` runs when its `dir` is `dir`: dir_ltr/dir_rtl as given, and
for dir_auto the first strong character's (UAX #9 P2/P3; left to right when there is none).

### `fn text_engine_add_font_dir(dir: Str)`

add every face under `dir` to the faces families are resolved against.

### `fn text_engine_use_system_fonts(on: Bool)`

whether a family none of the added directories has may be looked for in the system's fonts
(on by default).

### `fn text_family_list(families: Str) -> Vec<Str>`

the families of a comma-separated list, trimmed of spaces and quotes.

### `fn text_engine_family_stack(families: Str, weight: Int, italic: Bool) -> Int`

the font stack for a family list at `weight` (100-900; 0 = 400) and `italic`: the faces the list
names, best match first, then the default stack. 0 (the default stack) when nothing in the
list is installed and no weight or slant is asked for that the default face lacks.


### `fn text_collapse_ws(s: Str) -> Str`

`s` with every run of space/tab/CR/LF made one space and none at either end: what wrapping
does to a line before filling it.

### `fn text_wrap_lines(text: Str, max_w: Float, style: TextMeasureStyle, hard_break: Bool) -> Vec<Str>`

the wrapped lines of `text` at `max_w` for `style`: each hard line collapsed and filled at UAX #14
opportunities. `max_w <= 1` leaves the text as it is (one piece per hard line).

### `fn text_wrap_lines_ix(text: Str, max_w: Float, style: TextMeasureStyle, hard_break: Bool, inout hard_ix: Vec<Int>) -> Vec<Str>`

text_wrap_lines, also saying which hard line (paragraph) of `text` each line came from.

### `fn text_para_dirs(text: Str, dir: Int) -> Str`

the direction of each hard line (paragraph) of `text`: `dir`, or for dir_auto its first strong
character's, one character per hard line, 'L' or 'R'.

### `fn text_truncate_ellipsis(text: Str, max_w: Float, style: TextMeasureStyle) -> Str`

`text` cut to fit `max_w` with "..." after it, at a grapheme cluster boundary (never inside a
cluster). Unchanged when it already fits.

### `fn text_line_offsets(widths: Vec<Float>, dirs: Str, box_w: Float, align: Int, anchor_x: Float) -> Vec<Float>`

where each line of a laid-out text box starts, relative to the box's left edge: line `i` of width
`widths[i]` in a box `box_w` wide, aligned by `ta_*`: start/end follow the line's
direction ('L'/'R' in `dirs`), left/right/center do not. Empty when every line starts at 0 (the
common case costs no allocation in the primitive).

### `fn ta_auto() -> Int`

text alignment: physical (left/center/right, or an `anchor` x, what `anchor_x` says) or
direction-relative (start/end: the line's own start or end, which is the right for a line of
Arabic). ta_auto is start, and is what a text node without `text_align` gets.

### `fn text_style_dir(style: TextMeasureStyle, rtl: Bool) -> TextMeasureStyle`

the measure style of a line that runs in direction `rtl` (the painters shape it that way).

### `fn text_render_st(text: Str, available_w: Float, max_width: Float, wrap: Bool, hard_break: Bool, line_clamp: Int, ellipsis: Bool, transform: Int, line_height_mult: Float, min_height: Float, style: TextMeasureStyle) -> TextLayout`

lay a text out: transform it, wrap it (when `wrap` or `line_clamp > 0`), clamp it or cut it with an
`available_w`. Uses the shaper's widths, UAX #14
breaks and each line's direction (TextLayout.line_dir) and width (TextLayout.line_w).


### `struct TextMeasureStyle`

font/style inputs for measurement (size in px plus bold/italic/family and the two spacings), and
what the text engine shapes with: the font stack (0 = the default; see text_engine.nori), the
paragraph direction (dir_ltr/dir_rtl/dir_auto) and the language (BCP 47, "" for none).

### `fn text_measure_style(size_px: Float) -> TextMeasureStyle`

a measure style with just a size (no bold/italic/family, no extra spacing).

### `fn text_measure_style_sp(size_px: Float, letter_spacing: Float, word_spacing: Float) -> TextMeasureStyle`

a measure style carrying `letter_spacing` / `word_spacing` (px). Both are clamped to >= 0 where
they are applied.

### `fn text_spacing_offsets_cps(cps: Vec<Int>, letter_spacing: Float, word_spacing: Float) -> Vec<Float>`

cumulative extra advance before each of the `n` glyphs in `cps`, plus a final total: a table of
`n + 1` floats where entry `i` is what to add to glyph `i`'s pen x and entry `n` is the whole
line's extra width. Zero spacing gives an all-zero table (and costs one pass).

### `fn text_spacing_offsets_bytes(text: Str, letter_spacing: Float, word_spacing: Float) -> Vec<Float>`

the same table indexed by byte offset into `text` (`text.len() + 1` entries): entry `i` is the
extra advance accumulated before the caret at byte `i`, and the last entry is the line total.
A UTF-8 continuation byte carries its lead byte's value, so a multi-byte character is one glyph
for spacing purposes, the same count the painters make from the decoded codepoints.

### `fn text_spacing_extra(text: Str, letter_spacing: Float, word_spacing: Float) -> Float`

the total extra width `letter_spacing` / `word_spacing` add to one line of `text`.

### `struct TextLineMetrics`

line metrics (height of a line box and its top offset).

### `fn text_line_height(size_px: Float) -> Float`

the height of a line box at `size_px`. A line box's height is a function of the size alone, so
callers that want only the number ask for it here instead of measuring a line and reading one
field off the result.

### `fn text_baseline_in_box(size_px: Float, ascent: Float, descent: Float, line_gap: Float) -> Float`

px from a line box's top edge down to its baseline, for a face with these vertical metrics.

The baseline is placed relative to the box rather than hung from the face's ascender. The line
box is `size + 5` and knows nothing about the face, so hanging glyphs from the ascent would move
every glyph in the window down or up inside a box that had not moved whenever the font changed.
DejaVuSansMono ascends 0.928 em and JetBrainsMono Nerd 1.020 em, which at 13px is a 1.2px
drop of every label, every row of source and every button caption at once.

Instead the face's ink is centred in the box reserved for it. The leftover leading
(box minus (ascent - descent + line_gap)) is split evenly above and below, so a taller face eats
its own extra height symmetrically instead of pushing everything downwards. The same two faces
then differ by 0.19px rather than 1.2px, and a face with a big descender (a Nerd font's icons)
does not hang out of the bottom of its row.

`descent` is negative, as `font_v_metrics_px` reports it.

### `fn text_line_metrics_fallback(size_px: Float) -> TextLineMetrics`

fallback line metrics for a font size (height = size + 5, top = 0).

### `fn set_char_advances(adv: Vec<Int>)`

install a 256-entry per-byte advance table so text measurement matches a real font. Each entry is
`advance_units / units_per_em * 65536` (16.16 fixed point). Clears to the heuristic if `< 256`.

### `fn fallback_char_advance(ch: Int, size_px: Float) -> Float`

approximate advance width of one ASCII byte `ch` at `size_px`: the installed real-metrics table
if set (see set_char_advances), else the flat heuristic.

### `fn fallback_advance_to_caret(text: Str, caret: Int, size_px: Float) -> Float`

approximate width of the first `caret` chars of `text` at `size_px` (fallback heuristic).

### `fn text_measure_cumulative_x(text: Str, style: TextMeasureStyle) -> Vec<Float>`

cumulative caret X for every position 0..=len(text) (table for caret placement), in logical
order: entry `i` is the advance of the text before byte `i`. With the text engine on this is the
shaped line's (a ligature's advance shared over the clusters inside it).

### `fn text_measure_width(text: Str, style: TextMeasureStyle) -> Float`

total advance width of `text` at the style's size.

### `fn text_measure_width_from_cumulative(cumulative_x: Vec<Float>, caret: Int) -> Float`

caret X at index `caret` from a precomputed cumulative table (clamped to the table).

### `fn text_measure_width_to_caret(text: Str, caret: Int, style: TextMeasureStyle) -> Float`

caret X at index `caret` within `text` (reshapes; prefer the cumulative form in hot loops).

### `fn text_measure_line(style: TextMeasureStyle, text: Str) -> TextLineMetrics`

line metrics for `text` (fallback: depends only on the font size).

### `fn text_measure_glyph_extent(text: Str, style: TextMeasureStyle) -> TextLineMetrics`

visual glyph top + height for `text` (fallback: the line box top/height).

### `fn text_measure_draw_pos(bounds: Rect, text: Str, style: TextMeasureStyle, ax: Float, ay: Float) -> Vec2`

top-left position to anchor `text` inside `bounds`; anchors ax/ay in [0,1] (0=start, 1=end).


### `fn widget_row_height_const() -> Float`

default widget row height.

### `fn widget_spacing() -> Float`

vertical spacing between widgets.

### `fn widget_char_width() -> Float`

approximate character width for widget text sizing.

### `fn checkbox_size() -> Float`

checkbox side length.

### `fn slider_track_h() -> Float`

slider track height.

### `fn slider_thumb_w() -> Float`

slider thumb width.

### `fn wk_button() -> Int`

kind tag: clickable button.

### `fn wk_label() -> Int`

kind tag: non-interactive label.

### `fn wk_heading() -> Int`

kind tag: section heading.

### `fn wk_subheading() -> Int`

kind tag: subheading.

### `fn wk_checkbox() -> Int`

kind tag: checkbox.

### `fn wk_slider() -> Int`

kind tag: horizontal slider.

### `fn wk_text_field() -> Int`

kind tag: single-line text field.

### `fn wk_separator() -> Int`

kind tag: horizontal separator.

### `fn wk_spacer() -> Int`

kind tag: vertical spacer.

### `fn wk_progress() -> Int`

kind tag: progress bar.

### `fn wk_toggle() -> Int`

kind tag: toggle switch.

### `fn wk_dropdown() -> Int`

kind tag: dropdown (cycles selection on click).

### `fn wk_small_button() -> Int`

kind tag: compact button.

### `fn wk_link() -> Int`

kind tag: text link.

### `fn wk_monospace() -> Int`

kind tag: monospace text.

### `fn wk_number() -> Int`

kind tag: number input (value with -/+ steppers).

### `fn wk_drag() -> Int`

kind tag: drag value (compact draggable number field).

### `fn wk_badge() -> Int`

kind tag: badge (small text pill).

### `fn wk_selectable() -> Int`

kind tag: selectable label (selectable read-only text).

### `fn wk_icon() -> Int`

kind tag: icon (host-resolved icon key + size; non-interactive).

### `fn wk_collapsing() -> Int`

kind tag: collapsing header (arrow + label; click toggles).

### `fn wk_radio() -> Int`

kind tag: radio group (one selected; options stacked vertically).

### `fn wk_tabbar() -> Int`

kind tag: tab bar (horizontal tabs; click selects).

### `fn wk_color_swatch() -> Int`

color swatch (square + label; click to report).

### `fn wk_color_picker() -> Int`

color picker (swatch + label; click opens the 2D square + hue strip popup).

### `fn wk_image_button() -> Int`

button with an optional icon key (host resolves the icon to an image).

### `fn wk_tilegrid() -> Int`

grid of colored ASCII tiles (each cell: char + fg + bg color).

### `fn wk_tooltip() -> Int`

tooltip text for the preceding widget (takes no space; see tooltip_for_hovered).

### `fn wk_hyperlink() -> Int`

link with an optional URL (styled like a text link).

### `fn wk_resize_handle() -> Int`

thin draggable resize line (vertical or horizontal).

### `fn wk_button_styled() -> Int`

a button with inline font overrides (family/size/bold/italic).

### `fn wk_label_styled() -> Int`

a label with inline font overrides (family/size/bold/italic).

### `fn whk_button() -> Int`

hit kind: button click.

### `fn whk_checkbox() -> Int`

hit kind: checkbox toggle.

### `fn whk_slider_thumb() -> Int`

hit kind: slider thumb drag.

### `fn whk_slider_track() -> Int`

hit kind: slider track click.

### `fn whk_text_field() -> Int`

hit kind: text-field focus.

### `fn whk_dropdown() -> Int`

hit kind: dropdown click.

### `fn whk_toggle() -> Int`

hit kind: toggle click.

### `fn whk_link() -> Int`

hit kind: link click.

### `fn whk_noninteractive() -> Int`

hit kind: non-interactive.

### `fn whk_disabled() -> Int`

hit kind: disabled widget (ignore clicks).

### `fn whk_number() -> Int`

hit kind: number-input field (focus/edit).

### `fn whk_number_plus() -> Int`

hit kind: number-input increment button.

### `fn whk_number_minus() -> Int`

hit kind: number-input decrement button.

### `fn whk_drag() -> Int`

hit kind: drag-value field (drag to change).

### `fn whk_badge() -> Int`

hit kind: badge click.

### `fn whk_selectable() -> Int`

hit kind: selectable-label click.

### `fn whk_collapsing() -> Int`

hit kind: collapsing-header toggle.

### `fn whk_small_button() -> Int`

hit kind: small-button click (distinct from a regular button).

### `fn whk_radio_option() -> Int`

hit kind: radio option click (the chosen option is in the hit's `sub`).

### `fn whk_tabbar_tab() -> Int`

hit kind: tab-bar tab click (the clicked tab index is in the hit's `sub`).

### `fn whk_color_swatch() -> Int`

color swatch click.

### `fn whk_color_picker_swatch() -> Int`

color-picker swatch click (open/close the picker popup).

### `fn whk_color_picker_square() -> Int`

2D saturation/value square (click/drag sets S and V).

### `fn whk_color_picker_hue_strip() -> Int`

hue strip (click/drag sets H).

### `fn whk_image_button() -> Int`

image-button click.

### `fn whk_hyperlink() -> Int`

hyperlink click.

### `fn whk_resize_handle() -> Int`

resize-handle drag.

### `struct TileGridCell`

a single cell in a TileGrid: a character + foreground + background color.

### `fn tile_cells_dup(v: Vec<TileGridCell>) -> Vec<TileGridCell>`

a fresh list of cell copies.

### `fn tile_cell(ch: Str, copy fg: Color, copy bg: Color) -> TileGridCell`

build a TileGrid cell.

### `struct Widget`

a widget: a `kind` tag plus a union of all widget fields (only some apply per kind).

### `fn widget_blank(kind: Int) -> Widget`

a blank widget with default fields (internal builder base).

### `fn widget_button(id: Int, label: Str) -> Widget`

a clickable button.

### `fn widget_small_button(id: Int, label: Str) -> Widget`

a compact button.

### `fn widget_label(text: Str) -> Widget`

a non-interactive label.

### `fn widget_heading(text: Str) -> Widget`

a section heading.

### `fn widget_subheading(text: Str) -> Widget`

a subheading.

### `fn widget_monospace(text: Str) -> Widget`

monospace text.

### `fn widget_link(id: Int, label: Str) -> Widget`

a text link.

### `fn widget_checkbox(id: Int, label: Str, checked: Bool) -> Widget`

a checkbox with current checked state.

### `fn widget_slider(id: Int, label: Str, value: Float, vmin: Float, vmax: Float) -> Widget`

a horizontal slider with current value in [min, max].

### `fn widget_text_field(id: Int, placeholder: Str, text: Str) -> Widget`

a single-line text field (plain display).

### `fn widget_toggle(id: Int, label: Str, on: Bool) -> Widget`

a toggle switch with current state.

### `fn widget_dropdown(id: Int, label: Str, options: Vec<Str>, selected: Int) -> Widget`

a dropdown with options and the selected index.

### `fn widget_separator() -> Widget`

a horizontal separator line.

### `fn widget_spacer(height: Float) -> Widget`

vertical empty space of `height` pixels.

### `fn widget_progress(value: Float) -> Widget`

a read-only progress bar with value in [0, 1].

### `fn widget_number(id: Int, label: Str, value: Float, vmin: Float, vmax: Float, step: Float) -> Widget`

a number input: a value field flanked by `-`/`+` stepper buttons.

### `fn widget_drag(id: Int, label: Str, value: Float, vmin: Float, vmax: Float, step: Float) -> Widget`

a drag value: a compact field you drag horizontally to change `value`.

### `fn widget_badge(id: Int, text: Str) -> Widget`

a small text pill; `id` lets the host treat it as clickable.

### `fn widget_selectable(id: Int, text: Str) -> Widget`

read-only text the host can let the user select/copy (hover-highlighted).

### `fn widget_icon(icon_key: Str, side: Float) -> Widget`

a non-interactive icon: `icon_key` is resolved by the host, `side` is the side length in px.

### `fn widget_collapsing(id: Int, label: Str, collapsed: Bool) -> Widget`

a collapsing-section header: an arrow + label; `collapsed` chooses the arrow direction.

### `fn widget_radio(id: Int, label: Str, options: Vec<Str>, selected: Int) -> Widget`

a radio group: a group `label` + vertically-stacked `options`, the `selected` one filled.

### `fn widget_tabbar(id: Int, tabs: Vec<Str>, selected: Int) -> Widget`

a horizontal tab bar: `tabs` shown side by side, the `selected` one highlighted.

### `fn widget_color_swatch(id: Int, label: Str, col: Color) -> Widget`

a color swatch: a colored square + `label`; click reports (e.g. to open a picker).

### `fn widget_color_picker(id: Int, label: Str, col: Color) -> Widget`

a color picker: a swatch + `label`; click opens the 2D square + hue strip (see layout_color_picker_popup).

### `fn widget_image_button(id: Int, label: Str, icon_key: Str) -> Widget`

a button with an optional icon key (`icon_key` resolved by the host); good for toolbars.

### `fn widget_tilegrid(cols: Int, rows: Int, cell_w: Float, cell_h: Float, cells: Vec<TileGridCell>) -> Widget`

a grid of colored ASCII tiles: `cols`x`rows` cells of `cell_w`x`cell_h`, row-major in `cells`.

### `fn widget_tooltip(text: Str) -> Widget`

tooltip text shown for the immediately-preceding widget when it is hovered (takes no layout space).

### `fn widget_hyperlink(id: Int, label: Str, url: Str) -> Widget`

a link with an optional `url` (empty = none); the host opens the URL on click.

### `fn widget_resize_handle(id: Int, vertical: Bool) -> Widget`

a thin draggable resize line; `vertical` chooses a vertical (6px-wide) vs horizontal (6px-tall) bar.

### `fn widget_button_styled(id: Int, label: Str, font_family: Str, font_size: Float, bold: Bool, italic: Bool) -> Widget`

a button with inline font overrides: empty `font_family` = default, `font_size` <= 0 = default.

### `fn widget_label_styled(text: Str, font_family: Str, font_size: Float, bold: Bool, italic: Bool) -> Widget`

a label with inline font overrides: empty `font_family` = default, `font_size` <= 0 = default.

### `fn widget_row_height(w: Widget, row_h_base: Float) -> Float`

the laid-out row height of `w` given the theme base row height.

### `fn content_height(widgets: Vec<Widget>, theme: Theme) -> Float`

total vertical height to display all widgets (for scroll clamping).

### `struct WidgetHitRect`

one hit-test rect: widget index, id, rect, and hit kind.

### `fn widget_hit_dup(h: WidgetHitRect) -> WidgetHitRect`

a WidgetHitRect of its own.

### `struct LayoutWidgetsResult`

the output of layout_widgets: solid rects, text labels, and hit rects.

### `fn widget_is_disabled(disabled: Vec<Int>, i: Int) -> Bool`

true if index `i` is in the disabled list.

### `struct TextFieldFocus`

text-field editing state for the focused field: caret blink/position + an optional selection range.

### `fn text_field_focus_none() -> TextFieldFocus`

no field focused (the default; plain text-field rendering).

### `fn text_field_focus(index: Int, caret_visible: Bool, cursor: Int) -> TextFieldFocus`

field `index` focused with caret at `cursor` (blink toggled by `caret_visible`), no selection.

### `fn text_field_focus_sel(index: Int, caret_visible: Bool, cursor: Int, sel_start: Int, sel_end: Int) -> TextFieldFocus`

field `index` focused with caret + a highlighted selection [sel_start, sel_end).

### `fn layout_widgets(content_rect: Rect, widgets: Vec<Widget>, theme: Theme, has_hovered: Bool, hovered_index: Int, scroll_offset: Float, disabled: Vec<Int>) -> LayoutWidgetsResult`

lay out widgets vertically in `content_rect`, producing draw rects, labels, and hit rects.
`has_hovered`/`hovered_index` mark the hovered row; `disabled` lists disabled indices.

### `fn layout_widgets_focus(content_rect: Rect, widgets: Vec<Widget>, theme: Theme, has_hovered: Bool, hovered_index: Int, scroll_offset: Float, disabled: Vec<Int>, focus: TextFieldFocus) -> LayoutWidgetsResult`

like `layout_widgets`, but a focused TextField gets a focus ring, selection highlight, and a
blinking caret inserted at the cursor (per `focus`).

### `fn hit_test_widgets(cursor: Vec2, hits: Vec<WidgetHitRect>) -> WidgetHitRect`

hit-test laid-out widgets at `cursor`; returns the matching hit (index -1 if none, last match wins).

### `fn tooltip_for_hovered(widgets: Vec<Widget>, hovered_index: Int) -> Str`

the tooltip text for the widget at `hovered_index`: the text of an immediately-following Tooltip
widget, or "" if the next widget isn't a Tooltip (or there is none).

### `fn scrollbar_track_width() -> Float`

vertical scrollbar track width.

### `fn scrollbar_thumb_min_h() -> Float`

minimum scrollbar thumb height.

### `fn scrollbar_node_thumb(track: Rect, content: Float, vw: Float, offset: Float) -> Rect`

where the thumb of a `scrollbar { }` node sits in its track.

One place, because two would drift. The library paints the bar and the host hit-tests it, and a
thumb drawn a few pixels from where it is grabbed is the kind of wrongness that reads as the
pointer being inaccurate rather than as a bug. `content`/`view`/`offset` are the plugin's own
units; only their ratios are used.

### `struct ScrollbarLayout`

a vertical scrollbar layout: the track rect plus an optional thumb rect.

### `fn scrollbar_layout(content_rect: Rect, content_height_px: Float, scroll_offset: Float, visible_height: Float) -> ScrollbarLayout`

compute the vertical scrollbar for `content_rect`; thumb absent when content fits the viewport.

### `struct ColorPickerPopup`

the laid-out color-picker popup: draw commands + the square/hue rects for hit-testing.

### `fn color_picker_popup_size() -> Vec2`

total (width, height) of the floating color-picker popup.

### `fn layout_color_picker_popup(popup_rect: Rect, col: Color) -> ColorPickerPopup`

lay out the floating color-picker popup: a 4x4 HSV gradient S/V square with a marker + a 36-segment
hue strip with a marker. Returns the draw commands and the square/hue rects for hit-testing.


### `fn window_title_bar_height() -> Float`

height of a window title bar.

### `fn window_resize_handle() -> Float`

width of the resize handle band at a window edge.

### `fn window_close_button_size() -> Float`

size of the close button square.

### `fn window_minimize_button_size() -> Float`

size of the minimize button square.

### `fn window_inner_padding() -> Float`

inner padding inside a window.

### `enum ResizeEdge`

a resize edge/corner. Tags: 0 Left,1 Right,2 Top,3 Bottom,4 TopLeft,5 TopRight,6 BottomLeft,7 BottomRight, -1 none.

### `fn resize_edge_tag(e: ResizeEdge) -> Int`

integer tag of a ResizeEdge.

### `struct WindowConstraints`

per-window size constraints (min, optional max, optional aspect lock).

### `fn window_constraints_default() -> WindowConstraints`

the default window constraints (min 160x120, no max, free aspect).

### `struct WindowBehavior`

per-window behavior flags.

### `fn window_behavior_default() -> WindowBehavior`

the default window behavior (closable/minimizable/draggable/resizable, nothing locked).

### `struct WindowStyle`

per-window chrome style. Optional overrides are gated by has_* / OptColor.

### `fn window_style_default() -> WindowStyle`

the default window style (all built-in chrome on, no overrides).

### `fn window_style_fully_custom() -> WindowStyle`

a transparent/fully-custom style (no built-in chrome; host renders everything).

### `struct WindowState`

persistent state for one window.

### `fn window_state_new(id: Int, sink title: Str, rect: Rect) -> WindowState`

a window with default behavior/constraints/style, visible and at z 0.

### `struct WindowInteractionResult`

the result of a window-manager frame: which window (if any) was focused/closed/etc.

### `fn window_interaction_none() -> WindowInteractionResult`

an empty interaction result (nothing happened).

### `struct WindowManager`

the floating-window manager: a z-sorted window list plus drag/resize/focus state.

### `fn window_manager_new() -> WindowManager`

a new, empty WindowManager.

### `fn wm_index(wm: WindowManager, id: Int) -> Int`

the index of the window with id `id`, or -1 if absent.

##### WindowManager

### `fn window(self, id: Int) -> view WindowState`

the window with id `id` (call only when present; check wm_index first).

### `fn has_window(self, id: Int) -> Bool`

true if a window with id `id` exists.

### `fn upsert(inout self, sink win: WindowState)`

insert `win` or replace the existing one with the same id (preserving its z); assigns z on insert.

### `fn set_visible(self, id: Int, visible: Bool)`

set a window's visibility.

### `fn bring_to_front(inout self, id: Int)`

raise window `id` above the others (unless it is always-on-bottom).

### `fn topmost_window_at(self, p: Vec2) -> Int`

the topmost visible window containing point `p`, or -1.

### `fn sort_by_z(self)`

re-sort the window list so later indices paint on top (on-bottom first, then by z).

### `fn begin_frame(inout self, input: InputState, screen_rect: Rect) -> WindowInteractionResult`

process one input frame: focus/drag/resize/close/minimize; returns what changed.

### `fn remove(inout self, id: Int)`

remove a window entirely (e.g. when a floating tab is re-docked into the dock tree).

### `fn wm_sort_key(w: WindowState) -> Int`

the z-sort key of a window (on-bottom windows sort first, then by ascending z).

### `fn wm_title_bar_rect(w: WindowState) -> Rect`

the title-bar rect of `w` (full width x title height); zero-height if the bar is hidden.

### `fn wm_close_button_rect(w: WindowState) -> Rect`

the close-button rect (top-right of the title bar) for window `w`.

### `fn wm_minimize_button_rect(w: WindowState) -> Rect`

the minimize-button rect (left of the close button if present) for window `w`.

### `fn wm_in_close_button(w: WindowState, p: Vec2) -> Bool`

true if `p` is over `w`'s close button (and the button is shown).

### `fn wm_in_minimize_button(w: WindowState, p: Vec2) -> Bool`

true if `p` is over `w`'s minimize button (and the button is shown).

### `fn wm_in_title_bar(w: WindowState, p: Vec2) -> Bool`

true if `p` is in the title bar of `w` (excluding the control buttons).

### `fn wm_in_drag_region(w: WindowState, p: Vec2) -> Bool`

true if `p` is in `w`'s drag-anywhere body region (excluding control buttons).

### `fn wm_edge_at(w: WindowState, p: Vec2) -> ResizeEdge`

the resize edge of `w` under `p` (EdNone if not on a resize band or not resizable).

### `fn wm_window_hit_edge(w: WindowState) -> Bool`

reserved: a non-None placeholder.

### `struct WindowLayout`

layout result of chrome: a window's rects, plus its content/title/button rects.

### `struct WindowLayoutResult`

the full window-chrome draw output: rect list, label list, per-window layout.

### `fn layout_windows(manager: WindowManager, theme: Theme) -> WindowLayoutResult`

build the backend-agnostic chrome (rects + labels + per-window layout) for all visible windows.

### `fn subtract_rect(base: Rect, cut: Rect, inout out: Vec<Rect>)`

subtract `cut` from `base`, pushing the (up to four) remaining fragments into `out`.

### `fn visible_fragments(base: Rect, overlays: Vec<Rect>) -> Vec<Rect>`

the visible fragments of `base` after subtracting every rect in `overlays`.

### `fn overlays_above(windows: Vec<WindowLayout>, owner_index: Int) -> Vec<Rect>`

the window rects above index `owner_index` in z-order (higher-z windows).

### `struct StrOpt`

a present flag + string value.

### `fn nui_trim(s: Str) -> Str`

trim ASCII whitespace.

### `fn nui_contains(s: Str, sub: Str) -> Bool`

true if `sub` occurs anywhere in `s`.

### `fn nui_starts_with(s: Str, p: Str) -> Bool`

true if `s` starts with `p`.

### `fn sanitize_save_thumb_filename(name: Str) -> StrOpt`

validate a save-thumb filename: trimmed, non-empty, no '/', '\\', or ".."; ok=false otherwise.

### `fn sanitize_overlay_image_path(path: Str) -> StrOpt`

validate an overlay image path: trimmed, non-empty, no '\\' or "..", not absolute ('/' start or
"X:" drive prefix); ok=false otherwise.

### `fn json_escape(s: Str) -> Str`

minimal JSON string escape (quote + backslash).

### `fn float_to_str(f: Float) -> Str`

render a Float compactly for JSON.

### `fn dock_tree_to_json(t: Tree) -> Str`

serialize one dock tree to compact JSON.

### `fn serialize_dock_state(ds: DockState) -> Str`

serialize a DockState to a JSON string (surfaces + their trees).

### `fn dp_peek(s: Str, pos: Vec<Int>) -> Int`

the byte at the cursor, or -1 at end.

### `fn dp_adv(inout pos: Vec<Int>)`

advance the cursor by one.

### `fn dp_ws(s: Str, inout pos: Vec<Int>)`

skip ASCII whitespace at the cursor.

### `fn dp_eat(s: Str, inout pos: Vec<Int>, c: Int)`

advance past `c` if it's at the cursor.

### `fn dp_string(s: Str, inout pos: Vec<Int>) -> Str`

parse a JSON string literal at the cursor.

### `fn parse_float_sub(s: Str, a: Int, b: Int) -> Float`

parse the decimal Float in s[a, b).

### `fn is_number_sub(s: Str, a: Int, b: Int) -> Bool`

is `s[a..b)` a well-formed number: optional sign, digits with at most one `.` and at least one
digit overall, an optional exponent, and nothing else?

This exists because `parse_float_sub` is best-effort: it answers 0.0 for text that is not a number
at all and has no way to say so. Anything that turns user-authored text into a length or a
coordinate has to ask this first, or a typo becomes a silent zero.

### `fn is_number(s: Str) -> Bool`

`is_number_sub` over a whole string.

### `fn dp_number(s: Str, inout pos: Vec<Int>) -> Float`

parse a number (Float) at the cursor.

### `fn dp_bool(s: Str, inout pos: Vec<Int>) -> Bool`

parse a `true`/`false` literal at the cursor.

### `fn dock_set_collapsed(n: Node, fc: Bool, sc: Bool)`

set both collapse flags on a split node.

### `fn dock_tree_parse(s: Str, inout pos: Vec<Int>) -> Tree`

rebuild a dock tree from the mini-JSON at the cursor (inverse of dock_tree_to_json).

### `fn deserialize_dock_state(str: Str) -> DockState`

parse a DockState from a string produced by serialize_dock_state.

### `fn serialize_window_manager(wm: WindowManager) -> Str`

serialize a WindowManager's windows (id/title/rect/visible/minimized/z) to compact JSON.

### `fn deserialize_window_manager(str: Str) -> WindowManager`

parse a WindowManager from a string produced by serialize_window_manager.


