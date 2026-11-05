#include "Sem_Interface.h"

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

static int lock_fd = -1;

int Lock_Interface(void)
{
    char path[128];

    //FIXME: acquire temporary directory from $TMPDIR, per POSIX. fall back to
    //       P_tmpdir and /tmp otherwise
    snprintf(path, sizeof(path), "/tmp/arcade-databroker-%lu.lock",
             (unsigned long)getuid());
    lock_fd = open(path, O_CREAT | O_RDWR | O_CLOEXEC , 0600);
    if (lock_fd < 0) {
        return -1;
    }
    struct stat info;
    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET};
    if (fstat(lock_fd, &info) || !S_ISREG(info.st_mode) ||
        info.st_uid != getuid() || fcntl(lock_fd, F_SETLK, &lock) < 0) {
        int saved = errno ? errno : EINVAL;
        close(lock_fd);
        lock_fd = -1;
        errno = saved;
        return -1;
    }
    return 0;
}

void Unlock_Interface(void)
{
    if (lock_fd >= 0) {
        close(lock_fd);
        lock_fd = -1;
    }
}

static const char *names[] = {
    "/pp_sem", "/up_sem", "/stop", "/msg",
    "/co_sim", "/co_sim_2", "/SemaphoreWrite", "/SemaphoreDone"};

int Sem_Interface(void)
{
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        sem_t *sem = sem_open(names[i], O_CREAT, 0644, 0);
        if (sem == SEM_FAILED) {
            return -1;
        }

        int rc;
        do {
            rc = sem_trywait(sem);
        } while (rc == 0 || errno == EINTR);
        int saved = errno;

        sem_close(sem);
        if (saved != EAGAIN) {
            errno = saved;
            return -1;
        }
    }
    return 0;
}

void Cleanup_Interface(void)
{
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        sem_unlink(names[i]);
    }

    const key_t keys[] = {10618, 10619, 10620, 10621};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        int id = shmget(keys[i], 1, 0600);
        if (id >= 0) {
            shmctl(id, IPC_RMID, NULL);
        }
    }
}
