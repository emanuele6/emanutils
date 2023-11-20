#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/poll.h>
#include <unistd.h>

int
main(int const argc, char *const argv[])
{
    if (argc <= 1) {
        if (EOF == fputs("Usage: pollinfd fd [cmd] [args]...\n",
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
        if (fputs("Invalid argument.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }
    int const fd = (int)longfd;

    int ret;
    struct pollfd pollfd = {
        .fd = fd,
        .events = POLLIN,
    };
    do {
        do {
            ret = poll(&pollfd, 1, -1);
        } while (ret == -1 && errno == EINTR);
        if (ret == -1) {
            perror("poll");
            return 2;
        }
        if (pollfd.revents & POLLHUP)
            return 1;
    } while (!(pollfd.revents & POLLIN));

    if (argc == 2)
        return 0;

    (void)execvp(argv[2], &argv[2]);
    perror("execvp");
    return 2;
}
