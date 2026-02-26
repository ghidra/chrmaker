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

/* ── Section 3: NES master palette picker (8×8 grid) ────────────*/
#define PANEL_NES_SZ   13    /* each cell size (square)             */
#define PANEL_NES_GAP   1    /* 1px gap between cells               */
#define PANEL_NES_STEP (PANEL_NES_SZ + PANEL_NES_GAP)   /* 14 px  */
/* Centred horizontally within the panel: */
#define PANEL_NES_X0   ((PANEL_W - 8 * PANEL_NES_STEP + PANEL_NES_GAP) / 2)
#define PANEL_NES_Y0   (PANEL_ACT_Y0 + PANEL_ACT_SH + 10)

/* ── Section 4: view-mode indicator ─────────────────────────────*/
#define PANEL_VIEW_Y0  (PANEL_NES_Y0 + 8 * PANEL_NES_STEP + 10)

/* ── Minimum window height to show the full panel ────────────────
   Used by state_update_dims to ensure the window is tall enough
   to display the ? button and view-mode indicator.               */
#define PANEL_FULL_H   (PANEL_VIEW_Y0 + 44)
