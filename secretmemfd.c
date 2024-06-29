#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/syscall.h>
#include <unistd.h>

static void
usage(void)
{
    static char const msg[] = "Usage: secretmemfd fd cmd [args]...\n";
    if (fputs(msg, stderr) == EOF)
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

    if (argc - optind < 2) {
        usage();
        return 2;
    }

    char *const strfd = argv[optind];
    char *endptr;
    errno = 0;
    long const longfd = strtol(strfd, &endptr, 10);
    if (errno) {
        perror("strtol");
        return 2;
    }
    if (endptr == strfd || longfd < 0 || longfd > INT_MAX || *endptr) {
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

    (void)execvp(argv[optind + 1], &argv[optind + 1]);
    perror("execvp");
    return 2;
}
