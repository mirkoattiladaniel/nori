# std/nori_ui::core

```nori
import "std/nori_ui" as ui          // then ui::core::…
```

### `enum KeyCode`

a platform-agnostic keyboard key.

### `fn key_code_tag(k: KeyCode) -> Int`

the stable integer tag of a KeyCode (0 = Unknown; order matches the enum).

### `fn ints_contains(v: Vec<Int>, x: Int) -> Bool`

linear membership test for an Int in a Vec<Int> (internal helper for tiny key sets).

### `struct InputState`

current-frame input from the runner (mouse, scroll, keys, modifiers).

### `fn input_state_default() -> InputState`

an empty InputState (no buttons, no keys, cursor at origin).

### `enum Sense`

desired interaction level for a widget.

### `fn sense_rank(s: Sense) -> Int`

the rank of a Sense (None=0, Hover=1, Click=2, Drag=3, ClickAndDrag=4).

### `fn sense_at_least(s: Sense, other: Sense) -> Bool`

true if `self` includes at least the interaction level of `other`.
Sense ordering: ClickAndDrag covers everything;
otherwise a higher rank covers a lower one, and anything covers None.

### `struct InteractionResponse`

the result of interacting with a rect (hover, click, drag).

### `fn interaction_none() -> InteractionResponse`

a response with no interaction.

### `fn widget_id_from_str(s: Str) -> Int`

a stable widget id derived from a string (djb2 hash, masked to 63 bits).

### `struct FloatNote`

a string->float note kept in a Vec.

### `fn float_note_max(inout notes: Vec<FloatNote>, id: Str, value: Float)`

max-merge `value` into the note named `id` in `notes` (insert if absent); ignores non-positive.

### `fn float_note_get(notes: Vec<FloatNote>, id: Str) -> Float`

the value of the note named `id`, or 0.0 if absent.

### `fn float_note_set(inout notes: Vec<FloatNote>, id: Str, value: Float)`

set the note named `id` to `value` (insert if absent).

### `struct RegionScrollReq`

a per-region scroll request kept in a Vec (region id -> request).

### `struct Context`

per-frame UI context: screen rect, layout cursor, input, and draw-command buffers.

### `fn context_new(screen_rect: Rect) -> Context`

a Context for `screen_rect` with no input and empty buffers.

### `fn context_from_input(screen_rect: Rect, sink input: InputState) -> Context`

a Context for a frame built from screen rect and current input.

##### Context

### `fn begin_frame(inout self)`

reset per-frame state (layout cursor, repaint flag, draw buffers, auto ids); keeps screen_rect/input.

### `fn request_repaint(inout self)`

request a repaint next frame.

### `fn take_repaint_requested(inout self) -> Bool`

take and clear the repaint-requested flag.

### `fn begin_disabled(inout self)`

enter a disabled region (nesting supported).

### `fn end_disabled(inout self)`

leave the current disabled region.

### `fn is_disabled(self) -> Bool`

true if inside any disabled region.

### `fn widget_id(inout self) -> Int`

allocate a fresh unique widget id.

### `fn cursor_in_rect(self, rect: Rect) -> Bool`

true if the cursor is inside `rect`.

### `fn available_rect(self) -> Rect`

the rect available for layout (layout cursor to screen max).

### `fn note_panel_scroll_extent_from_top(inout self, extent: Float)`

record total scrollable height from the top of screen_rect (keeps the max seen).

### `fn scroll_extent_from_top(self) -> Float`

the reported scrollable height from screen_rect.min.y (0 if unset).

### `fn set_panel_viewport(inout self, viewport: Rect)`

set the visible panel viewport (not scroll-shifted).

### `fn panel_paint_rect(self) -> Rect`

the rect to paint into: the panel viewport if set, else available_rect.

### `fn set_host_scroll_offset(self, region_id: Str, offset: Float)`

host injects a scroll offset for a region (includes the root region).

### `fn host_scroll_offset(self, region_id: Str) -> Float`

the host scroll offset for a region (0 if none).

### `fn register_scroll_region(inout self, sink reg: RegisteredScrollRegion)`

register (or replace by id) a scroll region reported this frame.

### `fn take_registered_scroll_regions(inout self) -> Vec<RegisteredScrollRegion>`

take and clear the registered scroll regions.

### `fn panel_uses_region_scroll(self) -> Bool`

true if any registered region is not the root (i.e. nested region scrolling is in use).

### `fn note_scroll_region_extent(self, region_id: Str, extent: Float)`

record a region's content extent (keeps the max seen).

### `fn scroll_region_extent(self, region_id: Str) -> Float`

a region's recorded content extent (0 if none).

### `fn request_panel_scroll_bottom_for(inout self, region_id: Str)`

request a region scroll to bottom (also sets the root request when it's the root).

### `fn request_panel_scroll_for(inout self, region_id: Str, y: Float)`

request a region scroll to an absolute offset (also sets the root request when it's the root).

### `fn request_panel_scroll_bottom(inout self)`

request the root region scroll to bottom.

### `fn request_panel_scroll(inout self, y: Float)`

request the root region scroll to an absolute offset.

### `fn set_region_request(inout self, region_id: Str, sink req: PanelScrollRequest)`

insert/replace a region request by id (internal).

### `fn take_scroll_region_requests(inout self) -> Vec<RegionScrollReq>`

take and clear the per-region scroll requests.

### `fn interact_rect(inout self, rect: Rect, sense: Sense) -> InteractionResponse`

interact with `rect` using a fresh auto id; updates active/dragging state.

### `fn interact_rect_id(inout self, id: Int, rect: Rect, sense: Sense) -> InteractionResponse`

interact with `rect` using an explicit id, testing against the cursor.

### `fn interact_rect_id_at_pointer(inout self, id: Int, rect: Rect, sense: Sense, pointer: Vec2) -> InteractionResponse`

interact with `rect` using an explicit id, testing against `pointer`.

### `fn allocate_interact_rect(inout self, id: Int, rect: Rect, sense: Sense) -> InteractionResponse`

allocate a rect for layout and interact with it; advances the layout cursor only when Sense is None.

### `fn key_pressed(self, key: KeyCode) -> Bool`

true if `key` was pressed this frame.

### `fn key_down(self, key: KeyCode) -> Bool`

true if `key` is currently held.

### `fn mouse_pos(self) -> view Vec2`

the current mouse position.

### `fn pointer_delta(self) -> view Vec2`

the mouse delta this frame.

### `fn scroll_delta(self) -> view Vec2`

the scroll delta this frame.

### `fn record_draw(inout self, layer: PaintLayer, sink cmd: DrawCommand)`

record a command on the given layer.

### `fn push_command(inout self, sink cmd: DrawCommand)`

push a command to the main layer.

### `fn overlay_command(inout self, sink cmd: DrawCommand)`

record a command on the overlay (top) layer.

### `fn clear_draw_commands(inout self)`

clear both draw-command buffers.

### `fn draw_commands(self, layer: PaintLayer) -> view Vec<DrawCommand>`

the draw commands for a layer.

### `fn rect_filled(inout self, rect: Rect, color: Color, corner_radius: Float)`

draw a filled rect on the main layer.

### `fn rect_stroke(inout self, rect: Rect, strk: Stroke, corner_radius: Float)`

draw a stroked rect on the main layer.

### `fn gradient_rect(inout self, rect: Rect, copy tl: Color, copy tr: Color, copy bl: Color, copy br: Color, corner_radius: Float)`

draw a bilinear gradient rect on the main layer.

### `fn line(inout self, from: Vec2, to: Vec2, strk: Stroke)`

draw a line on the main layer.

### `fn circle_filled(inout self, center: Vec2, radius: Float, color: Color)`

draw a filled circle on the main layer.

### `fn circle_stroke(inout self, copy center: Vec2, radius: Float, strk: Stroke)`

draw a stroked circle on the main layer.

### `fn text(inout self, pos: Vec2, text: Str, color: Color, size: Float)`

draw text on the main layer.

### `fn text_styled(inout self, sink cmd: TextCommand)`

draw a full TextCommand (with outline/gradient/font) on the main layer.

### `fn image_png(inout self, rect: Rect, path: Str)`

draw a PNG letterboxed into `rect` on the main layer.

### `fn overlay_rect_filled(inout self, rect: Rect, color: Color, corner_radius: Float)`

draw a filled rect on the overlay layer.

### `fn overlay_rect_stroke(inout self, rect: Rect, strk: Stroke, corner_radius: Float)`

draw a stroked rect on the overlay layer.

### `fn overlay_line(inout self, from: Vec2, to: Vec2, strk: Stroke)`

draw a line on the overlay layer.

### `fn overlay_text(inout self, pos: Vec2, text: Str, color: Color, size: Float)`

draw text on the overlay layer.

### `fn allocate_rect(inout self, size: Vec2) -> Rect`

allocate a rect at the layout cursor and advance the cursor right (same row).

### `fn allocate_rect_with_spacing(inout self, size: Vec2, spacing_x: Float, spacing_y: Float) -> Rect`

allocate a rect with trailing spacing; cursor advances right by width+spacing_x and down to rect bottom+spacing_y.

### `fn allocate_rect_at(self, min: Vec2, size: Vec2) -> Rect`

allocate a rect at an explicit position without moving the layout cursor.


### `enum PaintLayer`

render layer: Main (base UI) draws first, Overlay (floating UI) on top.

### `fn paint_layer_tag(l: PaintLayer) -> Int`

the integer tag of a PaintLayer (0 = Main, 1 = Overlay).

### `struct ClipRegion`

an axis-aligned clip rect with optional rounded corners.

### `struct StyledTextRun`

a contiguous text substring with its own color (concatenate runs for a full line).

### `struct TextGradientFill`

a horizontal or vertical color ramp applied across a label's layout bounds.

### `struct RectDropShadow`

a drop shadow for a filled rect (blur + spread + offset around `body_rect`).

### `struct GradientRectCommand`

a bilinear (four-corner) rectangle gradient.

### `struct BorderDash`

a CSS-like dash pattern for stroked rects (dasharray + dashoffset).

### `struct TextCommand`

a text draw command. Optional features are gated by `has_*` flags; `styled_runs`
is empty when a single solid color is used.

### `fn text_cmd(copy pos: Vec2, text: Str, copy color: Color, size: Float) -> TextCommand`

a plain text command (single color, no outline/gradient/clip/custom font).

### `struct RectFilledCmd`

payload for DrawCommand::DcRectFilled.

### `struct RectStrokeCmd`

payload for DrawCommand::DcRectStroke.

### `struct LineCmd`

payload for DrawCommand::DcLine.

### `struct CircleFilledCmd`

payload for DrawCommand::DcCircleFilled.

### `struct CircleStrokeCmd`

payload for DrawCommand::DcCircleStroke.

### `struct ImageCmd`

payload for DrawCommand::DcImage (RGBA PNG path, host-resolved).

### `enum DrawCommand`

a backend-agnostic drawing command (one variant per primitive kind).

### `fn draw_command_tag(c: DrawCommand) -> Int`

the integer kind tag of a DrawCommand (stable order matching the enum).

### `fn dc_rect_filled(rect: Rect, copy color: Color, corner_radius: Float) -> DrawCommand`

a simple filled-rect command (no shadow, no clip, square corners by default).

### `fn dc_rect_stroke(rect: Rect, strk: Stroke, corner_radius: Float) -> DrawCommand`

a stroked-rect command (solid, no dash).

### `fn dc_line(copy from: Vec2, copy to: Vec2, strk: Stroke) -> DrawCommand`

a line command.

### `fn dc_circle_filled(copy center: Vec2, radius: Float, copy color: Color) -> DrawCommand`

a filled-circle command.

### `fn dc_text(sink t: TextCommand) -> DrawCommand`

a text draw command wrapping a plain TextCommand.

### `fn dc_gradient_rect(rect: Rect, copy tl: Color, copy tr: Color, copy bl: Color, copy br: Color, corner_radius: Float) -> DrawCommand`

a 4-corner gradient-rect command.


### `fn serialize_theme(t: Theme) -> Str`

serialize a Theme to a JSON string (Preferences -> Theme save).

### `struct ThemeLoad`

the deserialize outcome: ok=false (with the default theme) on malformed input.

### `fn deserialize_theme(src: Str) -> ThemeLoad`

deserialize a Theme from serialize_theme's JSON.


### `fn panel_scroll_root() -> Str`

the region id for the single-viewport root panel scroll.

### `enum ScrollAxis`

scroll axis for a region.

### `fn scroll_axis_tag(a: ScrollAxis) -> Int`

the integer tag of a ScrollAxis (0 = Y, 1 = X).

### `enum OverflowMode`

overflow behaviour (CSS overflow-*).

### `fn overflow_scrolls(m: OverflowMode) -> Bool`

true when content can scroll (Auto or Scroll).

### `fn overflow_clips(m: OverflowMode) -> Bool`

true when content outside the viewport should be clipped (Hidden, Auto, or Scroll).

### `struct PanelScrollRequest`

a scroll-position request: an absolute offset, or "scroll to bottom".

### `fn scroll_to(px: Float) -> PanelScrollRequest`

request scrolling to an absolute offset.

### `fn scroll_to_bottom() -> PanelScrollRequest`

request scrolling to the bottom.

### `struct RegisteredScrollRegion`

one scroll region reported during a panel frame.

### `struct ScrollBarKey`

a key for persistent per-tab scroll state in the host runner.

### `fn scrollbar_key_root(tab_id: Str) -> ScrollBarKey`

a ScrollBarKey for a tab's root scroll region.


### `fn rem_euclid(x: Float, m: Float) -> Float`

floored modulo: the result is in [0, m) for positive m.

### `fn push_solid_rect_stroke(rect: Rect, stroke_w: Float, inout out: Vec<Rect>)`

push the four solid edge rects of a `stroke_w`-thick border around `rect` into `out`.

### `fn stroke_segment_bounds(x0: Float, y0: Float, x1: Float, y1: Float, ux: Float, uy: Float, hw: Float) -> Rect`

bounding rect of a stroked segment from (x0,y0)-(x1,y1) with unit dir (ux,uy) and half-width hw.

### `fn emit_dashed_edge(x0: Float, y0: Float, x1: Float, y1: Float, stroke_w: Float, dash: BorderDash, edge_start: Float, inout out: Vec<Rect>)`

emit dash-segment fill rects along one edge into `out`.

### `fn push_dashed_rect_stroke(rect: Rect, stroke_w: Float, dash: BorderDash, inout out: Vec<Rect>)`

push fill rects approximating a dashed border around `rect`'s perimeter into `out`.
Falls back to a solid stroke when the dash pattern is degenerate.


### `struct OptColor`

an optional color: `color` is meaningful only when `present` is true.

### `fn some_color(copy c: Color) -> OptColor`

a present OptColor.

### `fn opt_color_copy(o: OptColor) -> OptColor`

an OptColor of its own with `o`'s state and colour.

### `fn no_color() -> OptColor`

an absent OptColor (color is a placeholder).

### `struct Margin`

per-edge margin (left, right, top, bottom).

### `fn margin_zero() -> Margin`

zero margin.

### `fn margin_same(v: Float) -> Margin`

the same margin on all four edges.

### `fn margin_symmetric(h: Float, v: Float) -> Margin`

symmetric margin: horizontal `h` (left/right), vertical `v` (top/bottom).

### `struct Stroke`

a stroke: line width plus color, for borders and outlines.

### `fn stroke_copy(s: Stroke) -> Stroke`

a stroke of its own with `s`'s width and colour, for a field or a return that must not share them.

### `fn stroke(width: Float, copy color: Color) -> Stroke`

construct a stroke.

### `fn stroke_default() -> Stroke`

the default stroke (width 1, black).

### `struct CornerRadius`

per-corner radius (nw, ne, sw, se).

### `fn corner_same(r: Float) -> CornerRadius`

the same radius on all corners.

### `fn corner_copy(c: CornerRadius) -> CornerRadius`

a CornerRadius of its own with `c`'s four radii.

### `struct WidgetVisuals`

the look of a single widget state (fills, strokes, rounding, hover expansion).

### `fn widget_visuals_default() -> WidgetVisuals`

the default widget visuals.

### `struct Widgets`

widget visuals for every interaction state.

### `fn widget_visuals_copy(w: WidgetVisuals) -> WidgetVisuals`

a WidgetVisuals of its own with `w`'s fills, strokes and rounding.

### `fn widgets_copy(w: Widgets) -> Widgets`

a Widgets of its own with `w`'s five states.

### `fn widgets_default() -> Widgets`

the default set of per-state widget visuals.

### `struct Visuals`

high-level visuals: panel/window fills, text/selection, and dock chrome colors.

### `fn visuals_copy(v: Visuals) -> Visuals`

a Visuals of its own with every colour, stroke and rounding of `v`.

### `fn visuals_default() -> Visuals`

the default dark visuals.

### `struct Spacing`

spacing and minimum sizes used by widget and dock layout.

### `fn spacing_default() -> Spacing`

the default spacing.

### `struct FontId`

a font identifier: point size plus family name ("Proportional" / "Monospace" / custom).

### `fn font_proportional(size: Float) -> FontId`

a proportional font of the given size.

### `fn font_monospace(size: Float) -> FontId`

a monospace font of the given size.

### `fn font_default() -> FontId`

the default font (proportional 13).

### `struct TextStyles`

the resolved FontId for each named text style.

### `struct Theme`

the full theme: visuals, widget visuals, spacing, named text styles, and animation time.

### `fn default_text_styles() -> TextStyles`

the default text-style -> FontId set (Small/Body/Button/Heading/Monospace, no custom styles).

##### TextStyles

### `fn put_style(inout self, name: Str, sink font: FontId)`

register or replace a custom named style.

### `fn get(self, name: Str) -> Option<FontId>`

resolve a style name to its FontId; Option::None if unknown.

### `fn theme_default() -> Theme`

the default theme (delegates to the dark editor chrome, like Theme::default_dark).

### `fn theme_engine_dark() -> Theme`

the dark editor chrome (deep blue-gray surfaces, bright blue active controls).

### `fn theme_default_dark() -> Theme`

the default dark editor theme (alias of theme_engine_dark).

### `fn theme_default_light() -> Theme`

a VS Code-style light theme.

##### Theme

### `fn resolve_text_style(self, style: Str) -> Option<FontId>`

resolve a text-style name to its FontId; Option::None if the name is unknown.

### `fn scale(self, scale_factor: Float) -> Theme`

a copy with spacing and font sizes multiplied by `scale_factor` (clamped to >= 0.25).

### `fn margin_scale(m: Margin, f: Float) -> Margin`

multiply every edge of a margin by `f` (internal helper).

### `fn font_scale(fid: FontId, f: Float) -> FontId`

scale a FontId's size by `f` (clamped to >= 8), keeping its family (internal helper).


### `struct Vec2`

2D vector (position, size, or delta).

### `fn vec2(x: Float, y: Float) -> Vec2`

construct a Vec2 from components.

### `fn vec2_zero() -> Vec2`

the zero vector (0, 0).

### `fn vec2_splat(v: Float) -> Vec2`

a vector with both components set to `v`.

##### Vec2

### `fn add(inout self, o: Vec2) -> Vec2`

component-wise sum.

### `fn sub(self, o: Vec2) -> Vec2`

component-wise difference.

### `fn mul(self, s: Float) -> Vec2`

scale by a scalar.

### `fn div(self, s: Float) -> Vec2`

divide by a scalar.

### `fn length(self) -> Float`

Euclidean length.

### `fn normalize(self) -> Vec2`

unit vector, or zero if length is zero.

### `fn eq(self, o: Vec2) -> Bool`

true when both components equal the other's.

### `struct Rect`

axis-aligned rectangle stored as min corner + max corner.

### `fn rect_min_size(copy min: Vec2, size: Vec2) -> Rect`

rectangle from a min corner and a size.

### `fn rect_min_max(copy min: Vec2, copy max: Vec2) -> Rect`

rectangle from min and max corners.

### `fn rect_copy(r: Rect) -> Rect`

a rectangle of its own with `r`'s corners, for a field or a return that must not share them.

##### Rect

### `fn width(self) -> Float`

width (max.x - min.x).

### `fn height(self) -> Float`

height (max.y - min.y).

### `fn size(self) -> Vec2`

size as a Vec2.

### `fn center(self) -> Vec2`

center point.

### `fn inset(self, left: Float, right: Float, top: Float, bottom: Float) -> Rect`

shrink by per-edge insets; size clamps to non-negative.

### `fn expand(self, left: Float, right: Float, top: Float, bottom: Float) -> Rect`

expand by per-edge amounts (positive grows); size clamps to non-negative.

### `fn contains(self, p: Vec2) -> Bool`

true if point `p` is inside (inclusive of edges).

### `fn translate(inout self, d: Vec2) -> Rect`

translate by a delta.

### `fn union(self, o: Rect) -> Rect`

smallest rect containing both this and `o`.

### `fn overlaps(self, o: Rect) -> Bool`

true if this rect overlaps `o` (positive area intersection).

### `fn intersect(self, o: Rect) -> Option<Rect>`

intersection rect; Option::None when they do not overlap.

### `fn shrink_to_avoid_overlay(self, overlay: Rect) -> Rect`

shrink `self` to the larger full-width strip above/below `overlay` that avoids it. Returns self
unchanged when they don't overlap or when `overlay` spans the full height.

### `struct Color`

RGBA color, 0..=255 per channel.

### `fn rgba_pack(c: Color) -> Int`

a colour packed into one Int, 0xRRGGBBAA. A document's nodes keep their colours this way: a `Color`
is a record of its own (40 bytes and an allocation), and a node has fifteen of them.

### `fn rgba_color(v: Int) -> Color`

the Color a packed 0xRRGGBBAA Int holds.

### `fn rgba_white() -> Int`

packed opaque white / transparent black

### `fn color_rgb(r: Int, g: Int, b: Int) -> Color`

opaque color from RGB (alpha = 255).

### `fn color_rgba(r: Int, g: Int, b: Int, a: Int) -> Color`

color from RGBA.

### `fn color_transparent() -> Color`

fully transparent black.

### `fn color_white() -> Color`

opaque white.

### `fn color_black() -> Color`

opaque black.

### `fn color_gray(v: Int) -> Color`

opaque gray (r=g=b=v).

##### Color

### `fn with_alpha(self, a: Int) -> Color`

copy with a new alpha.

### `fn lerp(self, o: Color, t: Float) -> Color`

linear interpolation toward `o`; `t` clamped to [0, 1].

### `fn scale_alpha(self, scale: Float) -> Color`

copy with alpha multiplied by `scale` (clamped to [0, 1]); truncates toward zero.

### `fn eq(self, o: Color) -> Bool`

true if all channels match.

### `struct VisualTransform`

a per-node visual transform: uniform scale about the rect center + a vertical translation.

### `fn visual_transform_identity() -> VisualTransform`

the identity transform (scale 1, no translation).

### `fn transform_rect(rect: Rect, transform: VisualTransform) -> Rect`

apply `transform` to `rect`: scale about the center (floored at 0.01) then translate vertically.

### `fn lerp_ch(a: Int, b: Int, t: Float) -> Int`

interpolate one 0..=255 channel and round to Int (internal helper).

### `struct Hsv`

HSV color: H in [0, 360), S and V in [0, 1].

### `fn rgb_to_hsv(r: Int, g: Int, b: Int) -> Hsv`

RGB (0..=255) to HSV.

### `fn hsv_to_rgb(h: Float, s: Float, v: Float) -> Color`

HSV to RGB (0..=255).

### `fn chan_255(x: Float) -> Int`

scale a [0,1] float to a clamped 0..=255 Int (internal helper).


