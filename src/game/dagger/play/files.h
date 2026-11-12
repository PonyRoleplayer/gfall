#ifndef GFALL_FILES_H
#define GFALL_FILES_H

/* Classic's six-slot save/load screen over GFALL saves. See docs/gfall.md. */
#include "game/dagger/play/save.h"
#define FILE_ACCEPT_X 112
#define FILE_REJECT_X 176
#define FILE_BUTTON_Y 120
#define FILE_BUTTON_W 32
#define FILE_BUTTON_H 16
struct PlayIn;
enum { FILES_NONE, FILES_SAVE, FILES_LOAD, FILES_CANCEL };
void files_open(int saving);
int files_frame(const struct PlayIn *input);
int files_saving(void);
int files_selected(void);
int files_editing(void);
const char *files_name(void);
const char *files_error(void);
void files_failed(const char *message);
const SaveInfo *files_info(int slot);
unsigned files_revision(void);
unsigned files_generation(void);

#endif
