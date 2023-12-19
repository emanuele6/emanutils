#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

static bool
str2posint(int *const intp, const char string[], const char error[])
{
    char *endptr;
    errno = 0;
    long const longnum = strtol(string, &endptr, 10);
    if (errno) {
        perror("strtol");
        return false;
    }
    if (longnum < 0 || longnum > INT_MAX || *endptr != '\0') {
        if (fputs(error, stderr) == EOF)
            perror("fputs");
        return false;
    }
    *intp = (int)longnum;
    return true;
}

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

    int fd;
    if (!str2posint(&fd, argv[1], "Invalid fd.\n"))
        return 2;

    off_t length;
    int lengthint;
    if (argc <= 2)
        length = 0;
    else if (str2posint(&lengthint, argv[2], "Invalid length.\n"))
        length = (int)lengthint;
    else
        return 2;

    for (;;) {
        int const ret = ftruncate(fd, length);
        if (ret != -1)
            break;
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
