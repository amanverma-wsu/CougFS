/*
 * CougFS: Filesystem Mount/Format Operations
 * WSU CPTS 360
 *
 * Top-level filesystem lifecycle: format, mount, unmount.
 */

#ifndef FS_H
#define FS_H

#include "cougfs.h"

/*
 * Format a fresh filesystem onto the disk image at `path`.
 * Wipes all metadata, initializes the superblock, clears bitmaps,
 * sets up the inode table, and creates the root directory (inode 0).
 * Returns 0 on success, -1 on error.
 */
int fs_format(const char *path);

/*
 * Mount an existing CougFS filesystem from `path`.
 * Verifies the magic number, loads the superblock into memory,
 * initializes bitmaps, and marks the filesystem dirty.
 * Returns 0 on success, -1 on error.
 */
int fs_mount(const char *path);

/*
 * Unmount the currently mounted filesystem.
 * Flushes bitmaps and superblock to disk, marks state clean,
 * and closes the disk image.
 */
void fs_unmount(void);

/*
 * Return a pointer to the in-memory superblock.
 * Valid only while the filesystem is mounted.
 */
cougfs_superblock_t *fs_get_superblock(void);

/*
 * Write the in-memory superblock back to disk (block 0).
 * Called after any operation that changes free block/inode counts.
 * Returns 0 on success, -1 on error.
 */
int fs_sync_superblock(void);

#endif /* FS_H */