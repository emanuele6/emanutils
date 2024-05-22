#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <linux/kcmp.h>
#include <sys/syscall.h>
#include <unistd.h>

static bool
str2posint(int *const intp, char const str[const],
           char const error[const])
{
    char *endptr;
    errno = 0;
    long const longnum = strtol(str, &endptr, 10);
    if (errno) {
        perror("strtol");
        return false;
    }
    if (endptr == str || longnum < 0 || longnum > INT_MAX || *endptr) {
        if (fputs(error, stderr) == EOF)
            perror("fputs");
        return false;
    }
    *intp = (int)longnum;
    return true;
}

static void
usage(void)
{
    static char const message[] =
        "Usage: fdcmp [-0123en] [-p PID1] [-P PID2] fd1 fd2 [cmd]...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *argv[const])
{
    char const *pid1_str = NULL;
    char const *pid2_str = NULL;
    bool zeroflag = false;
    bool oneflag = false;
    bool twoflag = false;
    bool threeflag = false;
    bool envflag = false;
    bool negateflag = false;

    for (int opt; opt = getopt(argc, argv, "+0123enp:P:"), opt != -1;) {
        switch (opt) {
        case '0':
            zeroflag = true;
            break;
        case '1':
            oneflag = true;
            break;
        case '2':
            twoflag = true;
            break;
        case '3':
            threeflag = true;
            break;
        case 'e':
            envflag = true;
            break;
        case 'n':
            negateflag = true;
            break;
        case 'p':
            pid1_str = optarg;
            break;
        case 'P':
            pid2_str = optarg;
            break;
        default:
            usage();
            return 2;
        }
    }

    if (!(oneflag || twoflag || threeflag))
        zeroflag = true;

    if (optind + 2 > argc) {
        usage();
        return 2;
    }

    int fd1;
    int fd2;
    if (!str2posint(&fd1, argv[optind], "Invalid fd1.\n") ||
        !str2posint(&fd2, argv[optind + 1], "Invalid fd2.\n")) {
        return 2;
    }

    char *const *const cmd = &argv[optind + 2];

    pid_t pid1;
    if (pid1_str) {
        int intpid;
        if (!str2posint(&intpid, pid1_str, "Invalid pid1.\n"))
            return 2;
        pid1 = (pid_t)intpid;
    } else {
        pid1 = getpid();
    }
    pid_t pid2;
    if (pid2_str) {
        int intpid;
        if (!str2posint(&intpid, pid2_str, "Invalid pid2.\n"))
            return 2;
        pid2 = (pid_t)intpid;
    } else {
        pid2 = pid1;
    }

    int const res = syscall(SYS_kcmp, pid1, pid2, KCMP_FILE, fd1, fd2);
    char const *res_str;
    switch (res) {
    case -1:
        perror("kcmp");
        return 2;
    case 0:
        if (zeroflag == negateflag)
            return 1;
        res_str = "0";
        break;
    case 1:
        if (oneflag == negateflag)
            return 1;
        res_str = "1";
        break;
    case 2:
        if (twoflag == negateflag)
            return 1;
        res_str = "2";
        break;
    default: /* 3 */
        if (threeflag == negateflag)
            return 1;
        res_str = "3";
    }

    if (*cmd == NULL)
        return 0;

    if (envflag && setenv("FDCMP_KCMP", res_str, 1) == -1) {
        perror("setenv");
        return 2;
    }

    (void)execvp(*cmd, cmd);
    perror("execvp");
    return 2;
}
