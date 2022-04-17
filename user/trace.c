#include "user.h"


int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(2, "invalid format\n");
        exit(1);
    }
    if (trace(atoi(argv[1])) < 0) {
        fprintf(2, "%s: trace failed\n", argv[0]);
        exit(1);
    }
    int i;
    char *new_argv[argc-2];
    for (i = 2; i < argc; i++) {
        new_argv[i-2] = argv[i];
    }
    exec(new_argv[0], new_argv);
    exit(0);
}