#ifndef DF_NAMEGEN_H
#define DF_NAMEGEN_H

#include "engine/core/df.h"

/*
 * NAMEGEN.DAT: the pieces classic builds people's names from. Read
 * docs/data.md (NAME) before editing.
 *
 * Eleven banks, one after another from the start, 48 bytes each: six sets'
 * offsets (u32) then six counts (u32); a set not used has offset and count 0.
 * The banks are Daggerfall Unity's NameHelper.BankTypes: Breton, Redguard,
 * Nord, Dark Elf, High Elf, Wood Elf, Khajiit, Imperial (where Argonians'
 * would be), and three of monsters'. A set is its count of pieces, each 10
 * bytes, the text padded with zeros.
 *
 * Plain C: hostcheck reads the file.
 */

#define NAMEGEN_BANKS 11
#define NAMEGEN_SETS 6
#define NAMEGEN_PIECE 10

typedef struct NamegenPiece {
    int bank;
    int set;
    int index;
    char text[NAMEGEN_PIECE + 1];
} NamegenPiece;

/* The pieces of the file's bytes, into out (cap of them): how many; 0 when the file is short or wrong. */
int namegen_read(const df_u8 *bytes, df_u32 n, NamegenPiece *out, int cap);

#endif
