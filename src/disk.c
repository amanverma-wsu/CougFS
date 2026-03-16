#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "disk.h"

static int g_disk_fd = -1;

int disk_open(const char *path)
{
    g_disk_fd = open(path, O_RDWR);
    if (g_disk_fd < 0) {
        g_disk_fd = open(path, O_RDWR | O_CREAT, 0644);
        if (g_disk_fd < 0) {
            perror("disk_open: open");
            return -1;
        }
        if (ftruncate(g_disk_fd, DISK_SIZE) < 0) {
            perror("disk_open: ftruncate");
            close(g_disk_fd);
            g_disk_fd = -1;
            return -1;
        }
    }
    return 0;
}

void disk_close(void)
{
    if (g_disk_fd >= 0) {
        fsync(g_disk_fd);
        close(g_disk_fd);
        g_disk_fd = -1;
    }
}

int disk_read_block(uint32_t block_num, void *buf)
{
    /* TODO: implement block read */
    (void)block_num;
    (void)buf;
    return -1;
}

int disk_write_block(uint32_t block_num, const void *buf)
{
    /* TODO: implement block write */
    (void)block_num;
    (void)buf;
    return -1;
}

void disk_sync(void)
{
    if (g_disk_fd >= 0)
        fsync(g_disk_fd);
}

int disk_fd(void)
{
    return g_disk_fd;
}
