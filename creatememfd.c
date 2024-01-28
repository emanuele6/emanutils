#define _GNU_SOURCE /* memfd_create, MFD_ALLOW_SEALING */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <unistd.h>

static void
usage()
{
    static char const message[] =
        "Usage: creatememfd [-S] fd name cmd [args]...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *argv[])
{
    unsigned memfdflags = 0;

    for (int opt; opt = getopt(argc, argv, "+S"), opt != -1;) {
        switch (opt) {
        case 'S':
            memfdflags |= MFD_ALLOW_SEALING;
            break;
        default:
            usage();
            return 2;
        }
    }

    if (argc - optind < 3) {
        usage();
        return 2;
    }

    char const *const argfd = argv[optind];
    char const *const name = argv[optind + 1];
    char *const *const command = &argv[optind + 2];

    char *endptr;
    errno = 0;
    long const longfd = strtol(argfd, &endptr, 10);
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

    int const memfd = memfd_create(name, memfdflags);
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

    (void)execvp(*command, command);
    perror("execvp");
    return 2;
}
