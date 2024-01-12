#define _GNU_SOURCE /* F_ADD_SEALS, F_GET_SEALS, F_SEAL_* */
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

struct sealinfo {
    int flag; 
    char *string;
} const infos[] = {
#define SEAL(s) { F_SEAL_ ## s, "F_SEAL_" #s, }
    SEAL(SEAL),
    SEAL(SHRINK),
    SEAL(GROW),
    SEAL(WRITE),
    SEAL(FUTURE_WRITE),
#undef SEAL
    { 0 },
};

static void
usage()
{
    static char const message[] =
        "Usage: fdseal add [-s SEAL]... FD [CMD]...\n"
        "Usage: fdseal check [-xn] [-s SEAL]... FD [CMD]...\n"
        "Usage: fdseal get FD\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

static bool
fd_from_string(char const string[], int *v)
{
    char *endptr;
    errno = 0;
    long const longfd = strtol(string, &endptr, 10);
    if (longfd == LONG_MAX && errno != 0) {
        perror("strtol");
        return false;
    }
    if (longfd < 0 || longfd > INT_MAX || *endptr != '\0') {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return false;
    }
    *v = (int)longfd;
    return true;
}

static bool
seal_from_string(char const string[], int *v)
{
    for (struct sealinfo const *si = infos; si->flag; ++si) {
        if (strcmp(string, si->string) == 0) {
            *v = si->flag;
            return true;
        }
    }
    return false;
}

static int
do_add(int const argc, char *argv[])
{
    int seals = 0;
    for (int opt; opt = getopt(argc, argv, "+s:"), opt != -1;) {
        switch (opt) {
        case 's': {
            int s;
            if (!seal_from_string(optarg, &s)) {
                if (fputs("Invalid seal.\n", stderr) == EOF)
                    perror("fputs");
                return 2;
            }
            seals |= s;
            break;
        }
        default:
            return 2;
        }
    }

    if (argc - optind < 1) {
        usage();
        return 2;
    }
    char const *const argfd = argv[optind];
    char **const command = &argv[optind + 1];

    int fd;
    if (!fd_from_string(argfd, &fd))
        return 2;

    if (fcntl(fd, F_ADD_SEALS, seals) != 0) {
        perror("fcntl(F_ADD_SEALS)");
        return 1;
    }

    if (!*command)
        return 0;

    (void)execvp(*command, command);
    perror("execvp");
    return 0;
}

static int
do_check(int const argc, char *argv[])
{
    bool exactflag = false;
    bool negateflag = false;
    int seals = 0;
    for (int opt; opt = getopt(argc, argv, "+ns:x"), opt != -1;) {
        switch (opt) {
        case 'n':
            negateflag = true;
            break;
        case 's': {
            int s;
            if (!seal_from_string(optarg, &s)) {
                if (fputs("Invalid seal.\n", stderr) == EOF)
                    perror("fputs");
                return 2;
            }
            seals |= s;
            break;
        }
        case 'x':
            exactflag = true;
            break;
        default:
            return 2;
        }
    }

    if (argc - optind < 1) {
        usage();
        return 2;
    }
    char const *const argfd = argv[optind];
    char **const command = &argv[optind + 1];

    int fd;
    if (!fd_from_string(argfd, &fd))
        return 2;

    int fdseals = fcntl(fd, F_GET_SEALS);
    if (seals == -1) {
        perror("fcntl(F_GET_SEALS)");
        return 2;
    }
    if (!exactflag)
        fdseals &= seals;

    if ((fdseals == seals) == negateflag)
        return 1;

    if (!*command)
        return 0;

    (void)execvp(*command, command);
    perror("execvp");
    return 0;
}

static int
do_get(int const argc, char *argv[])
{
    for (int opt; opt = getopt(argc, argv, "+"), opt != -1;) {
        switch (opt) {
        default:
            return 2;
        }
    }

    if (optind + 1 != argc) {
        usage();
        return 2;
    }

    int fd;
    if (!fd_from_string(argv[optind], &fd))
        return 2;

    int seals = fcntl(fd, F_GET_SEALS);
    if (seals == -1) {
        perror("fcntl(F_GET_SEALS)");
        return 1;
    }

    bool first = true;
    for (struct sealinfo const *si = infos; seals && si->flag; ++si) {
        if (seals & si->flag) {
            seals &= ~si->flag;
            if (!first && putchar('|') == EOF) {
                perror("putchar");
                return 2;
            }
            if (fputs(si->string, stdout) == EOF) {
                perror("fputs");
                return 2;
            }
            first = false;
        }
    }

    if (seals) {
        if (!first && putchar('|') == EOF) {
            perror("putchar");
            return 2;
        }
        if (printf("??? (%d)", seals) < 0) {
            perror("printf");
            return 2;
        }
    }

    if (putchar('\n') == EOF) {
        perror("putchar");
        return 2;
    }

    return 0;
}

int
main(int const argc, char *argv[])
{
    if (argc > 1) {
        if (strcmp(argv[1], "add") == 0)
            return do_add(argc - 1, &argv[1]);
        if (strcmp(argv[1], "check") == 0)
            return do_check(argc - 1, &argv[1]);
        if (strcmp(argv[1], "get") == 0)
            return do_get(argc - 1, &argv[1]);
    }

    usage();
    return 2;
}
