/*
 * CougFS: Inode Operations
 *
 * Read, write, allocate, and free inodes.
 */

#ifndef INODE_H
#define INODE_H

#include "cougfs.h"

/* Read inode from disk into *inode. Returns 0 on success. */
int inode_read(uint32_t ino, cougfs_inode_t *inode);

/* Write inode to disk. Returns 0 on success. */
int inode_write(uint32_t ino, const cougfs_inode_t *inode);

/* Allocate a new inode with given mode. Returns inode number or -1. */
int inode_alloc(uint16_t mode);

/* Free an inode and all its data blocks. Returns 0 on success. */
int inode_free(uint32_t ino);

/* Get the disk block number for a given logical block of a file.
 * If alloc is nonzero, allocate the block if it doesn't exist.
 * Returns block number or 0 on failure. */
uint32_t inode_get_block(cougfs_inode_t *inode, uint32_t logical_block, int alloc);

/* Truncate a file's data blocks to new_size bytes. Returns 0 on success. */
int inode_truncate(uint32_t ino, cougfs_inode_t *inode, uint32_t new_size);

#endif /* INODE_H */
