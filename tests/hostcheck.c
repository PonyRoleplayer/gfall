/*
 * Host test. Engine checks first, then the one Daggerfall room.
 * Does not open Glide or a window.
 */

#include "check_util.h"

#include <stdio.h>

void check_engine(void);
int check_dagger(const char *root);
void check_play(void);

int main(int argc, char **argv)
{
    const char *root;

    root = (argc > 1) ? argv[1] : "daggerfall/DAGGER/ARENA2";
    check_engine();
    if (check_dagger(root) && !check_failed()) {
        return 1;
    }
    /* After the dagger checks: ARENA2 mounted, MAIN.DAT merged. */
    check_play();
    if (check_failed()) {
        fprintf(stderr, "hostcheck failed\n");
        return 1;
    }
    printf("hostcheck ok\n");
    return 0;
}
