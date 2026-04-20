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
    if (g_disk_fd < 0) {
        fprintf(stderr, "disk_read_block: disk not open\n");
        return -1;
    }
    if (block_num >= DISK_SIZE_BLOCKS) {
        fprintf(stderr, "disk_read_block: block %u out of range\n", block_num);
        return -1;
    }
    off_t offset = (off_t)block_num * BLOCK_SIZE;
    if (lseek(g_disk_fd, offset, SEEK_SET) < 0) {
        perror("disk_read_block: lseek");
        return -1;
    }
    ssize_t total = 0;
    while (total < BLOCK_SIZE) {
        ssize_t n = read(g_disk_fd, (char *)buf + total, BLOCK_SIZE - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("disk_read_block: read");
            return -1;
        }
        if (n == 0) {
            memset((char *)buf + total, 0, BLOCK_SIZE - total);
            break;
        }
        total += n;
    }
    return 0;
}

int disk_write_block(uint32_t block_num, const void *buf)
{
    if (g_disk_fd < 0) {
        fprintf(stderr, "disk_write_block: disk not open\n");
        return -1;
    }
    if (block_num >= DISK_SIZE_BLOCKS) {
        fprintf(stderr, "disk_write_block: block %u out of range\n", block_num);
        return -1;
    }
    off_t offset = (off_t)block_num * BLOCK_SIZE;
    if (lseek(g_disk_fd, offset, SEEK_SET) < 0) {
        perror("disk_write_block: lseek");
        return -1;
    }
    ssize_t total = 0;
    while (total < BLOCK_SIZE) {
        ssize_t n = write(g_disk_fd, (const char *)buf + total, BLOCK_SIZE - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("disk_write_block: write");
            return -1;
        }
        total += n;
    }
    return 0;
}

int disk_zero_and_write_block(uint32_t block_num)
{
    uint8_t zeros[BLOCK_SIZE];
    memset(zeros, 0, BLOCK_SIZE);
    return disk_write_block(block_num, zeros);
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