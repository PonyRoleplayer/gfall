#ifndef DATFALL_BEAST_H
#define DATFALL_BEAST_H

#include "view.h"

/*
 * One creature's pictures, from its ENEM row (docs/data.md, docs/entities.md).
 * Walk, attack and hurt are five records each (the five views). Stand is
 * records 15..19, and only when the row says it has them. The corpse is
 * one record in corpse_archive. Archive 0 is no picture (the horse).
 */

#define BEAST_POSES 5

typedef struct BeastPose {
    char label[16];
    int archive;
    int record;
    int views;
} BeastPose;

int beast_poses(const DatFile *f, int row, BeastPose *out, int cap);

#endif
