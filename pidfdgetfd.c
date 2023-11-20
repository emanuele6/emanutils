#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

int
main(int const argc, char *const argv[])
{
    if (argc <= 4) {
        if (EOF == fputs("Usage: "
                         "pidfdgetfd pidfd targetfd fd cmd [args]...\n",
                         stderr)) {
            perror("fputs");
        }
        return 2;
    }

    int pidfd, targetfd, fd;
    for (int i = 0; i < 3; ++i) {
        char *endptr;
        errno = 0;
        long const longnum = strtol(argv[i + 1], &endptr, 10);
        if (errno) {
            perror("strtol");
            return 2;
        }
        if (longnum < 0 || longnum > INT_MAX || *endptr != '\0') {
            if (fputs("Invalid argument.\n", stderr) == EOF)
                perror("fputs");
            return 2;
        }
        *(int *const []){ &pidfd, &targetfd, &fd }[i] = (int)longnum;
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

    (void)execvp(argv[4], &argv[4]);
    perror("execvp");
    return 2;
}
