#ifdef ENABLE_FUSE

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include "fuse_ops.h"
#include "cougfs.h"
#include "fs.h"
#include "dir.h"
#include "file.h"
#include "inode.h"
#include "disk.h"
#include "concurrency.h"

/* Helper: split path into parent path and filename */
static int split_path(const char *path, char *parent, char *name)
{
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *last_slash = strrchr(tmp, '/');
    if (!last_slash)
        return -1;

    strncpy(name, last_slash + 1, MAX_NAME_LEN);
    name[MAX_NAME_LEN] = '\0';

    if (last_slash == tmp)
        strcpy(parent, "/");
    else {
        *last_slash = '\0';
        strncpy(parent, tmp, 1023);
        parent[1023] = '\0';
    }
    return 0;
}

static void inode_to_stat(const cougfs_inode_t *inode, struct stat *st)
{
    memset(st, 0, sizeof(*st));
    st->st_mode = inode->mode;
    st->st_nlink = inode->link_count;
    st->st_uid = inode->uid;
    st->st_gid = inode->gid;
    st->st_size = inode->size;
    st->st_blocks = inode->blocks * (BLOCK_SIZE / 512);
    st->st_blksize = BLOCK_SIZE;
    st->st_atime = inode->atime;
    st->st_mtime = inode->mtime;
    st->st_ctime = inode->ctime;
}

static int cougfs_getattr(const char *path, struct stat *st)
{
    fs_read_lock();
    int ino = dir_resolve_path(path);
    if (ino < 0) {
        fs_read_unlock();
        return -ENOENT;
    }
    cougfs_inode_t inode;
    if (inode_read(ino, &inode) < 0) {
        fs_read_unlock();
        return -EIO;
    }
    inode_to_stat(&inode, st);
    fs_read_unlock();
    return 0;
}

static int cougfs_readdir(const char *path, void *buf,
                          fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;
    fs_read_lock();
    int dir_ino = dir_resolve_path(path);
    if (dir_ino < 0) {
        fs_read_unlock();
        return -ENOENT;
    }
    cougfs_inode_t dir_inode;
    if (inode_read(dir_ino, &dir_inode) < 0) {
        fs_read_unlock();
        return -EIO;
    }
    uint32_t num_blocks = (dir_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    cougfs_dir_entry_t entries[DIR_ENTRIES_PER_BLOCK];
    for (uint32_t b = 0; b < num_blocks; b++) {
        uint32_t blk = inode_get_block(&dir_inode, b, 0);
        if (blk == INVALID_BLOCK)
            continue;
        if (disk_read_block(blk, entries) < 0)
            continue;
        for (int i = 0; i < DIR_ENTRIES_PER_BLOCK; i++) {
            if (entries[i].inode != INVALID_INODE) {
                if (filler(buf, entries[i].name, NULL, 0) != 0) {
                    fs_read_unlock();
                    return 0;
                }
            }
        }
    }
    fs_read_unlock();
    return 0;
}

static int cougfs_open(const char *path, struct fuse_file_info *fi)
{
    /* file_open updates atime, so we need a write lock */
    fs_write_lock();
    int ino = dir_resolve_path(path);
    if (ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    int flags = COUGFS_O_RDWR;
    if ((fi->flags & O_ACCMODE) == O_RDONLY)
        flags = COUGFS_O_RDONLY;
    else if ((fi->flags & O_ACCMODE) == O_WRONLY)
        flags = COUGFS_O_WRONLY;
    int fd = file_open(ino, flags);
    if (fd < 0) {
        fs_write_unlock();
        return -EISDIR; /* most likely a directory */
    }
    fi->fh = fd;
    fs_write_unlock();
    return 0;
}

static int cougfs_read(const char *path, char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    if (size > UINT32_MAX)
        return -EINVAL;
    /* file_read modifies fd offset and atime, needs write lock */
    fs_write_lock();
    int fd = (int)fi->fh;
    file_seek(fd, (int32_t)offset, 0);
    int ret = file_read(fd, buf, (uint32_t)size);
    fs_write_unlock();
    return ret < 0 ? -EIO : ret;
}

static int cougfs_write(const char *path, const char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi)
{
    (void)path;
    if (size > UINT32_MAX)
        return -EINVAL;
    fs_write_lock();
    int fd = (int)fi->fh;
    file_seek(fd, (int32_t)offset, 0);
    int ret = file_write(fd, buf, (uint32_t)size);
    fs_write_unlock();
    return ret < 0 ? -EIO : ret;
}

static int cougfs_release(const char *path, struct fuse_file_info *fi)
{
    (void)path;
    file_close((int)fi->fh);
    return 0;
}

static int cougfs_create(const char *path, mode_t mode,
                         struct fuse_file_info *fi)
{
    fs_write_lock();
    char parent_path[1024];
    char filename[MAX_NAME_LEN + 1];
    if (split_path(path, parent_path, filename) < 0) {
        fs_write_unlock();
        return -EINVAL;
    }
    int parent_ino = dir_resolve_path(parent_path);
    if (parent_ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    /* Check if file already exists */
    if (dir_lookup(parent_ino, filename) >= 0) {
        fs_write_unlock();
        return -EEXIST;
    }
    int ino = file_create(parent_ino, filename, mode & 0777);
    if (ino < 0) {
        fs_write_unlock();
        return -EIO;
    }
    int fd = file_open(ino, COUGFS_O_RDWR);
    if (fd < 0) {
        /* Clean up orphan file */
        file_delete(parent_ino, filename);
        fs_write_unlock();
        return -EIO;
    }
    fi->fh = fd;
    fs_write_unlock();
    return 0;
}

static int cougfs_unlink(const char *path)
{
    fs_write_lock();
    char parent_path[1024];
    char filename[MAX_NAME_LEN + 1];
    if (split_path(path, parent_path, filename) < 0) {
        fs_write_unlock();
        return -EINVAL;
    }
    int parent_ino = dir_resolve_path(parent_path);
    if (parent_ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    if (dir_lookup(parent_ino, filename) < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    int ret = file_delete(parent_ino, filename);
    fs_write_unlock();
    return ret < 0 ? -EIO : 0;
}

static int cougfs_mkdir(const char *path, mode_t mode)
{
    fs_write_lock();
    char parent_path[1024];
    char dirname[MAX_NAME_LEN + 1];
    if (split_path(path, parent_path, dirname) < 0) {
        fs_write_unlock();
        return -EINVAL;
    }
    int parent_ino = dir_resolve_path(parent_path);
    if (parent_ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    if (dir_lookup(parent_ino, dirname) >= 0) {
        fs_write_unlock();
        return -EEXIST;
    }
    int ret = dir_create(parent_ino, dirname, mode & 0777);
    fs_write_unlock();
    return ret < 0 ? -EIO : 0;
}

static int cougfs_rmdir(const char *path)
{
    fs_write_lock();
    char parent_path[1024];
    char dirname[MAX_NAME_LEN + 1];
    if (split_path(path, parent_path, dirname) < 0) {
        fs_write_unlock();
        return -EINVAL;
    }
    int parent_ino = dir_resolve_path(parent_path);
    if (parent_ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    int ino = dir_lookup(parent_ino, dirname);
    if (ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    if (!dir_is_empty(ino)) {
        fs_write_unlock();
        return -ENOTEMPTY;
    }
    int ret = dir_remove(parent_ino, dirname);
    fs_write_unlock();
    return ret < 0 ? -EIO : 0;
}

static int cougfs_truncate(const char *path, off_t size)
{
    if (size < 0 || (uint64_t)size > UINT32_MAX) {
        return -EINVAL;
    }
    fs_write_lock();
    int ino = dir_resolve_path(path);
    if (ino < 0) {
        fs_write_unlock();
        return -ENOENT;
    }
    int ret = file_truncate(ino, (uint32_t)size);
    fs_write_unlock();
    return ret < 0 ? -EIO : 0;
}

static struct fuse_operations cougfs_ops = {
    .getattr  = cougfs_getattr,
    .readdir  = cougfs_readdir,
    .open     = cougfs_open,
    .read     = cougfs_read,
    .write    = cougfs_write,
    .release  = cougfs_release,
    .create   = cougfs_create,
    .unlink   = cougfs_unlink,
    .mkdir    = cougfs_mkdir,
    .rmdir    = cougfs_rmdir,
    .truncate = cougfs_truncate,
};

int cougfs_fuse_main(int argc, char *argv[], const char *disk_path)
{
    if (fs_mount(disk_path) < 0) {
        fprintf(stderr, "Failed to mount CougFS from %s\n", disk_path);
        return 1;
    }
    int ret = fuse_main(argc, argv, &cougfs_ops, NULL);
    fs_unmount();
    return ret;
}

#else /* !ENABLE_FUSE */

#include <stdio.h>
#include "fuse_ops.h"

int cougfs_fuse_main(int argc, char *argv[], const char *disk_path)
{
    (void)argc;
    (void)argv;
    (void)disk_path;
    fprintf(stderr, "FUSE support not compiled. Rebuild with ENABLE_FUSE=1.\n");
    return 1;
}

#endif /* ENABLE_FUSE */