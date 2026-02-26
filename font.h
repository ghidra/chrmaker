#pragma once
#include <SDL2/SDL.h>

/* ── Embedded 5×7 pixel font ──────────────────────────────────────
   Covers ASCII 0x20–0x5F (space through underscore).
   Lowercase letters are mapped to uppercase on draw.
   FONT_SCALE controls the render multiplier (2 = each pixel = 2×2). */
#define FONT_W      5
#define FONT_H      7
#define FONT_SCALE  2

/* Width of one character cell in screen pixels (includes 1px gap). */
int font_char_w(void);

/* Height of one text line in screen pixels (includes 1px gap). */
int font_line_h(void);

/* Draw a single character.  c is clamped to the supported range. */
void font_draw_char(SDL_Renderer *ren, char c, int x, int y, SDL_Color col);

/* Draw a NUL-terminated string.  '\n' advances to the next line.
   Lowercase characters are shown as uppercase.                    */
void font_draw_str(SDL_Renderer *ren, const char *s, int x, int y, SDL_Color col);
