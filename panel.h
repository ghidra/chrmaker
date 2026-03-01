#pragma once
#include "chr.h"   /* for PANEL_W */

/* ── Palette panel layout ─────────────────────────────────────────
   All x values are panel-relative (add CANVAS_W for screen coords).
   All y values are screen-absolute (panel top == screen y 0).
   Computed as macros so both render.c and input.c share one source
   of truth for hit-testing and drawing.                           */

/* ── Section 1: sub-palette rows (8 rows × 4 swatches) ──────────*/
#define PANEL_PAL_X0    4    /* left margin (panel-relative)        */
#define PANEL_PAL_Y0    8    /* top margin                          */
#define PANEL_PAL_SW   22    /* swatch width                        */
#define PANEL_PAL_SH   10    /* swatch height                       */
#define PANEL_PAL_XGAP  2    /* gap between swatches                */
#define PANEL_PAL_ROW  16    /* row pitch  (SH + inter-row gap)     */

/* ── Section 2: active palette large swatches ───────────────────*/
#define PANEL_ACT_Y0   (PANEL_PAL_Y0 + 8 * PANEL_PAL_ROW + 8)
#define PANEL_ACT_SW   26    /* large swatch width                  */
#define PANEL_ACT_SH   18    /* large swatch height                 */
#define PANEL_ACT_XGAP  2    /* gap between large swatches          */

/* ── Section 3: NES master palette picker (16×4 grid) ───────────
   16 columns = one per hue; 4 rows = one per brightness tier.
   idx = row * 16 + col maps directly to NES master palette index.

   Cell size scales with zoom: cell_px = zoom * PANEL_NES_CELL_BASE.
   Panel width, PANEL_VIEW_Y0, and PANEL_FULL_H are therefore all
   runtime — computed in state_update_dims() in main.c.           */
#define PANEL_NES_COLS      16
#define PANEL_NES_ROWS       4
#define PANEL_NES_GAP        1   /* px gap between cells (fixed)    */
#define PANEL_NES_CELL_BASE  3   /* cell_px = zoom * this           */
#define PANEL_NES_Y0        (PANEL_ACT_Y0 + PANEL_ACT_SH + 10)

/* ── Section 4: Animation (below help button) ────────────────────*/
/* Preview size and section height are computed at runtime from
   sprite_mode and anim_preview_zoom — see state_update_dims().   */
#define PANEL_ANIM_SCRUB_H  8    /* scrubber bar height in px (fixed)      */
