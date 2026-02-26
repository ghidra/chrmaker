#pragma once
#include "chr.h"

/* Writes raw NES CHR binary to path.
   Format: ntiles × 16 bytes, no header.
   Each tile: 8 bytes bitplane-0, 8 bytes bitplane-1.
   Returns 0 on success, -1 on error. */
int export_chr(const ChrPage *chr, int ntiles, const char *path);
