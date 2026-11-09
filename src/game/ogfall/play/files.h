#ifndef OGFALL_FILES_H
#define OGFALL_FILES_H

/* Classic's six-slot save/load screen over OGFALL saves. See docs/ogfall.md. */
#include "game/ogfall/play/save.h"
#define OG_FILE_ACCEPT_X 112
#define OG_FILE_REJECT_X 176
#define OG_FILE_BUTTON_Y 120
#define OG_FILE_BUTTON_W 32
#define OG_FILE_BUTTON_H 16
struct OgIn;
enum { OG_FILES_NONE, OG_FILES_SAVE, OG_FILES_LOAD, OG_FILES_CANCEL };
void og_files_open(int saving);
int og_files_frame(const struct OgIn *input);
int og_files_saving(void);
int og_files_selected(void);
int og_files_editing(void);
const char *og_files_name(void);
const char *og_files_error(void);
void og_files_failed(const char *message);
const OgSaveInfo *og_files_info(int slot);
unsigned og_files_revision(void);
unsigned og_files_generation(void);

#endif
