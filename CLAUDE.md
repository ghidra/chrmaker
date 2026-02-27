# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

```bash
make          # build ./chrmaker (gcc + SDL2)
make clean    # remove binary
```

Run: `./chrmaker [file.chr] [COLSxROWS]`
Example: `./chrmaker output.chr 16x32`

No tests — verification is manual or via quick e2e checks (write a .chr, re-open it, confirm round-trip).

## Architecture

Single-process SDL2 app. All mutable state lives in one `EditorState` struct (`main.h`). The main loop in `main.c` calls three things per tick: `input_handle`, file-op logic, then `render_frame`.

**Data flow:**
- `input.c` writes to `EditorState` fields (never touches SDL renderer directly)
- `main.c` acts on deferred flags (`want_save`, `want_load`, `want_resize`) between input and render
- `render.c` reads `EditorState` as `const` — pure output, no side effects

**Key files:**
- `chr.h/c` — `ChrPage` pixel data, `PaletteState`, `chr_load`, `chr_init`
- `main.h` — all enums (`ViewMode`, `WrapMode`, `SpriteMode`, `InputType`) and `EditorState`
- `panel.h` — layout constants shared by both `render.c` and `input.c` (macros only, no C code)
- `export.h/c` — `export_chr(chr, ntiles, path)` writes raw NES CHR binary
- `font.h/c` — embedded 5×7 bitmap font for overlays

## NES CHR format

- Pixel values are **2-bit indices 0–3** (NES bitplane), never RGB
- 16 bytes per tile: 8 bytes bitplane-0, then 8 bytes bitplane-1
- bp0 row r: bit `(7-c)` = `px[t][r][c] & 1`; bp1: `(px[t][r][c] >> 1) & 1`
- `export_chr` takes `ntiles = chr_cols * chr_rows`; `chr_load` returns tile count

## Window & canvas sizing

All dimensions are runtime — no compile-time canvas constants. Call `state_update_dims(s)` whenever `chr_cols`, `chr_rows`, or `zoom` change, then set `s->want_resize = true` to trigger `SDL_SetWindowSize` + `render_resize` in the main loop.

- `canvas_w = chr_cols × TILE_W × zoom`
- `win_w = canvas_w + PANEL_W` (128 px panel, always right of canvas)
- `win_h = max(canvas_h, PANEL_FULL_H) + STATUS_H`

## Sprite-16 mode

When `sprite_mode == SPRITE_16`, 4 sequential tiles are displayed as one 16×16 sprite: layout is `[0][2] / [1][3]` (column-major). `screen_to_tile` and `sel_tile_idx` both account for this remapping. Operations that affect a selected sprite (palette assign, `[`/`]` cycling, right-click) always apply to all 4 sub-tiles.

## Keyboard shortcuts (reference)

| Key | Action |
|-----|--------|
| `0`–`3` | Set draw colour |
| `T` | Select tile under cursor (enter tile mode) |
| `W` | Cycle wrap mode (none → H → V → both) |
| `M` | Toggle sprite-8 / sprite-16 mode |
| `G` / `P` | Toggle tile / pixel grid |
| `V` | Toggle grayscale ↔ NES colour view |
| `=` / `-` | Zoom in / out (1–4×) |
| `[` / `]` | Cycle tile's sub-palette |
| `Ctrl+S` | Save CHR; `Ctrl+Shift+S` = save as |
| `Ctrl+O` | Open CHR file (text overlay) |
| `Ctrl+R` | Resize canvas (text overlay, format `COLSxROWS`) |
| `Ctrl+Shift+P` | Save palette to `stem.pal` (derived from current CHR path) |
| `Ctrl+P` | Load palette from file (text overlay) |
| `?` / `F1` | Toggle help overlay |
| `Escape` | Close overlay → exit tile mode → quit |
