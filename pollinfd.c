#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <poll.h>
#include <unistd.h>

static void
usage()
{
    static char const message[] =
        "Usage: pollinfd [-t timeout] fd [cmd] [args]...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

static bool
str2num(char const str[const], int const min, int const max,
        int *const out, char const err[const])
{
    char *endptr;
    errno = 0;
    long const longnum = strtol(str, &endptr, 10);
    if (errno) {
        perror("strtol");
        return false;
    }
    if (endptr == str || longnum < min || longnum > max || *endptr) {
        if (fputs(err, stderr) == EOF)
            perror("fputs");
        return false;
    }
    *out = (int)longnum;
    return true;
}

int
main(int const argc, char *argv[const])
{
    int timeout = -1;

    for (int opt; opt = getopt(argc, argv, "+t:"), opt != -1;) {
        switch (opt) {
        case 't': {
            static const char etimeout[] = "Invalid timeout.\n";
            if (!str2num(optarg, INT_MIN, INT_MAX, &timeout, etimeout))
                return 2;
            break;
        }
        default:
            usage();
            return 2;
        }
    }

    if (argc <= optind) {
        usage();
        return 2;
    }

    char const *const argfd = argv[optind];
    char **const command = &argv[optind + 1];

    struct pollfd pollfd = {
        .events = POLLIN,
    };

    if (!str2num(argfd, 0, INT_MAX, &pollfd.fd, "Invalid fd.\n"))
        return 2;

    do {
        switch (poll(&pollfd, 1, timeout)) {
        case 0:
            return 1;
        case -1:
            if (errno == EINTR || errno == EAGAIN)
                continue;
            perror("poll");
            return 2;
        }
    } while (0);

    if (!(pollfd.revents & POLLIN)) {
        if (pollfd.revents & POLLNVAL) {
            static char const epollnval[] =
                "File descriptor cannot be used with poll.\n";
            if (fputs(epollnval, stderr) == EOF)
                perror("fputs");
            return 2;
        }
        return 1;
    }

    if (!*command)
        return 0;

    (void)execvp(*command, command);
    perror("execvp");
    return 2;
}
