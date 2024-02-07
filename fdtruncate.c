#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

int
main(int const argc, char *const argv[])
{
    if (argc <= 1) {
        if (EOF == fputs("Usage: fdtruncate fd [length] [cmd]...\n",
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
    if (longfd < 0 || longfd > INT_MAX || *endptr) {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }
    int const fd = (int)longfd;

    off_t length;
    if (argc <= 2) {
        length = 0;
    } else {
        long const longlength = strtol(argv[2], &endptr, 10);
        if (errno) {
            perror("strtol");
            return 2;
        }
        if (longlength < 0 || *endptr) {
            if (fputs("Invalid length.\n", stderr) == EOF)
                perror("fputs");
            return 2;
        }
        length = (off_t)longlength;
    }

    while (ftruncate(fd, length) == -1) {
        if (errno != EINTR) {
            perror("ftruncate");
            return 2;
        }
    }

    if (argc <= 3)
        return 0;

    (void)execvp(argv[3], &argv[3]);
    perror("execvp");
    return 2;
}
