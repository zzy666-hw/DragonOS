#include "pipe.h"
#include <common/spinlock.h>
#include <process/process.h>
#include <process/ptrace.h>
#include <filesystem/VFS/VFS.h>
#include <filesystem/fat32/fat32.h>
#include <common/atomic.h>
#include <mm/slab.h>

#define MAX_PIPE_BUFF_SIZE  512

struct pipe_t {
    volatile unsigned int valid_cnt;
    unsigned int read_pos;
    unsigned int write_pos;
    wait_queue_node_t read_wait_queue;
    wait_queue_node_t write_wait_queue;
    char buf[MAX_PIPE_BUFF_SIZE];
    spinlock_t lock;
};

long pipe_read(struct vfs_file_t *file_ptr, char *buf,
               int64_t count, long *position)
{
    int i = 0;
    struct pipe_t *pipe_ptr = NULL;

    kdebug("pipe_read into!\n");
    pipe_ptr = (struct pipe_t *)file_ptr->private_data;
    spin_lock(&pipe_ptr->lock);
    while (pipe_ptr->valid_cnt == 0) {
        /* pipe 空 */
        kdebug("pipe_read empty!\n");
        wait_queue_wakeup(&pipe_ptr->write_wait_queue, PROC_UNINTERRUPTIBLE);
        wait_queue_sleep_on_unlock(&pipe_ptr->read_wait_queue, (void *)&pipe_ptr->lock);
        spin_lock(&pipe_ptr->lock);
    }
    for (i = 0; i < pipe_ptr->valid_cnt; i++) {
        if (i == count) {
            break;
        }
        copy_to_user(buf + i, &pipe_ptr->buf[pipe_ptr->read_pos], sizeof(char));
        pipe_ptr->read_pos = (pipe_ptr->read_pos + 1) % MAX_PIPE_BUFF_SIZE;
    }
    pipe_ptr->valid_cnt = pipe_ptr->valid_cnt - i;
    spin_unlock(&pipe_ptr->lock);
    wait_queue_wakeup(&pipe_ptr->write_wait_queue, PROC_UNINTERRUPTIBLE);
    kdebug("pipe_read end!\n");

    return i;
}
long pipe_write(struct vfs_file_t *file_ptr, char *buf,
                int64_t count, long *position)
{
    int i = 0;
    struct pipe_t *pipe_ptr = NULL;

    kdebug("pipe_write into!\n");
    pipe_ptr = (struct pipe_t *)file_ptr->private_data;
    spin_lock(&pipe_ptr->lock);
    while (pipe_ptr->valid_cnt + count >= MAX_PIPE_BUFF_SIZE) {
        /* pipe 满 */
        kdebug("pipe_write pipe full!\n");
        wait_queue_wakeup(&pipe_ptr->read_wait_queue, PROC_UNINTERRUPTIBLE);
        wait_queue_sleep_on_unlock(&pipe_ptr->write_wait_queue, (void *)&pipe_ptr->lock);
        spin_lock(&pipe_ptr->lock);
    }
    for (i = pipe_ptr->valid_cnt; i < MAX_PIPE_BUFF_SIZE; i++) {
        if (i - pipe_ptr->valid_cnt == count) {
            break;
        }
        copy_from_user(&pipe_ptr->buf[pipe_ptr->write_pos], buf + i, sizeof(char));
        pipe_ptr->write_pos = (pipe_ptr->write_pos + 1) % MAX_PIPE_BUFF_SIZE;
    }
    pipe_ptr->valid_cnt += count;
    spin_unlock(&pipe_ptr->lock);
    wait_queue_wakeup(&pipe_ptr->read_wait_queue, PROC_UNINTERRUPTIBLE);
    kdebug("pipe_write out!\n");

    return count;
}

long pipe_close(struct vfs_index_node_t *inode, struct vfs_file_t *file_ptr)
{
    return 0;
}

struct vfs_file_operations_t g_pipe_file_ops = {
    .open       = NULL,
    .close      = pipe_close,
    .read       = pipe_read,
    .write      = pipe_write,
    .lseek      = NULL,
    .ioctl      = NULL,
    .readdir    = NULL,
};

static struct pipe_t *pipe_alloc()
{
    struct pipe_t *pipe_ptr = NULL;
    
    pipe_ptr = (struct pipe_t *)kmalloc(sizeof(struct pipe_t), 0);
    spin_init(&pipe_ptr->lock);
    pipe_ptr->read_pos  = 0;
    pipe_ptr->write_pos = 0;
    pipe_ptr->valid_cnt = 0;
    memset(pipe_ptr->buf, 0, MAX_PIPE_BUFF_SIZE);
    wait_queue_init(&pipe_ptr->read_wait_queue, NULL);
    wait_queue_init(&pipe_ptr->write_wait_queue, NULL);

    return pipe_ptr;
}

/**
 * @brief 创建管道
 *
 * @param fd(r8) 文件句柄指针
 * @param num(r9) 文件句柄个数
 * @return uint64_t 
 */
uint64_t sys_pipe(struct pt_regs *regs)
{
    int *fd = NULL;
    struct pipe_t *pipe_ptr = NULL;
    struct vfs_file_t *read_file = NULL;
    struct vfs_file_t *write_file = NULL;

    fd = (int *)regs->r8;
    kdebug("pipe creat into!\n");
    /* step1 申请pipe结构体、初始化 */
    pipe_ptr = pipe_alloc();
    /* step2 申请2个fd文件句柄，1个作为读端、1个作为写端 */
    read_file = (struct vfs_file_t *)kmalloc(sizeof(struct vfs_file_t), 0);
    memset(read_file, 0, sizeof(struct vfs_file_t));
    fd[0] = process_fd_alloc(read_file);
    if (fd[0] == -1) {
        kdebug("pipe alloc read fd fail!\n");
        kfree(pipe_ptr);
        kfree(read_file);
        return -1;
    }
    write_file = (struct vfs_file_t *)kmalloc(sizeof(struct vfs_file_t), 0);
    memset(write_file, 0, sizeof(struct vfs_file_t));
    fd[1] = process_fd_alloc(write_file);
    if (fd[1] == -1) {
        kdebug("pipe alloc write fd fail!\n");
        kfree(pipe_ptr);
        kfree(read_file);
        kfree(write_file);
        return -1;
    }
    /* step3 绑定pipe和file */
    read_file->private_data     = (void *)pipe_ptr;
    read_file->file_ops         = &g_pipe_file_ops;
    read_file->mode             = ATTR_READ_ONLY;
    write_file->private_data    = (void *)pipe_ptr;
    write_file->file_ops        = &g_pipe_file_ops;
    write_file->mode            = O_WRONLY;
    kdebug("pipe creat end!\n");

    return 0;
}
