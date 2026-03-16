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
- `main.h` — all enums (`ViewMode`, `WrapMode`, `SpriteMode`, `InputType`, `ComposeLayer`) and `EditorState`
- `panel.h` — layout constants shared by both `render.c` and `input.c` (macros only, no C code)
- `export.h/c` — `export_chr(chr, ntiles, path)` writes raw NES CHR binary
- `font.h/c` — embedded 5×7 bitmap font for overlays
- `compose.h/c` — `ComposeScene`, `ComposeData`, `compose_init`, `compose_save`, `compose_load`

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

In compose mode, `state_update_dims` uses a different path:
- `compose_canvas_w = 256 × compose_zoom`, `compose_canvas_h = 240 × compose_zoom`
- Two-column panel: CHR picker (left) at `COMPOSE_PICKER_SCALE` + controls (right)
- `win_w = compose_canvas_w + panel_w`

## Sprite-16 mode

When `sprite_mode == SPRITE_16`, 4 sequential tiles are displayed as one 16×16 sprite: layout is `[0][2] / [1][3]` (column-major). `screen_to_tile` and `sel_tile_idx` both account for this remapping. Operations that affect a selected sprite (palette assign, `[`/`]` cycling, right-click) always apply to all 4 sub-tiles.

## Compose mode

Press `Tab` to enter compose mode — a NES screen layout editor (256×240 nametable). Tiles from the CHR sheet are arranged into backgrounds and sprites respecting NES hardware constraints.

**Layout:** Compose canvas on left (256×240 scaled by `compose_zoom` 1–4×), two-column panel on right (CHR picker at 2× left, controls right).

**Layers:**
- **BG layer** (`B` key): LMB places `brush_tile` on the 32×30 nametable; auto-sets 2×2 attribute palette. RMB eyedroppers tile+palette. Shift+LMB erases.
- **Sprite layer** (`L` key): LMB places/selects/drags sprites (max 64). RMB deletes. Arrow keys nudge ±1px. H/F flip brush. Sprites support hflip/vflip and behind-BG flag.

**NES accuracy:** BG tiles cannot flip (only sprites). Attribute table is per 2×2 tile block (palettes 0–3 for BG, 4–7 for sprites).

**Scenes:** Up to 16 scenes per file. PgUp/PgDn to navigate, Ctrl+N to add. Ctrl+S saves `.scn` sidecar. Auto-loads alongside CHR file.

**File format (`.scn`):** Binary — magic `"NSCN"`, version byte, scene count, then per scene: 960B nametable + 240B attributes + sprite list.

## Clipboard (cross-instance)

Ctrl+C / Ctrl+X / Ctrl+V in tile mode uses a file-based clipboard at `/tmp/chrmaker_clipboard.bin`. This allows copy/paste between separate chrmaker instances. Format: 1 byte s16 flag + 1 or 4 tiles of raw pixel data.

## Sprite generation (Claude scripting)

`chrgen.py` converts inline JSON sprite definitions to raw NES CHR binary. **Injects** into existing files — finds the first blank tile slot and appends there, preserving all existing content. Claude pipes pixel data directly via heredoc:

```bash
python3 /home/jimmy/projects/nes/chrmaker/chrgen.py output.chr << 'EOF'
[
  { "size": 16, "pixels": ["0000011111100000", ...] },
  { "size":  8, "pixels": ["00111100", ...] }
]
EOF
```

- `size: 8` → 1 tile; `size: 16` → 4 tiles in S16 column-major order (TL/BL/TR/BR)
- Pixel values `0`–`3` are NES bitplane indices (never RGB)
- Output is always padded to 256 tiles (4096 bytes)
- Existing tiles are preserved; new sprites are injected at the first blank slot
- Status goes to stderr only; file is immediately openable with `./chrmaker output.chr`

**Color convention used by Claude when drawing:**
- `0` = transparent / background
- `1` = dark outline / shadow
- `2` = mid-tone fill (suit, skin, main body colour)
- `3` = bright highlight / accent (visor, glow, detail)

## Keyboard shortcuts (CHR editor)

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
| `Ctrl+C` / `Ctrl+V` / `Ctrl+X` | Copy / paste / cut tile (cross-instance) |
| `Ctrl+S` | Save CHR; `Ctrl+Shift+S` = save as |
| `Ctrl+O` | Open CHR file (text overlay) |
| `Ctrl+R` | Resize canvas (text overlay, format `COLSxROWS`) |
| `Ctrl+Shift+P` | Save palette to `stem.pal` (derived from current CHR path) |
| `Ctrl+P` | Load palette from file (text overlay) |
| `Tab` | Enter compose mode |
| `?` / `F1` | Toggle help overlay |
| `Escape` | Close overlay → exit tile mode → quit |

## Keyboard shortcuts (compose mode)

| Key | Action |
|-----|--------|
| `B` / `L` | Switch to BG / sprite layer |
| `LMB` | Place tile (BG) or place/select sprite (SPR) |
| `RMB` | Eyedropper (BG) or delete sprite (SPR) |
| `Shift+LMB` | Erase tile (BG) |
| `H` / `F` | Toggle brush hflip / vflip (sprite layer) |
| `[` / `]` | Cycle brush palette (BG: 0–3, SPR: 4–7) |
| `G` | Toggle attribute grid |
| `=` / `-` | Zoom in / out (1–4×) |
| `Arrow keys` | Nudge selected sprite ±1px |
| `Delete` | Delete selected sprite |
| `PgUp` / `PgDn` | Switch scenes |
| `Ctrl+N` | Add new scene |
| `Ctrl+S` | Save scene file (`.scn`) |
| `?` / `F1` | Toggle compose help overlay |
| `Tab` / `Escape` | Exit compose mode |
