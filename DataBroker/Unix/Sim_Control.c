#include "Sim_Control.h"

#include <errno.h>
#include <poll.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "Sem_Stop.h"
#include "init_Server.h"
#include "utils.h"

extern char **environ;

void *Sim_Control(void *_)
{
    char *name = SimName();
    pid_t child = -1;
    int result = 0;
    if (!name) {
        Set_Stop();
        return (void *)-1;
    }
    if (Sem_Stop()) {
        free(name);
        return NULL;
    }

    //TODO: this seems like a bit of a hack. it may be better to add a
    //      configuration option
    if (strcmp(name, "Simulink") != 0) {
        char *args[] = {name, NULL};
        int rc = posix_spawn(&child, name, NULL, NULL, args, environ);
        if (rc != 0) {
            errno = rc;
            perror("Starting simulator");
            free(name);
            Set_Stop();
            return (void *)-1;
        }
    } else {
        printf("External simulator selected. You may now start the simulator.\n");
    }
    free(name);
    bool terminal = true;
    printf("***Enter X to stop simulation***\n");
    while (!Sem_Stop()) {
        if (child > 0) {
            int status;
            pid_t rc = waitpid(child, &status, WNOHANG);
            if (rc == child || (rc == -1 && errno != EINTR)) {
                if (rc == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                    result = -1;
                child = -1;
                Set_Stop();
                break;
            }
        }
        if (!terminal) {
            sleep_ms(100);
            continue;
        }
        struct pollfd input = {STDIN_FILENO, POLLIN, 0};
        int rc = poll(&input, 1, 100);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            result = -1;
            Set_Stop();
            break;
        }
        if (rc > 0 && (input.revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))) {
            char buffer[128];
            ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (n == 0 || (n < 0 && errno != EINTR && errno != EAGAIN)) {
                terminal = false;
                continue;
            }
            for (ssize_t i = 0; i < n; ++i) {
                if (buffer[i] == 'x' || buffer[i] == 'X') {
                    Set_Stop();
                }
            }
        }
    }
    if (child > 0) {
        int status;
        pid_t rc = 0;
        for (int i = 0; i < 100; ++i) {
            rc = waitpid(child, &status, WNOHANG);
            if (rc == child || (rc < 0 && errno != EINTR)) {
                break;
            }
            sleep_ms(10);
        }
        if (rc == 0 || (rc < 0 && errno == EINTR)) {
            kill(child, SIGTERM);
            for (int i = 0; i < 100; ++i) {
                rc = waitpid(child, &status, WNOHANG);
                if (rc == child || (rc < 0 && errno != EINTR)) {
                    break;
                }
                sleep_ms(10);
            }
        }
        if (rc == 0 || (rc < 0 && errno == EINTR)) {
            kill(child, SIGKILL);
            RETRY_EINTR(rc, waitpid(child, &status, 0));
        }
        if (rc == -1) {
            result = -1;
        }
    }
    printf("User control exiting.\n");
    return result ? (void *)-1 : NULL;
}
