#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

static void
usage(void)
{
    static char const um[] = "Usage: fdtruncate fd [length] [cmd]...\n";
    if (fputs(um, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *const *const argv)
{
    for (int opt; opt = getopt(argc, argv, "+"), opt != -1;) {
        switch (opt) {
        default:
            usage();
            return 2;
        }
    }
    if (argc - optind < 1) {
        usage();
        return 2;
    }

    char *const fdstr = argv[optind];
    char *endptr;
    errno = 0;
    long const longfd = strtol(fdstr, &endptr, 10);
    if (errno) {
        perror("strtol");
        return 2;
    }
    if (endptr == fdstr || longfd < 0 || longfd > INT_MAX || *endptr) {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }
    int const fd = (int)longfd;

    off_t length;
    if (argc - optind < 2) {
        length = 0;
    } else {
        long const longlength = strtol(argv[optind + 1], &endptr, 10);
        if (errno) {
            perror("strtol");
            return 2;
        }
        if (endptr == argv[optind + 1] || longlength < 0 || *endptr) {
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

    if (argc - optind < 3)
        return 0;

    (void)execvp(argv[optind + 2], &argv[optind + 2]);
    perror("execvp");
    return 2;
}
