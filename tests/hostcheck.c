/*
 * Host test. Engine checks first, then the one Daggerfall room.
 * Does not open Glide or a window.
 */

#include "check_util.h"

#include <stdio.h>
#include <string.h>

void check_engine(void);
int check_dagger(const char *root);
void check_play(void);
void check_ogfall(const char *root);
int check_ogfall_resume(const char *root, int slot);

int main(int argc, char **argv)
{
    const char *root;

    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume")) { return check_ogfall_resume(argv[2],1); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-items")) { return check_ogfall_resume(argv[2],6); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-inventory")) { return check_ogfall_resume(argv[2],4); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-combat")) { return check_ogfall_resume(argv[2],5); }
    root = (argc > 1) ? argv[1] : "daggerfall/DAGGER/ARENA2";
    check_engine();
    if (check_dagger(root) && !check_failed()) {
        return 1;
    }
    /* After the dagger checks: ARENA2 mounted, MAIN.DAT merged. */
    check_play();
    check_ogfall(root);
    if (check_failed()) {
        fprintf(stderr, "hostcheck failed\n");
        return 1;
    }
    printf("hostcheck ok\n");
    return 0;
}
