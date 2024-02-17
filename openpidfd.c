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
    if (argc <= 3) {
        if (EOF == fputs("Usage: openpidfd fd pid cmd [args]...\n",
                         stderr)) {
            perror("fputs");
        }
        return 2;
    }

    int fd, pid;
    for (int i = 0; i < 2; ++i) {
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
        *(int *const[]){ &fd, &pid }[i] = (int)longnum;
    }

    int const pidfd = syscall(SYS_pidfd_open, (pid_t)pid, 0);
    if (pidfd == -1) {
        perror("pidfd_open");
        return 2;
    }

    if (pidfd != fd) {
        int ret;
        do {
            ret = dup2(pidfd, fd);
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

    (void)execvp(argv[3], &argv[3]);
    perror("execvp");
    return 2;
}
