#include <stdio.h>
#include <string.h>
#include <time.h>
#include "file.h"
#include "inode.h"
#include "dir.h"
#include "disk.h"
#include "bitmap.h"

static cougfs_fd_t fd_table[MAX_OPEN_FILES];

void file_init(void)
{
    memset(fd_table, 0, sizeof(fd_table));
}

int file_create(uint32_t dir_ino, const char *name, uint16_t mode)
{
    if (name == NULL || name[0] == '\0')
        return -1;
    if (dir_lookup(dir_ino, name) >= 0)
        return -1;
    int ino = inode_alloc(COUGFS_S_IFREG | (mode & 0777));
    if (ino < 0)
        return -1;
    if (dir_add_entry(dir_ino, name, ino, DT_REG) < 0) {
        inode_free(ino);
        return -1;
    }
    return ino;
}

int file_open(uint32_t ino, int flags)
{
    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!fd_table[i].in_use) {
            fd = i;
            break;
        }
    }
    if (fd < 0)
        return -1;
    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0)
        return -1;
    /* Reject opening directories as regular files */
    if ((inode.mode & COUGFS_S_IFMT) != COUGFS_S_IFREG)
        return -1;
    if (flags & COUGFS_O_TRUNC) {
        inode_truncate(ino, &inode, 0);
    }
    fd_table[fd].in_use = 1;
    fd_table[fd].inode = ino;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = flags;
    inode.atime = (uint32_t)time(NULL);
    inode_write(ino, &inode);
    return fd;
}

int file_read(int fd, void *buf, uint32_t count)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use)
        return -1;
    if (buf == NULL)
        return -1;
    /* Enforce open flags: write-only fd cannot read */
    int mode = fd_table[fd].flags & COUGFS_O_RDWR;
    if (mode == COUGFS_O_WRONLY)
        return -1;
    cougfs_inode_t inode;
    if (inode_read(fd_table[fd].inode, &inode) < 0)
        return -1;
    uint32_t offset = fd_table[fd].offset;
    if (offset >= inode.size)
        return 0;
    /* Safe overflow check */
    if (count > inode.size - offset)
        count = inode.size - offset;
    uint32_t bytes_read = 0;
    uint8_t block_buf[BLOCK_SIZE];
    while (bytes_read < count) {
        uint32_t logical_block = offset / BLOCK_SIZE;
        uint32_t block_offset = offset % BLOCK_SIZE;
        uint32_t to_read = BLOCK_SIZE - block_offset;
        if (to_read > count - bytes_read)
            to_read = count - bytes_read;
        uint32_t blk = inode_get_block(&inode, logical_block, 0);
        if (blk == INVALID_BLOCK) {
            memset((char *)buf + bytes_read, 0, to_read);
        } else {
            if (disk_read_block(blk, block_buf) < 0)
                return -1;
            memcpy((char *)buf + bytes_read, block_buf + block_offset, to_read);
        }
        bytes_read += to_read;
        offset += to_read;
    }
    fd_table[fd].offset = offset;
    inode.atime = (uint32_t)time(NULL);
    inode_write(fd_table[fd].inode, &inode);
    return (int)bytes_read;
}

int file_write(int fd, const void *buf, uint32_t count)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use)
        return -1;
    if (buf == NULL)
        return -1;
    /* Enforce open flags: read-only fd cannot write */
    int mode = fd_table[fd].flags & COUGFS_O_RDWR;
    if (mode == COUGFS_O_RDONLY)
        return -1;
    cougfs_inode_t inode;
    if (inode_read(fd_table[fd].inode, &inode) < 0)
        return -1;
    uint32_t offset = fd_table[fd].offset;
    if (offset >= MAX_FILE_SIZE)
        return count == 0 ? 0 : -1;
    if (count > MAX_FILE_SIZE - offset)
        count = MAX_FILE_SIZE - offset;
    uint32_t bytes_written = 0;
    uint8_t block_buf[BLOCK_SIZE];
    while (bytes_written < count) {
        uint32_t logical_block = offset / BLOCK_SIZE;
        uint32_t block_offset = offset % BLOCK_SIZE;
        uint32_t to_write = BLOCK_SIZE - block_offset;
        if (to_write > count - bytes_written)
            to_write = count - bytes_written;
        uint32_t blk = inode_get_block(&inode, logical_block, 1);
        if (blk == INVALID_BLOCK)
            break;
        if (to_write < BLOCK_SIZE) {
            if (disk_read_block(blk, block_buf) < 0)
                return -1;
        }
        memcpy(block_buf + block_offset, (const char *)buf + bytes_written, to_write);
        if (disk_write_block(blk, block_buf) < 0)
            return -1;
        bytes_written += to_write;
        offset += to_write;
    }
    fd_table[fd].offset = offset;
    if (offset > inode.size)
        inode.size = offset;
    inode.mtime = (uint32_t)time(NULL);
    inode_write(fd_table[fd].inode, &inode);
    bitmap_sync();
    return (int)bytes_written;
}

int file_seek(int fd, int32_t offset, int whence)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use)
        return -1;
    cougfs_inode_t inode;
    if (inode_read(fd_table[fd].inode, &inode) < 0)
        return -1;
    int32_t new_offset;
    switch (whence) {
    case 0: new_offset = offset; break;
    case 1: new_offset = (int32_t)fd_table[fd].offset + offset; break;
    case 2: new_offset = (int32_t)inode.size + offset; break;
    default: return -1;
    }
    if (new_offset < 0)
        return -1;
    if ((uint32_t)new_offset > MAX_FILE_SIZE)
        return -1;
    fd_table[fd].offset = (uint32_t)new_offset;
    return (int)fd_table[fd].offset;
}

int file_close(int fd)
{
    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use)
        return -1;
    fd_table[fd].in_use = 0;
    return 0;
}

int file_delete(uint32_t dir_ino, const char *name)
{
    if (name == NULL || name[0] == '\0')
        return -1;
    int ino = dir_lookup(dir_ino, name);
    if (ino < 0)
        return -1;
    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0)
        return -1;
    if (inode.mode & COUGFS_S_IFDIR)
        return -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].in_use && fd_table[i].inode == (uint32_t)ino)
            fd_table[i].in_use = 0;
    }
    if (dir_remove_entry(dir_ino, name) < 0)
        return -1;
    inode.link_count--;
    if (inode.link_count == 0) {
        inode_free(ino);
    } else {
        inode_write(ino, &inode);
    }
    return 0;
}

int file_truncate(uint32_t ino, uint32_t new_size)
{
    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0)
        return -1;
    return inode_truncate(ino, &inode, new_size);
}

int file_stat(uint32_t ino, cougfs_inode_t *out)
{
    if (out == NULL)
        return -1;
    return inode_read(ino, out);
}
