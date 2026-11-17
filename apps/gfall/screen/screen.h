#ifndef GFALL_APP_SCREEN_H
#define GFALL_APP_SCREEN_H

const char *gfall_screen_boot(void);
void gfall_screen_load_classes(void);
void gfall_screen_enter_world(void);
void gfall_screen_leave_world(void);
void gfall_screen_talk_name(const char *name);
void gfall_screen_talk_face(int archive, int record);
int gfall_talk_width(const char *text);
void gfall_screen_install_draw(void);
void gfall_screen_install_maps(void);
void gfall_screen_install_background(void);

#endif
