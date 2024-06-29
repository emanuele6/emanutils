#define _GNU_SOURCE /* O_PATH */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static void
usage(void)
{
    static char const message[] =
        "Usage: openpathfd [-dL] fd file cmd [args]...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *const argv[])
{
    int openflags = O_PATH | O_NOFOLLOW;
    for (int opt; opt = getopt(argc, argv, "+dL"), opt != -1;) {
        switch (opt) {
        case 'd':
            openflags |= O_DIRECTORY;
            break;
        case 'L':
            openflags &= ~O_NOFOLLOW;
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
    char const *const path = argv[optind + 1];
    char *const *const command = &argv[optind + 2];

    char *endptr;
    errno = 0;
    long const longfd = strtol(argfd, &endptr, 10);
    if (errno) {
        perror("strtol");
        return 2;
    }
    if (endptr == argfd || longfd < 0 || longfd > INT_MAX || *endptr) {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }
    int const fd = (int)longfd;

    int openfd;
    do {
        openfd = open(path, openflags);
    } while (openfd == -1 && errno == EINTR);
    if (openfd == -1) {
        perror("open");
        return 2;
    }

    if (openfd != fd) {
        int ret;
        do {
            ret = dup2(openfd, fd);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            perror("dup2");
            return 2;
        }
        do {
            ret = close(openfd);
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
