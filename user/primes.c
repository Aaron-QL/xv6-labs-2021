#include "../kernel/types.h"
#include "user.h"

#define RD 0
#define WR 1

void primes(int left_pipe[2])
{
    int first;
    // 从左边的管道读一个数，必然是质数，直接打印
    if (read(left_pipe[RD], &first, sizeof(int)) == sizeof(int)) {
        fprintf(1, "prime %d\n", first);
    } else {
        close(left_pipe[RD]);
        exit(0);
    }
    int right_pipe[2], next;
    pipe(right_pipe);
    // 剩下的都取出来跟第一个数取模，不能整除的放进右管道
    while (read(left_pipe[RD], &next, sizeof(int)) != 0) {
        if (next % first) {
            write(right_pipe[WR], &next, sizeof(int));
        }
    }
    close(left_pipe[RD]);
    close(right_pipe[WR]);
    int pid;
    // 开一个新进程递归此函数，把右管道当作左管道再来一次
    if ((pid = fork()) < 0) {
        close(right_pipe[RD]);
        exit(1);
    } else if (pid == 0) {
        primes(right_pipe);
    } else {
        close(right_pipe[RD]);
        wait(0);
    }
    exit(0);
}

int main()
{
    int p[2];
    pipe(p);
    for (int i = 2; i < 36; i++) {
        write(p[WR], &i, sizeof(int));
    }
    close(p[WR]);
    int pid;
    if ((pid = fork()) < 0) {
        close(p[RD]);
        fprintf(2, "fork error");
        exit(1);
    } else if (pid == 0) {
        primes(p);
    } else {
        close(p[RD]);
        wait(0);
    }
    exit(0);
}