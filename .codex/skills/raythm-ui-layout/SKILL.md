---
name: raythm-ui-layout
description: UI layout and drawing guidance for raythm. Use when adding, changing, reviewing, or fixing raythm UI screens, overlays, lists, dialogs, buttons, text, scroll areas, hit testing, layout constants, or visual polish, especially to avoid overlapping elements, clipped text, broken alignment, mismatched input regions, or unstable screen composition.
---

# Raythm UI Layout

## Overview

Build raythm UI from stable rectangles, shared draw helpers, and explicit layers. Preserve the existing 1920x1080 virtual-screen composition, derive child geometry from parent rectangles, and keep draw regions, hit regions, and clipped regions aligned.

## First Pass

Before editing UI code:

- Find the feature-specific layout file first: `*_layout.h` or `*_layout.cpp`.
- Find the existing view/draw helper for the same surface before adding new drawing code.
- Treat `src/ui/` as the default toolbox: `ui_layout.h`, `ui_text.h`, `ui_draw.h`, `ui_hit.h`, and `ui_clip.h`.
- Keep screen-specific geometry near the feature, not scattered through controller or scene code.
- Prefer small, named rectangles and layout functions over inline arithmetic inside draw loops.

## Coordinate Model

Use the 1920x1080 virtual coordinate system for 2D UI. Do not base layout on physical window pixels unless working on native window chrome or an explicitly physical surface.

Start from one of these roots:

- `virtual_screen::visible_rect()` when UI must respect letterboxed/pillarboxed visible bounds.
- `scene_common` screen constants or an existing `kScreenRect` when the composition intentionally uses the full design canvas.
- A feature parent rectangle, then derive all nested regions from it.

## Rectangle Rules

Derive UI regions in stages:

```cpp
const Rectangle panel = ui::place(screen, 720.0f, 420.0f,
                                  ui::anchor::center, ui::anchor::center);
const Rectangle content = ui::inset(panel, ui::edge_insets::symmetric(24.0f, 30.0f));
const ui::rect_pair rows = ui::split_rows(content, 54.0f, 18.0f);
```

Prefer:

- `ui::place` for anchored placement.
- `ui::inset` for padding.
- `ui::split_columns` and `ui::split_rows` for fixed structural splits.
- `ui::vstack`, `ui::hstack`, `ui::grid`, and fill variants for repeated rows/buttons.
- `ui::scroll_view` and `ui::vertical_scroll_metrics` for scrollable regions.

Avoid:

- Repeating the same x/y/width/height arithmetic in several draw functions.
- Placing text or hit boxes with different rectangles from the visual element.
- Encoding layout relationships only in comments.
- Letting hover/pressed visual shifts resize or move neighboring layout.

## Text

Draw text inside rectangles:

- Use `ui::draw_text_in_rect`, `ui::draw_body_text_in_rect`, or `ui::draw_display_text_in_rect` for ordinary labels.
- Use left/right aligned text rectangles instead of hand-computed `MeasureText` centering.
- Use marquee or truncation behavior already present in nearby UI when song titles, artists, usernames, file names, or localized strings can be long.
- Check Japanese/localized labels when changing button widths or compact rows.

## Shared Drawing

Use `ui_draw.h` helpers unless the surface truly needs custom rendering:

- `draw_button` / `enqueue_button` for buttons.
- `draw_row`, `draw_selectable_row`, or `enqueue_row` for list and setting rows.
- `draw_panel` and `draw_section` for framed regions.
- `draw_dropdown` / `enqueue_dropdown` and `enqueue_context_menu` for menus.
- `draw_slider_relative`, `draw_value_selector`, and scrollbar helpers for controls.

Keep drawing, hit testing, and returned commands tied to the same rectangle. Views may return commands or result structs; they should not apply unrelated state transitions just because a UI element was clicked.

## Layers And Input

Use layers for transient UI:

- Base screen content: `ui::draw_layer::base`.
- Dropdowns, context menus, and lightweight overlays: `ui::draw_layer::overlay`.
- Blocking dialogs: `ui::draw_layer::modal`.
- Debug surfaces: `ui::draw_layer::debug`.

Register hit regions for open overlays/menus so higher layers block lower-layer interactions. Use queued drawing when visual ordering must match layer ordering. Begin and flush the frame-local queues in the same pattern as nearby feature code.

## Scroll And Clip

For lists, timelines, rankings, catalogs, and panels with variable content:

- Compute a viewport rectangle separately from the container/header.
- Clamp scroll offsets to `max_scroll`.
- Draw scrollbars from `ui::vertical_scroll_metrics` or existing scrollbar helpers.
- Use `ui::scoped_clip_rect` around scrollable content so rows, album art, hover states, and text cannot spill into neighboring panels.
- Keep row height, spacing, top padding, and bottom padding as named layout constants.

## Popups And Dialogs

When adding dropdowns, context menus, tooltips, or dialogs:

- Compute the popup rectangle in a layout helper.
- Clamp popups to `kScreenRect` or `virtual_screen::visible_rect()` with edge margins.
- Use modal/overlay layers and registered hit regions.
- Keep dim overlays full-screen only when the interaction should block the full screen.
- Keep confirmation/dialog sizes stable and derive title, body, and button rows from the dialog rectangle.

## Layout Constants

Put durable constants in feature layout files when they define screen structure or cross-function relationships. Inline constants are acceptable only for local visual decoration that cannot affect overlap or hit testing.

Good candidates for layout files:

- Parent panel rectangles.
- Column widths and gaps.
- Row heights and row spacing.
- Button sizes and button gaps.
- Scroll viewport/header/bottom padding.
- Dialog/menu dimensions and clamp margins.

## Verification

When changing layout, verify more than compilation:

- Inspect nearby smoke tests for existing layout relationship assertions.
- Add focused assertions for important spacing, alignment, clamping, or content-height calculations when practical.
- Build with the repo's normal Codex build workflow if the change touches compiled code.
- For visual work, run the app or inspect the target screen when feasible, especially after changing dialogs, scroll areas, text widths, or layer behavior.

## Reference

Read `documents/ui-layout-system.md` when API details or examples are needed. Keep this skill as the compact checklist; use the document as the longer reference.
