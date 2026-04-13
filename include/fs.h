/*
 * CougFS: Filesystem Init / Mount
 *
 * High-level functions to format, mount, and unmount the filesystem.
 */

#ifndef FS_H
#define FS_H

#include "cougfs.h"

/* Format a new filesystem on the given disk image path. Returns 0 on success. */
int fs_format(const char *disk_path);

/* Mount an existing filesystem. Returns 0 on success. */
int fs_mount(const char *disk_path);

/* Unmount the filesystem and flush all data. */
void fs_unmount(void);

/* Get the current superblock (read-only). */
const cougfs_superblock_t *fs_get_superblock(void);

/* Update superblock free counts and write to disk. */
int fs_sync_superblock(void);

#endif /* FS_H */
