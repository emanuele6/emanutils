#define _GNU_SOURCE /* memfd_create */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

int
main(int const argc, char *const argv[])
{
    if (argc <= 3) {
        if (EOF == fputs("Usage: creatememfd fd name cmd [args]...\n",
                         stderr)) {
            perror("fputs");
        }
        return 2;
    }

    char *endptr;
    errno = 0;
    long const longfd = strtol(argv[1], &endptr, 10);
    if (errno) {
        perror("strtol");
        return 2;
    }
    if (longfd < 0 || longfd > INT_MAX || *endptr != '\0') {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }
    int const fd = (int)longfd;

    int const memfd = memfd_create(argv[2], 0);
    if (memfd == -1) {
        perror("memfd_create");
        return 2;
    }

    if (memfd != fd) {
        int ret;
        do {
            ret = dup2(memfd, fd);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            perror("dup2");
            return 2;
        }
        do {
            ret = close(memfd);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            perror("close");
            return 2;
        }
    }

    (void)execvp(argv[3], &argv[3]);
    perror("execvp");
    return 2;
}
