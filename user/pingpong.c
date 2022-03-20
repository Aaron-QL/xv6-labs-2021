//#include "user/user.h"
#include "user.h"

#define RD 0 //pipe的read
#define WR 1

int main() {
    int pid, fd_p2c[2], fd_c2p[2];
    pipe(fd_p2c);
    pipe(fd_c2p);
    char buf;

    if ((pid = fork()) < 0) {
        fprintf(2, "fork failed");
        exit(1);
    } else if (pid == 0) {
        if (read(fd_p2c[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child read error\n");
            exit(1);
        }
        printf("%d: received ping\n", getpid());
        if (write(fd_c2p[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "child write error\n");
            exit(1);
        }
        exit(0);
    } else {
        if (write(fd_p2c[WR], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent write error\n");
            exit(1);
        }
        if (read(fd_c2p[RD], &buf, sizeof(char)) != sizeof(char)) {
            fprintf(2, "parent read error\n");
            exit(1);
        }
        printf("%d: received pong\n", getpid());
        exit(0);
    }
}