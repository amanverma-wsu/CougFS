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
    /* TODO: implement */
    (void)fd; (void)buf; (void)count;
    return -1;
}

int file_write(int fd, const void *buf, uint32_t count)
{
    /* TODO: implement */
    (void)fd; (void)buf; (void)count;
    return -1;
}

int file_seek(int fd, int32_t offset, int whence)
{
    /* TODO: implement */
    (void)fd; (void)offset; (void)whence;
    return -1;
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
    /* TODO: implement */
    (void)dir_ino; (void)name;
    return -1;
}

int file_truncate(uint32_t ino, uint32_t new_size)
{
    /* TODO: implement */
    (void)ino; (void)new_size;
    return -1;
}

int file_stat(uint32_t ino, cougfs_inode_t *out)
{
    return inode_read(ino, out);
}