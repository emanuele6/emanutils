#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

static void
usage(void)
{
    static char const message[] =
        "Usage: pidfdgetfd pidfd targetfd fd cmd [args]...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *const argv[const])
{
    for (int opt; opt = getopt(argc, argv, "+"), opt != -1;) {
        switch (opt) {
        default:
            usage();
            return 2;
        }
    }

    if (argc - optind < 4) {
        usage();
        return 2;
    }

    int pidfd, targetfd, fd;
    for (int i = 0; i < 3; ++i) {
        char *const str = argv[optind + i];
        char *endptr;
        errno = 0;
        long const num = strtol(str, &endptr, 10);
        if (errno) {
            perror("strtol");
            return 2;
        }
        if (endptr == str || num < 0 || num > INT_MAX || *endptr) {
            if (fputs("Invalid argument.\n", stderr) == EOF)
                perror("fputs");
            return 2;
        }
        *(int *const[]){ &pidfd, &targetfd, &fd }[i] = (int)num;
    }

    int const gotfd = syscall(SYS_pidfd_getfd, pidfd, targetfd, 0);
    if (gotfd == -1) {
        perror("pidfd_getfd");
        return 2;
    }

    if (gotfd != fd) {
        int ret;
        do {
            ret = dup2(gotfd, fd);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            perror("dup2");
            return 2;
        }
    } else {
        int const flags = fcntl(fd, F_GETFD);
        if (flags == -1) {
            perror("fcntl(F_GETFD)");
            return 2;
        }
        if ((flags & FD_CLOEXEC) &&
            fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == -1) {
            perror("fcntl(F_SETFD)");
            return 2;
        }
    }

    (void)execvp(argv[optind + 3], &argv[optind + 3]);
    perror("execvp");
    return 2;
}
