#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/syscall.h>
#include <unistd.h>

int
main(int const argc, char *const argv[])
{
    if (argc <= 2) {
        if (EOF == fputs("Usage: secretmemfd fd cmd [args]...\n",
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

    int const memfd = syscall(SYS_memfd_secret, 0);
    if (memfd == -1) {
        perror("memfd_secret");
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

    (void)execvp(argv[2], &argv[2]);
    perror("execvp");
    return 2;
}
