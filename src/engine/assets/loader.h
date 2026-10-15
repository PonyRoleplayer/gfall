#ifndef ENG_LOADER_H
#define ENG_LOADER_H

#include "engine/core/df.h"

/*
 * Games register the formats they know. The engine stores the function
 * and nothing else. kind is a short name the game chooses.
 * bytes may be null when the loader opens the file itself.
 * out is whatever that kind expects. The caller and the registrant
 * agree on the type. The engine does not look inside it.
 */

typedef int (*LoaderFn)(const char *name, const df_u8 *bytes, df_u32 size, void *out);

int loader_register(const char *kind, LoaderFn fn);
int loader_call(const char *kind, const char *name, const df_u8 *bytes, df_u32 size, void *out);

#endif
