#include "user.h"


int main(int argc, char *argv[])
{
    struct sysinfo si;
    if (sysinfo(&si) < 0) {
        fprintf(2, "sysinfo error");
        exit(1);
    }
    printf("free memory %d bytes, %d procs\n", si.freemem, si.nproc);
    exit(0);
}