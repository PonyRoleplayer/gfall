#ifndef DF_CIF_H
#define DF_CIF_H

#include "engine/core/df.h"

/*
 * Daggerfall's CIF pictures (docs/data.md). A face file is several IMG
 * pictures one after another, each a 12-byte header and uncompressed
 * pixels. A weapon file is one of those (the armed picture; WEAPON09.CIF,
 * the bow, has none) and then the swings: a 76-byte header and RLE
 * frames, as the public image notes describe them. No second IMG reader:
 * the header is the one img.c already reads, repeated.
 *
 * The armed picture is drawn in ART_PAL's grey ramp, indices 112..127
 * (UESP: iron is grey). cif_metal keeps that shade and moves the row to
 * the palette row of the material's colour (UESP's Weapons table).
 */

#define CIF_MAX 160
#define CIF_METALS 10

typedef struct CifFrame {
    int w;
    int h;
    /* 0 is the armed picture, or every frame of a face file. A swing is 1 and up. */
    int group;
    const df_u8 *pixels;
} CifFrame;

typedef struct CifSet {
    int n;
    int groups;
    CifFrame frame[CIF_MAX];
    void *heap;
} CifSet;

/* 0 when the file is missing or has no picture. */
int cif_open(const char *name, CifSet *out);
void cif_close(CifSet *set);
const CifFrame *cif_frame(const CifSet *set, int index);
/* Frames of one group. *first is the index of its first frame. */
int cif_group_span(const CifSet *set, int group, int *first);

const char *cif_metal_name(int metal);
/* The palette index to draw. Indices outside 112..127 are unchanged. */
int cif_metal(int index, int metal);

#endif
