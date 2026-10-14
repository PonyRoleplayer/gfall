#ifndef DF_INPUT_H
#define DF_INPUT_H

/* One frame. move_f is W minus S, move_r is D minus A, move_y is Q minus E.
   look_dx positive is mouse right. look_dy positive is mouse down. */

typedef struct Input {
    int move_f;
    int move_r;
    int move_y;
    int look_dx;
    int look_dy;
    int quit;
} Input;

void input_begin(void *hwnd);
void input_end(void);
void input_read(Input *out);

/* Client coordinates. The window calls this from WM_MOUSEMOVE. */
void input_on_mouse(int x, int y);
void input_request_quit(void);

#endif
