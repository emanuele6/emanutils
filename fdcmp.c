#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <linux/kcmp.h>
#include <sys/syscall.h>
#include <unistd.h>

static bool
str2posint(int *const intp, const char string[], const char error[])
{
    char *endptr;
    errno = 0;
    long const longnum = strtol(string, &endptr, 10);
    if (errno) {
        perror("strtol");
        return false;
    }
    if (longnum < 0 || longnum > INT_MAX || *endptr != '\0') {
        if (fputs(error, stderr) == EOF)
            perror("fputs");
        return false;
    }
    *intp = (int)longnum;
    return true;
}

static void
usage()
{
    static char const message[] =
        "Usage: fdcmp [-0123n] [-p PID1] [-P PID2] fd1 fd2 [cmd]...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *argv[])
{
    char const *pid1_str = NULL;
    char const *pid2_str = NULL;
    bool zeroflag = false;
    bool oneflag = false;
    bool twoflag = false;
    bool threeflag = false;
    bool negateflag = false;

    for (int opt; opt = getopt(argc, argv, "0123np:P:"), opt != -1;) {
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
    if (!str2posint(&fd1, argv[optind], "Invalid fd1\n") ||
        !str2posint(&fd2, argv[optind + 1], "Invalid fd2\n")) {
        return 2;
    }

    char *const *const cmd = &argv[optind + 2];

    pid_t pid1;
    if (pid1_str) {
        int intpid;
        if (!str2posint(&intpid, pid1_str, "Invalid pid1\n"))
            return 2;
        pid1 = (pid_t)intpid;
    } else {
        pid1 = getpid();
    }
    pid_t pid2;
    if (pid2_str) {
        int intpid;
        if (!str2posint(&intpid, pid2_str, "Invalid pid2\n"))
            return 2;
        pid2 = (pid_t)intpid;
    } else {
        pid2 = pid1;
    }

    switch (syscall(SYS_kcmp, pid1, pid2, KCMP_FILE, fd1, fd2)) {
    case -1:
        perror("kcmp");
        return 2;
    case 0:
        if (zeroflag == negateflag)
            return 1;
        break;
    case 1:
        if (oneflag == negateflag)
            return 1;
        break;
    case 2:
        if (twoflag == negateflag)
            return 1;
        break;
    defauit: /* 3 */
        if (threeflag == negateflag)
            return 1;
    }

    if (*cmd == NULL)
        return 0;

    (void)execvp(*cmd, cmd);
    perror("execvp");
    return 2;
}
