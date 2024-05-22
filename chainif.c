#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static char **
getblock(char *args[const])
{
    if (!*args)
        return NULL;
    if (!**args) {
        *args = NULL;
        return &args[1];
    }
    if (**args != ' ')
        return NULL;
    ++*args;
    return getblock(&args[1]);
}

static void
usage()
{
    static char const message[] =
        "Usage: chainif [-AEn] { condition... } { chain... } cmd...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *argv[const])
{
    bool appendflag = false;
    bool envflag = false;
    bool negateflag = false;

    for (int opt; (opt = getopt(argc, argv, "+AEn")) != -1;) {
        switch (opt) {
        case 'A':
            appendflag = true;
            break;
        case 'E':
            envflag = true;
            break;
        case 'n':
            negateflag = true;
            break;
        default:
            usage();
            return 2;
        }
    }

    char **const condition = &argv[optind];
    char **const chain = getblock(condition);
    if (!chain) {
        usage();
        return 2;
    }
    char **const cmd = getblock(chain);
    if (!cmd) {
        usage();
        return 2;
    }

    bool dochain = *condition == NULL;
    unsigned char exitstatus = 0;
    if (!dochain) {
        pid_t pid;
        int const ret = posix_spawnp(&pid, *condition, NULL, NULL,
                                     condition, environ);
        if (ret) {
            perror("posix_spawnp");
            return 2;
        }

        int status;
        for (;;) {
            int const ret = waitpid(pid, &status, 0);
            if (ret != -1)
                break;
            if (errno != EINTR) {
                perror("waitpid");
                return 2;
            }
        }
        exitstatus = WIFEXITED(status)
            ? WEXITSTATUS(status)
            : 128 + WTERMSIG(status);
        dochain = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    char *const *const toexec = dochain != negateflag
        ? memmove(&chain[1], chain, (cmd - &chain[1]) * sizeof *chain)
        : (appendflag ? chain : cmd);

    if (!*toexec)
        return 0;

    if (envflag) {
        char buf[3 + 1];
        int const ret = snprintf(buf, sizeof buf, "%hhu", exitstatus);
        if (ret < 0) {
            perror("snprintf");
            return 2;
        }
        if ((size_t)ret >= sizeof buf) {
            static char const efmt[] =
                "snprintf: the buffer is %zd byte%s too small.\n";
            ptrdiff_t const diff = ret - sizeof buf;
            if (fprintf(stderr, efmt, diff, &"s"[diff == 1]) == EOF)
                perror("fprintf");
            return 2;
        }
        if (setenv("?", buf, 1)) {
            perror("setenv");
            return 2;
        }
    }

    (void)execvp(*toexec, toexec);
    perror("execvp");
    return 2;
}
