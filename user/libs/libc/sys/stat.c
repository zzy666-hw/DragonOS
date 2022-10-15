#include "stat.h"
#include <libsystem/syscall.h>

int mkdir(const char *path, mode_t mode)
{
    return syscall_invoke(SYS_MKDIR, (uint64_t)path, (uint64_t)mode, 0, 0, 0, 0, 0, 0);
}

/**
 * @brief 获取系统的内存信息
 *
 * @param stat 传入的内存信息结构体
 * @return int 错误码
 */
int mstat(struct mstat_t *stat)
{
    return syscall_invoke(SYS_MSTAT, (uint64_t)stat, 0, 0, 0, 0, 0, 0, 0);
}

int pipe(int *fd)
{
    return syscall_invoke(SYS_PIPE, (uint64_t)fd, 0, 0,0,0,0,0,0);
}

uint64_t shmget()
{
    return syscall_invoke(SYS_SHMGET, (uint64_t)fd, 0, 0,0,0,0,0,0);
}

uint64_t shmat()
{
    return syscall_invoke(SYS_SHMAT, (uint64_t)fd, 0, 0,0,0,0,0,0);
}

uint64_t shmdt()
{
    return syscall_invoke(SYS_SHMDT, (uint64_t)fd, 0, 0,0,0,0,0,0);
}

uint64_t shmctl()
{
    return syscall_invoke(SYS_SHMCTL, (uint64_t)fd, 0, 0,0,0,0,0,0);
}