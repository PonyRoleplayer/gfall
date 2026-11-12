/*
 * Host test. Engine checks first, then the one Daggerfall room.
 * Does not open Glide or a window.
 */

#include "check_util.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void check_engine(void);
int check_dagger(const char *root);
void check_ogfall(const char *root);
int check_ogfall_resume(const char *root, int slot);

int main(int argc, char **argv)
{
    const char *root;
    pid_t child;
    int status;

    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume")) { return check_ogfall_resume(argv[2],1); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-items")) { return check_ogfall_resume(argv[2],6); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-inventory")) { return check_ogfall_resume(argv[2],4); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-combat")) { return check_ogfall_resume(argv[2],5); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-resume-world")) { return check_ogfall_resume(argv[2],7); }
    if (argc == 3 && !strcmp(argv[1], "--ogfall-suite")) {
        check_ogfall(argv[2]); return check_failed() ? 1 : 0;
    }
    root = (argc > 1) ? argv[1] : "daggerfall/DAGGER/ARENA2";
    check_engine();
    if (check_dagger(root) && !check_failed()) {
        return 1;
    }
    /* Each executable owns its command table and gameplay globals. */
    fflush(0);
    child = fork();
    if (!child) { execl("/proc/self/exe","hostcheck","--ogfall-suite",root,(char *)0); _exit(127); }
    status = -1;
    if (child > 0) { waitpid(child,&status,0); }
    check_expect(child > 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0,"OGFALL's isolated game suite");
    if (check_failed()) {
        fprintf(stderr, "hostcheck failed\n");
        return 1;
    }
    printf("hostcheck ok\n");
    return 0;
}
