#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char *environ[];

static char **
getblock(char *args[])
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
        "Usage: chainif { condition... } { chain... } cmd...\n";
    if (fputs(message, stderr) == EOF)
        perror("fputs");
}

int
main(int const argc, char *argv[])
{
    (void)argc;

    char **const condition = &argv[1];
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
        dochain = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    char **toexec;
    if (dochain) {
        toexec = &chain[1];
        for (char **a = &cmd[-1]; a > chain; --a)
            *a = a[-1];
    } else {
        toexec = cmd;
    }

    if (!*toexec)
        return 0;

    (void)execvp(*toexec, toexec);
    perror("execvp");
    return 2;
}
