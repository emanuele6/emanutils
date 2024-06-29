#define _GNU_SOURCE /* grantpt, ptsname, unlockpt */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>

static int
retryeintr_open(char const path[const], int const flags)
{
    int const fd = open(path, flags);
    if (fd != -1 || errno != EINTR)
        return fd;
    return retryeintr_open(path, flags);
}

static int
retryeintr_dup2(int const oldfd, int const newfd)
{
    int const ret = dup2(oldfd, newfd);
    if (ret != -1 || errno != EINTR)
        return ret;
    return retryeintr_dup2(oldfd, newfd);
}

static int
retryeintr_close(int const fd)
{
    int const ret = close(fd);
    if (ret != -1 || errno != EINTR)
        return ret;
    return retryeintr_close(fd);
}

static int
opentofd(int const fd, char const path[const], int const flags)
{
    int thefd = retryeintr_open(path, flags);
    if (thefd == -1) {
        perror("open");
        return -1;
    }

    if (thefd == fd)
       return fd;

    if (retryeintr_dup2(thefd, fd) == -1) {
        perror("dup2");
        return -1;
    }

    if (retryeintr_close(thefd) == -1) {
        perror("close");
        return -1;
    }

    return fd;
}

static int
str2fd(char const str[const])
{
    char *endptr;
    errno = 0;
    long const num = strtol(str, &endptr, 10);
    if (errno) {
        perror("strtol");
        return -1;
    }
    if (endptr == str || num < 0 || num > INT_MAX || *endptr) {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return -1;
    }
    return (int)num;
}

static void
usage(void)
{
    static char const msg[] =
        "Usage: ptytty [-N] ptyfd ttyfd cmd [args...]\n";
    if (fputs(msg, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *const argv[const])
{
    int ptflags = O_RDWR;
    for (int opt; opt = getopt(argc, argv, "+N"), opt != -1;) {
        switch (opt) {
        case 'N':
            ptflags |= O_NOCTTY;
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

    char const *const ptyfdstr = argv[optind];
    char const *const ttyfdstr = argv[optind + 1];
    char *const *const cmd = &argv[optind + 2];

    int const ptyfd = str2fd(ptyfdstr);
    if (ptyfd == -1)
        return 2;
    int const ttyfd = str2fd(ttyfdstr);
    if (ttyfd == -1)
        return 2;

    if (opentofd(ptyfd, "/dev/ptmx", ptflags) == -1)
        return 2;

    if (grantpt(ptyfd) == -1) {
        perror("grantpt");
        return 2;
    }
    if (unlockpt(ptyfd) == -1) {
        perror("unlockpt");
        return 2;
    }

    char const *const ttypath = ptsname(ptyfd);
    if (!ttypath) {
        perror("ptsname");
        return 2;
    }

    if (opentofd(ttyfd, ttypath, O_RDWR) == -1)
        return 2;

    (void)execvp(*cmd, cmd);
    perror("execvp");
    return 2;
}
