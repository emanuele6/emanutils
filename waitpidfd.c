#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <sys/poll.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int const argc, char *const argv[])
{
    pid_t const pid = fork();
    switch (pid) {
    case -1:
        perror("fork");
        return 2;
    case 0:
        /* child */
        sleep(5);
        return 0;
    }

    char buf[4096];
    snprintf(buf, 4096, "/proc/%d", (int)pid);
    int const pidfd = open(buf, O_RDONLY);

    int ret;
    siginfo_t info;
    do {
        ret = waitid(P_PID, pidfd, &info, 0);
    } while (ret == -1 && errno == EINTR);
    if (ret == -1) {
        perror("waitid");
        return 2;
    }

    do {
        ret = close(pidfd);
    } while (ret == -1 && errno == EINTR);
    if (ret == -1) {
        perror("close");
        return 2;
    }
}
