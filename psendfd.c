#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>

#define SPECIALSOURCES \
    SPECIALSOURCE(close) \

#define SPECIALTARGETS \
    SPECIALTARGET(any) \
    SPECIALTARGET(cwd) \

enum {
    SPECIALSOURCES_zero,
#define SPECIALSOURCE(x) SPECIALSOURCE_##x,
    SPECIALSOURCES
#undef SPECIALSOURCE
    SPECIALSOURCES_end,
};
enum {
    SPECIALTARGETS_zero,
#define SPECIALTARGET(x) SPECIALTARGET_##x,
    SPECIALTARGETS
#undef SPECIALTARGET
    SPECIALTARGETS_end,
};

static void
usage(void)
{
    static char const msg[] =
        "Usage: psendfd [-ef] [-m mintargetfd] [-P sourcepid] pid fd "
        "targetfd [cmd]...\n";
    if (fputs(msg, stderr) == EOF)
        perror("fputs");
}

static int
str2int(char const *const str)
{
    char *endptr;
    errno = 0;
    long const num = strtol(str, &endptr, 10);
    if (errno) {
        perror("strtol");
        return INT_MIN;
    }
    if (endptr == str || num < INT_MIN || num > INT_MAX || *endptr)
        return INT_MIN;
    return (int)num;
}

static void
tracee_perror(char const *const msg, int const err)
{
    if (fprintf(stderr, "tracee: %s: %s\n", msg, strerror(err)) == EOF)
        perror("fprintf");
}

static bool
nextstop(pid_t const pid, int *const status)
{
    while (waitpid(pid, status, 0) == -1) {
        if (errno != EINTR) {
            perror("waitpid");
            return false;
        }
    }
    if (!WIFSTOPPED(*status)) {
        if (fputs("Unexpected wait status.\n", stderr) == EOF)
            perror("fputs");
        return false;
    }
    return true;
}

static bool
do_syscall(pid_t const pid, struct user_regs_struct *const regs)
{
    if (ptrace(PTRACE_SETREGS, pid, 0, regs) == -1) {
        perror("ptrace(PTRACE_SETREGS)");
        return false;
    }
    do {
        int status;
        do {
            if (ptrace(PTRACE_SYSCALL, pid, 0, 0) == -1) {
                perror("ptrace(PTRACE_SYSCALL)");
                return false;
            }
            if (!nextstop(pid, &status))
                return false;
        } while (WSTOPSIG(status) != SIGTRAP);

        errno = 0;
        regs->rax = ptrace(PTRACE_PEEKUSER, pid, 8 * RAX, 0);
        if (errno) {
            perror("ptrace(PTRACE_PEEKUSER)");
            return false;
        }
    } while ((long)regs->rax == -ENOSYS);
    return true;
}

static int
do_close(pid_t const pid, int const fd, bool const fflag,
         struct user_regs_struct const *const savedregs)
{
    struct user_regs_struct regs;

    do {
        regs = *savedregs;
        regs.rax = SYS_close;
        regs.rdi = fd;
        if (!do_syscall(pid, &regs))
            return 2;
    } while ((long)regs.rax == -EINTR);

    if (regs.rax == 0 || (fflag && (long)regs.rax == -EBADF))
        return 0;

    tracee_perror("close", -regs.rax);
    return 2;
}

static int
do_send(pid_t const pid, int const fd, int *const targetfdp,
        pid_t const sourcepid,
        struct user_regs_struct const *const savedregs, int const fdmin)
{
    int ret = 0;
    int const targetfd = *targetfdp;
    struct user_regs_struct regs = *savedregs;
    regs.rax = SYS_pidfd_open;
    regs.rdi = sourcepid >= 0 ? sourcepid : getpid();
    regs.rsi = 0;
    if (!do_syscall(pid, &regs))
        return 2;
    if ((long)regs.rax < 0) {
        tracee_perror("pidfd_open", -regs.rax);
        return 2;
    }

    int const pidfd = regs.rax;
    regs = *savedregs;
    regs.rax = SYS_pidfd_getfd;
    regs.rdi = pidfd;
    regs.rsi = fd;
    regs.rdx = 0;
    if (!do_syscall(pid, &regs))
        return 2;
    if ((long)regs.rax < 0) {
        tracee_perror("pidfd_getfd", -regs.rax);
        return 2;
    }
    int thefd = regs.rax;

    if (targetfd < 0) {
        if (fdmin > thefd) {
            int const theoldfd = thefd;
            regs = *savedregs;
            regs.rax = SYS_fcntl;
            regs.rdi = thefd;
            regs.rsi = F_DUPFD;
            regs.rdx = fdmin;
            if (!do_syscall(pid, &regs))
                return 2;
            if ((long)regs.rax < 0) {
                ret = 2;
                tracee_perror("fcntl(F_DUPFD)", -regs.rax);
            } else {
                thefd = regs.rax;
            }
            if (do_close(pid, theoldfd, false, savedregs))
                ret = 2;
        }
    } else if (thefd != targetfd) {
        do {
            regs = *savedregs;
            regs.rax = SYS_dup2;
            regs.rdi = thefd;
            regs.rsi = targetfd;
            if (!do_syscall(pid, &regs))
                return 2;
        } while ((long)regs.rax == -EINTR);

        if ((long)regs.rax < 0) {
            tracee_perror("dup2", -regs.rax);
            ret = 2;
        }

        if (do_close(pid, thefd, false, savedregs))
            ret = 2;
    }

    if (ret || pidfd != targetfd) {
        if (do_close(pid, pidfd, false, savedregs))
            ret = 2;
    }

    if (!ret)
        *targetfdp = thefd;

    return ret;
}

static int
do_fchdir(pid_t const pid, int const fd,
          struct user_regs_struct const *const savedregs)
{
    int ret = 0;
    int targetfd = -SPECIALTARGET_any;
    if (do_send(pid, fd, &targetfd, -1, savedregs, -1))
        return 2;

    struct user_regs_struct regs;
    do {
        regs = *savedregs;
        regs.rax = SYS_fchdir;
        regs.rdi = targetfd;
        if (!do_syscall(pid, &regs))
            return 2;
    } while ((long)regs.rax == -EINTR);
    if ((long)regs.rax < 0) {
        tracee_perror("fchdir", -regs.rax);
        ret = 2;
    }

    if (do_close(pid, targetfd, false, savedregs))
        ret = 2;
    return ret;
}

int
main(int const argc, char *const *const argv)
{
    pid_t sourcepid = -1;
    bool eflag = false;
    bool fflag = false;
    int fdmin = -1;
    for (int opt; opt = getopt(argc, argv, "+efm:P:"), opt != -1;) {
        switch (opt) {
        case 'e':
            eflag = true;
            break;
        case 'f':
            fflag = true;
            break;
        case 'm':
            fdmin = str2int(optarg);
            if (fdmin <= -2) {
                static char const emsg[] =
                    "Invalid targetfd lower limit.\n";
                if (fputs(emsg, stderr) == EOF)
                    perror("fputs");
                return 2;
            }
            break;
        case 'P':
            sourcepid = str2int(optarg);
            if (sourcepid <= -2) {
                if (fputs("Invalid source pid.\n", stderr) == EOF)
                    perror("fputs");
                return 2;
            }
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

    int const intpid = str2int(argv[optind]);
    if (intpid < 0) {
        if (fputs("Invalid pid.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }
    pid_t const pid = (pid_t)intpid;

    char const *const fdstr = argv[optind + 1];
    int const fd =
#define SPECIALSOURCE(x) !strcmp(fdstr, #x) ? -SPECIALSOURCE_##x :
        SPECIALSOURCES
#undef SPECIALSOURCE
        str2int(fdstr);
    if (fd <= -SPECIALSOURCES_end) {
        if (fputs("Invalid fd.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }

    char const *const tfdstr = argv[optind + 2];
    int targetfd =
#define SPECIALTARGET(x) !strcmp(tfdstr, #x) ? -SPECIALTARGET_##x :
        SPECIALTARGETS
#undef SPECIALTARGET
        str2int(tfdstr);
    if (targetfd <= -SPECIALTARGETS_end || (fd < 0 && targetfd < 0)) {
        if (fputs("Invalid targetfd.\n", stderr) == EOF)
            perror("fputs");
        return 2;
    }

    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
        perror("ptrace(PTRACE_ATTACH)");
        return 2;
    }

    for (int status; !nextstop(pid, &status);)
        return 2;

    struct user_regs_struct savedregs;
    if (ptrace(PTRACE_GETREGS, pid, 0, &savedregs) == -1) {
        perror("ptrace(PTRACE_GETREGS)");
        return 2;
    }
    long const word = ptrace(PTRACE_PEEKTEXT, pid, savedregs.rip, 0);
    if (word == -1) {
        perror("ptrace(PTRACE_PEEKTEXT)");
        return 2;
    }
    long const pokeret = ptrace(PTRACE_POKETEXT, pid, savedregs.rip,
                                /* syscall */ 0x050f);
    if (pokeret == -1) {
        perror("ptrace(PTRACE_POKETEXT)");
        return 2;
    }

    int const ret =
        fd == -SPECIALSOURCE_close ?
            do_close(pid, targetfd, fflag, &savedregs) :
        targetfd == -SPECIALTARGET_cwd ?
            do_fchdir(pid, fd, &savedregs) :
        do_send(pid, fd, &targetfd, sourcepid, &savedregs, fdmin);

    if (ptrace(PTRACE_POKETEXT, pid, savedregs.rip, word) == -1) {
        perror("ptrace(PTRACE_POKETEXT)");
        return 2;
    }
    if (ptrace(PTRACE_SETREGS, pid, 0, &savedregs) == -1) {
        perror("ptrace(PTRACE_SETREGS)");
        return 2;
    }

    if (ret || argc - 3 <= optind)
        return ret;

    if (eflag && fd >= 0 && targetfd != -SPECIALTARGET_cwd) {
        char buf[10 + 1];
        int const sz = snprintf(buf, sizeof buf, "%d", targetfd);
        if (sz < 0) {
            perror("snprintf");
            return 2;
        }
        if ((size_t)sz >= sizeof buf) {
            static char const efmt[] =
                "snprintf: the buffer is %zd byte%s too small.\n";
            ptrdiff_t const diff = ret - sizeof buf;
            if (fprintf(stderr, efmt, diff, &"s"[diff == 1]) == EOF)
                perror("fprintf");
            return 2;
        }
        if (setenv("PSENDFD_FD", buf, 1) == -1) {
            perror("setenv");
            return 2;
        }
    }

    if (ptrace(PTRACE_DETACH, pid, 0, 0) == -1) {
        perror("ptrace(PTRACE_DETACH)");
        return 2;
    }

    (void)execvp(argv[optind + 3], &argv[optind + 3]);
    perror("execvp");
    return 2;
}
