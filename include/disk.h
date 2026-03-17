/*
 * CougFS: Disk I/O Layer
 *
 * Provides block-level read/write operations on the virtual disk image.
 */

#ifndef DISK_H
#define DISK_H

#include "cougfs.h"

/* Open or create the disk image file. Returns 0 on success, -1 on error. */
int disk_open(const char *path);

/* Close the disk image file. */
void disk_close(void);

/* Read a single block from disk into buf. Returns 0 on success. */
int disk_read_block(uint32_t block_num, void *buf);

/* Write a single block from buf to disk. Returns 0 on success. */
int disk_write_block(uint32_t block_num, const void *buf);

/* Sync all pending writes to disk. */
void disk_sync(void);

/* Return the file descriptor for the disk image, or -1 if not open. */
int disk_fd(void);

#endif /* DISK_H */
