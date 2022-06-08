#include "../kernel/types.h"
#include "../kernel/param.h"
#include "user.h"

int readline(int fd, void *ptr, int max_len) {
    char c, *new_ptr = ptr;
    int n, i;
    for (i = 0; i < max_len; i++) {
        n = read(fd, &c, sizeof(char));
        if (n < 0) {
            fprintf(2, "read error");
            exit(1);
        } else if (n == 0) {
            return i;
        }
        if (c == '\n') {
            *new_ptr = 0;
            return i;
        } else {
            *new_ptr = c;
        }
        new_ptr++;
    }
    return i;
}

int main(int argc, char *argv[])
{
    char buf[MAXARG], *cmd;
    int n, pid, new_argc = argc+1;
    if (argc == 1) {
        cmd = "echo";
        new_argc++;
    } else {
        cmd = argv[1];
    }
    char *new_argv[new_argc];
    new_argv[0] = cmd;
    for (int i = 2; i < argc; i++) {
        new_argv[i-1] = argv[i];
    }
    new_argv[new_argc-2] = buf;
    new_argv[new_argc-1] = 0;

    while ((n = readline(0, buf, MAXARG)) > 0) {
        fprintf(1, "read: %s\n", buf);
        if ((pid = fork()) < 0) {
            fprintf(2, "fork error");
        } else if (pid == 0) {
            if (exec(cmd, new_argv) != 0) {
                fprintf(2, "exec error");
                exit(1);
            }
        } else {
            wait(0);
        }
    }
    exit(0);
}