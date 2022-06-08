#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "../kernel/fs.h"
#include "user.h"

void find(char *root, const char *target) {
    int fd;
    if ((fd = open(root, 0)) < 0) {
        fprintf(2, "find: can not open %s\n", root);
        return;
    }
    char buf[512], *p;
    struct dirent de;
    if ((strlen(root) + 1 + DIRSIZ + 1) > sizeof(buf)) {
        fprintf(2, "find: path too long\n");
        close(fd);
        return;
    }

    strcpy(buf, root);
    p = buf + strlen(buf);
    *p = '/';
    p++;
    struct stat st;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) {
            continue;
        }
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
            fprintf(2, "find: can not stat %s\n", buf);
            continue;
        }
        if (st.type == T_DIR && strcmp(p, ".") != 0 && strcmp(p, "..") != 0) {
            find(buf, target);
        } else if (strcmp(target, p) == 0) {
            printf("%s\n", buf);
        }
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(2, "find: <root> <filename>\n");
        exit(1);
    }

    struct stat st;
    if (stat(argv[1], &st) < 0) {
        fprintf(2, "find: can not stat %s\n", argv[1]);
        exit(1);
    }
    if (st.type != T_DIR) {
        fprintf(2, "find: %s is not dir\n", argv[1]);
        exit(1);
    }

    find(argv[1], argv[2]);
    exit(0);
}