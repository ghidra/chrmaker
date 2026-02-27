# chrmaker

A native NES CHR pixel art editor. Draws directly in NES 2-bit pixel format, exports raw CHR ROM binaries, and manages up to 8 NES sub-palettes with a built-in colour picker.

Built with C and SDL2. Linux/X11.

## Build

```sh
make
./chrmaker [file.chr] [COLSxROWS]
```

**Dependencies:** `gcc`, `sdl2` (install via your package manager, e.g. `pacman -S sdl2` or `apt install libsdl2-dev`).

`COLSxROWS` is optional and sets the canvas size in tiles (e.g. `16x32`). If the file already exists on disk it is loaded automatically on startup.

## File formats

| Extension | Description |
|-----------|-------------|
| `.chr` | Raw NES CHR ROM — `ntiles × 16` bytes, standard 2-bitplane format, no header |
| `.pal` | Palette sidecar — 8 sub-palettes + per-tile palette assignments. Saved and loaded automatically alongside `.chr` files |

A `.pal` file is written whenever you save a `.chr` file, and loaded automatically whenever you open one. If a `.pal` is found on open, the view switches to NES colour mode automatically.

## Controls

### Drawing

| Key / Action | Description |
|---|---|
| `0` `1` `2` `3` | Select draw colour (NES bitplane value) |
| Left-click / drag | Paint pixels |
| Right-click / drag | Assign active palette to tile(s) |

### Tile mode

Press `T` over a tile to select it and enter tile mode. The selected tile is highlighted with an amber border.

| Key | Description |
|---|---|
| `T` | Select tile under cursor |
| `W` | Cycle wrap mode: none → horizontal → vertical → both |
| `[` / `]` | Cycle tile's sub-palette |
| `Esc` | Exit tile mode |

Wrap mode is shown in the status bar as a single box — amber when tile mode is active, with green lines indicating wrap axes (`─` horizontal, `│` vertical, `┼` both).

### View

| Key | Description |
|---|---|
| `V` | Toggle grayscale / NES colour view |
| `G` | Toggle tile grid |
| `P` | Toggle pixel grid |
| `M` | Toggle sprite-8 / sprite-16 mode |
| `=` / `-` | Zoom in / out (1×–4×) |

### Files & canvas

| Key | Description |
|---|---|
| `Ctrl+S` | Save CHR to current path |
| `Ctrl+Shift+S` | Save CHR as (prompts for path) |
| `Ctrl+O` | Open CHR file (prompts for path) |
| `Ctrl+R` | Resize canvas (prompts, format `COLSxROWS`) |
| `Ctrl+Shift+P` | Save palette sidecar |
| `Ctrl+P` | Load palette from file (prompts for path) |
| Drag & drop | Open dropped `.chr` file |

### Help

`F1` or `?` — toggle in-app help overlay.

## Palette panel

The panel on the right shows 8 sub-palettes (0–3 background, 4–7 sprite). Click a row to make it the active palette. The active row is marked with `*`.

Click one of the four large swatches below the row list to select which colour slot to edit. Then click a colour in the 16×4 NES master palette picker to assign it. The picker is organised as 16 hues × 4 brightness levels.

Right-click any tile on the canvas to assign the current active palette to it. Hold and drag to paint the palette across multiple tiles.

## Sprite-16 mode

Press `M` to toggle sprite-16 mode (requires even canvas dimensions ≥ 2×2 tiles). In this mode, 4 sequential tiles are composited into a single 16×16 sprite displayed as:

```
[0] [2]
[1] [3]
```

Palette operations and tile selection apply to all 4 sub-tiles of the sprite together.

## Canvas sizing

The window resizes dynamically. Canvas size is `chr_cols × chr_rows × 8 × zoom` pixels. The palette panel widens at higher zoom levels so the colour picker remains usable. Use `Ctrl+R` to change tile dimensions at any time without losing pixel data.
